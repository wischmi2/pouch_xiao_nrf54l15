/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>

#include <pouch/transport/gatt/common/types.h>
#include <pouch/transport/gatt/common/uuids.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(scan, CONFIG_POUCH_GATEWAY_GATT_LOG_LEVEL);

#include <pouch/gateway/bt/bond.h>
#include <pouch/gateway/bt/scan.h>

static inline bool version_is_compatible(const struct pouch_gatt_adv_data *adv_data)
{
    uint8_t self_ver =
        adv_data->version & POUCH_GATT_ADV_VERSION_SELF_MASK >> POUCH_GATT_ADV_VERSION_SELF_SHIFT;

    return POUCH_GATT_VERSION == self_ver;
}

static inline bool sync_requested(const struct pouch_gatt_adv_data *adv_data)
{
    return (adv_data->flags & POUCH_GATT_ADV_FLAG_SYNC_REQUEST);
}

struct tf_data
{
    const bt_addr_le_t *addr;
    bool is_tf;
    bool is_bonded;
    struct pouch_gatt_adv_data adv_data;
};

static const struct bt_uuid_128 golioth_svc_uuid_128 =
    BT_UUID_INIT_128(POUCH_GATT_UUID_SVC_VAL_128);
static const struct bt_uuid_16 golioth_svc_uuid_16 = BT_UUID_INIT_16(POUCH_GATT_UUID_SVC_VAL_16);

struct peer_cooldown
{
    bt_addr_le_t addr;
    int64_t until_ms;
};

static struct peer_cooldown peer_cooldowns[CONFIG_BT_MAX_PAIRED];

static struct peer_cooldown *peer_cooldown_find(const bt_addr_le_t *addr)
{
    for (size_t i = 0; i < ARRAY_SIZE(peer_cooldowns); i++)
    {
        if (bt_addr_le_cmp(&peer_cooldowns[i].addr, addr) == 0)
        {
            return &peer_cooldowns[i];
        }
    }

    return NULL;
}

static struct peer_cooldown *peer_cooldown_alloc(const bt_addr_le_t *addr)
{
    struct peer_cooldown *oldest = &peer_cooldowns[0];

    for (size_t i = 0; i < ARRAY_SIZE(peer_cooldowns); i++)
    {
        if (peer_cooldowns[i].until_ms == 0)
        {
            return &peer_cooldowns[i];
        }

        if (peer_cooldowns[i].until_ms < oldest->until_ms)
        {
            oldest = &peer_cooldowns[i];
        }
    }

    return oldest;
}

static bool peer_is_in_cooldown(const bt_addr_le_t *addr, int64_t now_ms, int64_t *remaining_ms)
{
    struct peer_cooldown *cooldown = peer_cooldown_find(addr);

    if (cooldown == NULL || cooldown->until_ms <= now_ms)
    {
        if (cooldown != NULL)
        {
            cooldown->until_ms = 0;
        }
        return false;
    }

    if (remaining_ms != NULL)
    {
        *remaining_ms = cooldown->until_ms - now_ms;
    }

    return true;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
    struct tf_data *tf = user_data;

    switch (data->type)
    {
        case BT_DATA_SVC_DATA128:
        {
            const struct pouch_gatt_adv_data *adv_data;

            if (data->data_len >= sizeof(golioth_svc_uuid_128.val) + sizeof(*adv_data)
                && memcmp(golioth_svc_uuid_128.val, data->data, sizeof(golioth_svc_uuid_128.val))
                    == 0)
            {
                adv_data = (const void *) &data->data[sizeof(golioth_svc_uuid_128.val)];

                tf->is_tf = true;
                tf->adv_data = *adv_data;

                return false;
            }
            return true;
        }

        case BT_DATA_SVC_DATA16:
        {
            const struct pouch_gatt_adv_data *adv_data;

            if (data->data_len >= sizeof(golioth_svc_uuid_16.val) + sizeof(*adv_data)
                && memcmp(&golioth_svc_uuid_16.val, data->data, sizeof(golioth_svc_uuid_16.val))
                    == 0)
            {
                adv_data = (const void *) &data->data[sizeof(golioth_svc_uuid_16.val)];

                tf->is_tf = true;
                tf->adv_data = *adv_data;

                return false;
            }
            return true;
        }

        default:
            return true;
    }
}

static void bond_filter(const struct bt_bond_info *info, void *user_data)
{
    struct tf_data *tf = user_data;

    if (memcmp(&info->addr, tf->addr, sizeof(info->addr)) == 0)
    {
        tf->is_bonded = true;
    }
}

static void device_found(const bt_addr_le_t *addr,
                         int8_t rssi,
                         uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    struct tf_data tf = {
        .addr = addr,
        .is_tf = false,
        /* When filtering bonded devices is disabled, treat all devices as bonded */
        .is_bonded = IS_ENABLED(CONFIG_POUCH_GATEWAY_GATT_SCAN_FILTER_BONDED) ? false : true,
    };
    int err;

    /* We're only interested in connectable events */
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND
        && type != BT_GAP_ADV_TYPE_SCAN_RSP)
    {
        return;
    }

    bt_data_parse(ad, data_cb, &tf);

    if (!tf.is_tf)
    {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    if (IS_ENABLED(CONFIG_POUCH_GATEWAY_GATT_SCAN_FILTER_BONDED))
    {
        bt_foreach_bond(BT_ID_DEFAULT, bond_filter, &tf);

        LOG_DBG("Pouch device found: %s, (RSSI %d) (bonded %d)",
                addr_str,
                rssi,
                (int) tf.is_bonded);
    }
    else
    {
        LOG_DBG("Pouch device found: %s, (RSSI %d)", addr_str, rssi);
    }

    LOG_DBG("version=0x%0x flags=0%0x", tf.adv_data.version, tf.adv_data.flags);

    if (!version_is_compatible(&tf.adv_data))
    {
        return;
    }

    if (!tf.is_bonded && !pouch_gateway_bonding_is_enabled())
    {
        return;
    }

    if (tf.is_bonded && !sync_requested(&tf.adv_data))
    {
        return;
    }

    int64_t remaining_ms;
    if (peer_is_in_cooldown(addr, k_uptime_get(), &remaining_ms))
    {
        LOG_DBG("Skipping %s during security cooldown (%lld ms remaining)",
                addr_str,
                remaining_ms);
        return;
    }

    err = bt_le_scan_stop();
    if (err)
    {
        LOG_ERR("Failed to stop scanning");
        return;
    }

    struct bt_conn *conn = NULL;
    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
    if (err)
    {
        LOG_ERR("Create auto conn failed (%d)", err);
        pouch_gateway_scan_start();
        return;
    }

    /* Disable bonding after first connect attempt */
    if (!tf.is_bonded)
    {
        pouch_gateway_bonding_disable();
    }
}

void pouch_gateway_scan_start(void)
{
    int err;

    err = bt_le_scan_start(BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_ACTIVE,
                                            BT_LE_SCAN_OPT_NONE,
                                            BT_GAP_SCAN_FAST_INTERVAL_MIN,
                                            BT_GAP_SCAN_FAST_WINDOW),
                           device_found);
    if (err)
    {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return;
    }

    LOG_INF("Scanning successfully started");
}

void pouch_gateway_scan_peer_cooldown(const bt_addr_le_t *addr)
{
    struct peer_cooldown *cooldown = peer_cooldown_find(addr);
    char addr_str[BT_ADDR_LE_STR_LEN];

    if (cooldown == NULL)
    {
        cooldown = peer_cooldown_alloc(addr);
        bt_addr_le_copy(&cooldown->addr, addr);
    }

    cooldown->until_ms =
        k_uptime_get() + (CONFIG_POUCH_GATEWAY_GATT_SECURITY_FAILURE_COOLDOWN_S * MSEC_PER_SEC);

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    LOG_WRN("Cooling down %s after BLE security failure for %d seconds",
            addr_str,
            CONFIG_POUCH_GATEWAY_GATT_SECURITY_FAILURE_COOLDOWN_S);
}

void pouch_gateway_scan_peer_cooldown_clear(const bt_addr_le_t *addr)
{
    struct peer_cooldown *cooldown = peer_cooldown_find(addr);

    if (cooldown != NULL)
    {
        cooldown->until_ms = 0;
    }
}

/*
 * Copyright (c) 2026 Golioth
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(example_ble_peripheral);

#include "ble_peripheral.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>

#include <pouch/transport/gatt/common/types.h>
#include <pouch/types.h>

static struct bt_conn *default_conn;

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr))
static const struct device *const rfsw_pwr_dev = DEVICE_DT_GET(DT_NODELABEL(rfsw_pwr));
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
static const struct device *const rfsw_ctl_dev = DEVICE_DT_GET(DT_NODELABEL(rfsw_ctl));
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr)) || DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
static bool antenna_external;
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr)) || DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
static int rf_antenna_apply(bool external)
{
    int err = 0;

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr))
    if (!device_is_ready(rfsw_pwr_dev))
    {
        LOG_WRN("rfsw_pwr not ready");
        return -ENODEV;
    }

    err = regulator_enable(rfsw_pwr_dev);
    if (err && err != -EALREADY)
    {
        LOG_ERR("rfsw_pwr enable failed (%d)", err);
        return err;
    }
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
    if (!device_is_ready(rfsw_ctl_dev))
    {
        LOG_WRN("rfsw_ctl not ready");
        return -ENODEV;
    }

    if (external)
    {
        err = regulator_disable(rfsw_ctl_dev);
    }
    else
    {
        err = regulator_enable(rfsw_ctl_dev);
    }

    if (err && err != -EALREADY && err != -EBUSY)
    {
        LOG_ERR("rfsw_ctl %s failed (%d)", external ? "disable" : "enable", err);
        return err;
    }
#endif

    antenna_external = external;
    LOG_INF("Antenna: %s", external ? "external (IPEX)" : "internal (ceramic)");
    return 0;
}
#endif

int ble_peripheral_antenna_set(bool external)
{
#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr)) || DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
    return rf_antenna_apply(external);
#else
    ARG_UNUSED(external);
    return -ENOTSUP;
#endif
}

bool ble_peripheral_antenna_is_external(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr)) || DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
    return antenna_external;
#else
    return false;
#endif
}

#if IS_ENABLED(CONFIG_EXAMPLE_BATTERY_POWER_LED)
static const struct gpio_dt_spec connect_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {});

static void connect_led_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (!gpio_is_ready_dt(&connect_led))
    {
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        gpio_pin_set_dt(&connect_led, 1);
        k_msleep(500);
        gpio_pin_set_dt(&connect_led, 0);
        k_msleep(200);
    }
}

K_WORK_DEFINE(connect_led_work, connect_led_work_handler);
#endif

static struct pouch_gatt_adv service_data = POUCH_GATT_ADV_DATA_INIT;

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_SVC_DATA16, &service_data, sizeof(service_data)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void sync_request_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(sync_request_work, sync_request_work_handler);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_DBG("Connection failed (err 0x%02x)", err);
        return;
    }

    LOG_DBG("Connected");
    default_conn = conn;

    /* Stop asking for gateway while connected (adv may resume with stale flags). */
    k_work_cancel_delayable(&sync_request_work);
    ble_peripheral_request_gateway(false);

#if IS_ENABLED(CONFIG_EXAMPLE_BATTERY_POWER_LED)
    k_work_submit(&connect_led_work);
#endif
}

static void sync_request_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    LOG_DBG("Requesting Gateway sync");
    ble_peripheral_request_gateway(true);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_DBG("Disconnected (reason 0x%02x)", reason);

    default_conn = NULL;

    /*
     * Clear sync-request immediately. The stack often resumes advertising with the
     * previous payload before resume_work runs; bonded gateways then reconnect in
     * milliseconds instead of waiting CONFIG_EXAMPLE_SYNC_PERIOD_S.
     */
    k_work_cancel_delayable(&sync_request_work);
    ble_peripheral_request_gateway(false);

    k_work_schedule(&sync_request_work, K_SECONDS(CONFIG_EXAMPLE_SYNC_PERIOD_S));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    char passkey_str[7];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    (void) snprintf(passkey_str, sizeof(passkey_str), "%06u", passkey);

    LOG_INF("Passkey for %s: %s", addr, passkey_str);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    char passkey_str[7];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    (void) snprintf(passkey_str, sizeof(passkey_str), "%06u", passkey);

    LOG_INF("Confirm passkey for %s: %s", addr, passkey_str);

    if (IS_ENABLED(CONFIG_EXAMPLE_BT_AUTO_CONFIRM))
    {
        LOG_INF("Confirming passkey");
        bt_conn_auth_passkey_confirm(conn);
    }
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .passkey_display = auth_passkey_display,
    .passkey_confirm = auth_passkey_confirm,
    .cancel = auth_cancel,
};

void ble_peripheral_request_gateway(bool request)
{
    int err;

    pouch_gatt_adv_req_sync(&service_data, request);
    err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
    if (err == -EAGAIN)
    {
        /* Not advertising yet; start with the requested sync flag. */
        (void) ble_peripheral_start(request);
    }
}

void ble_peripheral_button_handler(void)
{
    if (default_conn)
    {
        LOG_INF("Confirming passkey");
        bt_conn_auth_passkey_confirm(default_conn);
    }
    else
    {
        LOG_WRN("No BT connection for passkey confirmation");
    }
}

int ble_peripheral_init(void)
{
    int err;

#if DT_NODE_EXISTS(DT_NODELABEL(rfsw_pwr)) || DT_NODE_EXISTS(DT_NODELABEL(rfsw_ctl))
    err = rf_antenna_apply(IS_ENABLED(CONFIG_EXAMPLE_ANTENNA_EXTERNAL));
    if (err)
    {
        LOG_WRN("RF antenna init failed (%d); continuing with BLE", err);
    }
#endif

    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    err = bt_conn_auth_cb_register(&auth_cb_display);
    if (err)
    {
        LOG_ERR("Bluetooth auth cb register failed (err %d)", err);
        return err;
    }

    return 0;
}

int ble_peripheral_start(bool request_sync)
{
    pouch_gatt_adv_req_sync(&service_data, request_sync);
    return bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), NULL, 0);
}

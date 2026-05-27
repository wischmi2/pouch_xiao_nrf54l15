/*
 * Copyright (c) 2026 Golioth
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "battery.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <pouch/types.h>
#include <pouch/uplink.h>

LOG_MODULE_REGISTER(battery, LOG_LEVEL_INF);

#define BATTERY_USER_NODE DT_PATH(zephyr_user)
#define BATTERY_ADC_IDX 1

#define BATTERY_EMPTY_MV 3000
#define BATTERY_FULL_MV 4200

#define BATTERY_REGULATOR_SETTLE_MS 100
#define BATTERY_ADC_DIVIDER 2

#if DT_NODE_EXISTS(DT_NODELABEL(vbat_pwr)) && \
    DT_NODE_HAS_PROP(BATTERY_USER_NODE, io_channels) && \
    (DT_PROP_LEN(BATTERY_USER_NODE, io_channels) > BATTERY_ADC_IDX)

static const struct device *const vbat_reg = DEVICE_DT_GET(DT_NODELABEL(vbat_pwr));
static const struct adc_dt_spec battery_adc =
    ADC_DT_SPEC_GET_BY_IDX(BATTERY_USER_NODE, BATTERY_ADC_IDX);
static bool battery_ready;

static int battery_mv_to_percent(int32_t battery_mv)
{
    const int32_t range = BATTERY_FULL_MV - BATTERY_EMPTY_MV;

    if (battery_mv <= BATTERY_EMPTY_MV)
    {
        return 0;
    }

    if (battery_mv >= BATTERY_FULL_MV)
    {
        return 100;
    }

    return (int) ((battery_mv - BATTERY_EMPTY_MV) * 100 / range);
}

static int read_battery(int32_t *battery_mv, int *percent)
{
    struct adc_sequence sequence = {0};
    int16_t sample;
    int32_t sample_mv;
    int err;

    if (!battery_ready)
    {
        return -ENODEV;
    }

    if (!device_is_ready(vbat_reg))
    {
        LOG_WRN("Battery regulator device is not ready");
        return -ENODEV;
    }

    err = regulator_enable(vbat_reg);
    if (err)
    {
        LOG_WRN("Battery regulator enable failed (err %d)", err);
        return err;
    }

    k_sleep(K_MSEC(BATTERY_REGULATOR_SETTLE_MS));

    err = adc_channel_setup_dt(&battery_adc);
    if (err)
    {
        LOG_WRN("Battery ADC channel setup failed (err %d)", err);
        goto disable_regulator;
    }

    err = adc_sequence_init_dt(&battery_adc, &sequence);
    if (err)
    {
        LOG_WRN("Battery ADC sequence init failed (err %d)", err);
        goto disable_regulator;
    }

    sequence.buffer = &sample;
    sequence.buffer_size = sizeof(sample);

    err = adc_read_dt(&battery_adc, &sequence);
    if (err)
    {
        LOG_WRN("Battery ADC read failed (err %d)", err);
        goto disable_regulator;
    }

    sample_mv = sample;
    err = adc_raw_to_millivolts_dt(&battery_adc, &sample_mv);
    if (err)
    {
        LOG_WRN("Battery ADC millivolt conversion failed (err %d)", err);
        goto disable_regulator;
    }

    *battery_mv = sample_mv * BATTERY_ADC_DIVIDER;
    *percent = battery_mv_to_percent(*battery_mv);
    LOG_INF("Battery sample: divider_mv %d, battery_mv %d, percent %d",
            sample_mv,
            *battery_mv,
            *percent);
    err = 0;

disable_regulator:
    (void) regulator_disable(vbat_reg);
    return err;
}

int setup_battery(void)
{
    if (!device_is_ready(vbat_reg))
    {
        LOG_WRN("Battery regulator is not ready");
        return -ENODEV;
    }

    if (!adc_is_ready_dt(&battery_adc))
    {
        LOG_WRN("Battery ADC device is not ready");
        return -ENODEV;
    }

    battery_ready = true;
    LOG_INF("Battery ADC and regulator configured");
    return 0;
}

void build_battery_payload(char *data, size_t data_len)
{
    int32_t battery_mv;
    int percent;
    int err = read_battery(&battery_mv, &percent);

    if (err)
    {
        LOG_WRN("Battery read failed (err %d)", err);
        snprintf(data, data_len, "{\"battery_error\":%d}", err);
    }
    else
    {
        snprintf(data,
                 data_len,
                 "{\"battery_mv\":%d,\"battery_percent\":%d}",
                 (int) battery_mv,
                 percent);
    }
}

void write_battery_uplink(const char *data)
{
    int err;
    size_t data_len = strlen(data);

    LOG_INF("Writing battery uplink: path .s/battery, content_type %d, len %zu",
            POUCH_CONTENT_TYPE_JSON,
            data_len);

    err = pouch_uplink_entry_write(".s/battery",
                                   POUCH_CONTENT_TYPE_JSON,
                                   data,
                                   data_len,
                                   POUCH_FOREVER);
    if (err)
    {
        LOG_WRN("Battery uplink write failed (err %d)", err);
    }
}

#else

int setup_battery(void)
{
    LOG_WRN("Battery measurement not configured in devicetree");
    return -ENODEV;
}

void build_battery_payload(char *data, size_t data_len)
{
    snprintf(data, data_len, "{\"battery_error\":%d}", -ENODEV);
}

void write_battery_uplink(const char *data)
{
    ARG_UNUSED(data);
}

#endif

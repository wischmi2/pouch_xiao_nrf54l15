/*
 * Copyright (c) 2025 Golioth
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include "credentials.h"
#include "ble_peripheral.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include <pouch/pouch.h>
#include <pouch/events.h>
#include <pouch/uplink.h>
#include <pouch/downlink.h>
#include <pouch/transport/gatt/common/types.h>

#include <pouch/golioth/settings_callbacks.h>

#include <app_version.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {});
static struct gpio_callback button_cb_data;
static bool led_ready;
static struct k_work manual_uplink_work;
static struct k_mutex manual_uplink_lock;
static char manual_uplink_payload[96];
static bool manual_uplink_pending;

#define SOIL_SENSOR_NODE DT_PATH(zephyr_user)

#if DT_NODE_HAS_PROP(SOIL_SENSOR_NODE, io_channels)
static const struct adc_dt_spec soil_sensor_adc =
    ADC_DT_SPEC_GET_BY_IDX(SOIL_SENSOR_NODE, 0);
static bool soil_sensor_ready;

static int setup_soil_sensor(void)
{
    if (!adc_is_ready_dt(&soil_sensor_adc))
    {
        LOG_WRN("Soil sensor ADC device is not ready");
        return -ENODEV;
    }

    int err = adc_channel_setup_dt(&soil_sensor_adc);
    if (err)
    {
        LOG_WRN("Could not configure soil sensor ADC channel (err %d)", err);
        return err;
    }

    soil_sensor_ready = true;
    LOG_INF("Soil sensor ADC channel configured");
    return 0;
}

static int read_soil_sensor(int16_t *raw, int32_t *millivolts)
{
    if (!soil_sensor_ready)
    {
        return -ENODEV;
    }

    struct adc_sequence sequence = {0};
    int16_t sample;
    int err = adc_sequence_init_dt(&soil_sensor_adc, &sequence);
    if (err)
    {
        LOG_WRN("ADC sequence init failed (err %d)", err);
        return err;
    }

    sequence.buffer = &sample;
    sequence.buffer_size = sizeof(sample);

    LOG_INF("Reading soil sensor ADC: device %s, channel %u, resolution %u, buffer_size %zu",
            soil_sensor_adc.dev->name,
            soil_sensor_adc.channel_id,
            sequence.resolution,
            sequence.buffer_size);

    err = adc_read_dt(&soil_sensor_adc, &sequence);
    if (err)
    {
        LOG_WRN("ADC read failed: err %d, sequence buffer_size %zu, channels 0x%x",
                err,
                sequence.buffer_size,
                (unsigned int) sequence.channels);
        return err;
    }

    int32_t sample_mv = sample;
    err = adc_raw_to_millivolts_dt(&soil_sensor_adc, &sample_mv);
    if (err)
    {
        LOG_WRN("ADC millivolt conversion failed: err %d, raw %d", err, sample);
        return err;
    }

    *raw = sample;
    *millivolts = sample_mv;
    LOG_INF("Soil sensor ADC sample: raw %d, millivolts %d", sample, sample_mv);
    return 0;
}
#else
static int setup_soil_sensor(void)
{
    LOG_WRN("No soil sensor ADC channel configured");
    return -ENODEV;
}

static int read_soil_sensor(int16_t *raw, int32_t *millivolts)
{
    ARG_UNUSED(raw);
    ARG_UNUSED(millivolts);
    return -ENODEV;
}
#endif

/**
 * Build the JSON payload for one soil sensor reading.
 */
static void build_soil_sensor_payload(char *data, size_t data_len)
{
    int16_t raw;
    int32_t millivolts;
    int err = read_soil_sensor(&raw, &millivolts);

    if (err)
    {
        LOG_WRN("Soil sensor read failed (err %d)", err);
        snprintf(data, data_len, "{\"soil_sensor_error\":%d}", err);
    }
    else
    {
        snprintf(data,
                 data_len,
                 "{\"soil_moisture_mv\":%d,\"soil_moisture_raw\":%d}",
                 millivolts,
                 raw);
    }
}

static void write_soil_sensor_uplink(const char *data)
{
    int err;

    LOG_INF("Writing soil sensor uplink: path .s/sensor, content_type %d, payload %s",
            POUCH_CONTENT_TYPE_JSON,
            data);

    err = pouch_uplink_entry_write(".s/sensor",
                                   POUCH_CONTENT_TYPE_JSON,
                                   data,
                                   strlen(data),
                                   POUCH_FOREVER);
    if (err)
    {
        LOG_WRN("Soil sensor uplink write failed (err %d)", err);
    }
}

/**
 * Push timeseries application data to the cloud on every uplink.
 */
static void do_uplink(void)
{
    char data[96];
    bool use_manual_payload = false;

    k_mutex_lock(&manual_uplink_lock, K_FOREVER);
    if (manual_uplink_pending)
    {
        strncpy(data, manual_uplink_payload, sizeof(data));
        data[sizeof(data) - 1] = '\0';
        manual_uplink_pending = false;
        use_manual_payload = true;
    }
    k_mutex_unlock(&manual_uplink_lock);

    if (use_manual_payload)
    {
        LOG_INF("Sending button-triggered soil sensor reading");
    }
    else
    {
        build_soil_sensor_payload(data, sizeof(data));
    }

    write_soil_sensor_uplink(data);
}

POUCH_UPLINK_HANDLER(do_uplink);

/**
 * Settings handler for the "LED" setting.
 *
 * The settings handler gets called when the Settings service
 * receives a new value for the registered setting.
 */
static int led_setting_cb(bool new_value)
{
    LOG_INF("Received LED setting: %d", (int) new_value);

    if (led_ready)
    {
        gpio_pin_set_dt(&led, new_value ? 1 : 0);
    }

    return 0;
}

GOLIOTH_SETTINGS_HANDLER(LED, led_setting_cb);

/**
 * Setup pouch stack.
 *
 * Loads credentials and initializes pouch.
 */
static int setup_pouch(void)
{
    struct pouch_config config = {0};

    int err = load_certificate(&config.certificate);
    if (err)
    {
        LOG_ERR("Failed to load certificate (err %d)", err);
        return err;
    }

    config.private_key = load_private_key();
    if (config.private_key == PSA_KEY_ID_NULL)
    {
        LOG_ERR("Failed to load private key");
        return -ENOENT;
    }

    LOG_INF("Credentials loaded");

    err = pouch_init(&config);
    if (err)
    {
        LOG_ERR("Pouch init failed (err %d)", err);
        return err;
    }

    LOG_INF("Pouch initialized");
    return 0;
}

/**
 * Initialize LED.
 *
 * If the LED can't be initialized, we'll log it, but not crash out.
 * The example can still work without it.
 */
static void setup_led(void)
{
    if (!DT_HAS_ALIAS(led0))
    {
        return;
    }

    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err < 0)
    {
        LOG_WRN("Could not initialize LED");
        return;
    }

    led_ready = true;
}

static void blink_stage(uint8_t count)
{
    if (!led_ready)
    {
        return;
    }

    LOG_INF("LED stage %u", count);

    for (uint8_t i = 0; i < count; i++)
    {
        gpio_pin_set_dt(&led, 1);
        k_msleep(150);
        gpio_pin_set_dt(&led, 0);
        k_msleep(150);
    }

    k_msleep(500);
}

static void manual_uplink_work_handler(struct k_work *work)
{
    char data[sizeof(manual_uplink_payload)];

    ARG_UNUSED(work);

    build_soil_sensor_payload(data, sizeof(data));

    k_mutex_lock(&manual_uplink_lock, K_FOREVER);
    strncpy(manual_uplink_payload, data, sizeof(manual_uplink_payload));
    manual_uplink_payload[sizeof(manual_uplink_payload) - 1] = '\0';
    manual_uplink_pending = true;
    k_mutex_unlock(&manual_uplink_lock);

    LOG_INF("Button-triggered soil sensor reading queued: %s", data);
    ble_peripheral_request_gateway(true);
}

/**
 * Button handler for Bluetooth OOB authentication and manual sensor uplinks.
 */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    ble_peripheral_button_handler();
    k_work_submit(&manual_uplink_work);
}

/**
 * Initialize button.
 *
 * If the button can't be initialized, we'll log it, but not crash out.
 * The example can still work without it.
 */
static void setup_button(void)
{
    if (!DT_HAS_ALIAS(sw0))
    {
        return;
    }

    LOG_INF("Set up button at %s pin %d", button.port->name, button.pin);

    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err < 0)
    {
        LOG_WRN("Could not initialize Button");
        return;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err)
    {
        LOG_WRN("Error %d: failed to configure interrupt on %s pin %d",
                err,
                button.port->name,
                button.pin);
        return;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);
}

int main(void)
{
    LOG_INF("Pouch SDK Version: " STRINGIFY(APP_BUILD_VERSION));
    LOG_INF("Pouch Protocol Version: %d", POUCH_VERSION);
    LOG_INF("Pouch BLE Transport Protocol Version: %d", POUCH_GATT_VERSION);

    k_mutex_init(&manual_uplink_lock);
    k_work_init(&manual_uplink_work, manual_uplink_work_handler);

    setup_led();
    blink_stage(1);

    int err = ble_peripheral_init();
    if (err)
    {
        return err;
    }
    blink_stage(2);

    err = setup_pouch();
    if (err)
    {
        return err;
    }
    blink_stage(3);

    setup_button();
    setup_soil_sensor();
    blink_stage(4);

    err = ble_peripheral_start();
    if (err)
    {
        return err;
    }

    LOG_INF("Advertising started");

    // Request a gateway right away:
    ble_peripheral_request_gateway(true);
    blink_stage(5);

    return 0;
}

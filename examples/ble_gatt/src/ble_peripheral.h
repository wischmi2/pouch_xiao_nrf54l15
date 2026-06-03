/*
 * Copyright (c) 2026 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Button handler for Bluetooth OOB authentication.
 */
void ble_peripheral_button_handler(void);

/**
 * Enable or disable the gateway request flag in the Bluetooth advertisements.
 */
void ble_peripheral_request_gateway(bool request);

/**
 * Initialize application Bluetooth module.
 */
int ble_peripheral_init(void);

/**
 * Start Bluetooth advertising.
 *
 * @param request_sync If true, set POUCH_GATT_ADV_FLAG_SYNC_REQUEST so the gateway connects.
 */
int ble_peripheral_start(bool request_sync);

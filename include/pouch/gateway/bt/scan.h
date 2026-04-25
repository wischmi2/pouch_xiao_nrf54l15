/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/bluetooth/addr.h>

/**
 * Start Bluetooth scanning for devices.
 *
 * Start active Bluetooth scanning for devices that expose Pouch Service UUID (0xFC49 or
 * 89a316ae-89b7-4ef6-b1d3-5c9a6e27d272 for backward compatibility) with vendor data indicating:
 * - compatible 'version'
 * - sync request set in 'flags'
 */
void pouch_gateway_scan_start(void);

/**
 * Temporarily skip reconnecting to a peer after a security failure.
 *
 * @param addr Peer Bluetooth address.
 */
void pouch_gateway_scan_peer_cooldown(const bt_addr_le_t *addr);

/**
 * Clear any active scan cooldown for a peer after a successful sync/security setup.
 *
 * @param addr Peer Bluetooth address.
 */
void pouch_gateway_scan_peer_cooldown_clear(const bt_addr_le_t *addr);

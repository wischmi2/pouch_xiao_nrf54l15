/*
 * Copyright (c) 2026 Golioth
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

/** Initialize battery measurement hardware (XIAO vbat_pwr + ADC). */
int setup_battery(void);

/** Build JSON for a battery reading into @p data (up to @p data_len bytes). */
void build_battery_payload(char *data, size_t data_len);

/** Write the battery payload to the Pouch uplink at `.s/battery`. */
void write_battery_uplink(const char *data);

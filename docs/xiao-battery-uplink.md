# XIAO nRF54L15 battery uplink (ble_gatt)

This document describes the battery voltage feature on branch `feature/xiao-battery-uplink`.
The known-good soil-sensor baseline remains on `soil_sensor`.

## Overview

The XIAO nRF54L15 routes LiPo voltage through a TPS22916 load switch (`vbat_pwr`) and a
divider to SAADC channel 7 (AIN7). The firmware:

1. Enables `vbat_pwr` only during measurement (low standby drain).
2. Reads ADC channel 7, converts to millivolts, and multiplies by 2 for cell voltage.
3. Estimates charge percent with a simple 3.0 V–4.2 V LiPo linear map.
4. Uplinks JSON to Golioth at path `.s/battery` on every gateway sync.

Soil moisture continues to use `.s/sensor` unchanged.

Reference: [Seeed XIAO nRF54L15 battery section](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/#battery-powered-board)

## Branch

```powershell
cd C:\Users\Brian\pouch_xiao_nrf54l15
git checkout feature/xiao-battery-uplink
```

To return to the soil-only firmware:

```powershell
git checkout soil_sensor
```

## Devicetree and Kconfig

**Board overlay** ([`examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay`](../examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay)):

- `zephyr,user` io-channels for soil (ch0) and battery (ch7).
- `&adc { status = "okay"; }`

**Board DTS requirement:** `vbat_pwr` and SAADC channel 7 must exist in the Zephyr board files (included in upstream/NCS 3.2.3+ `xiao_nrf54l15`). The overlay does not duplicate those nodes.

```dts
zephyr,user {
    io-channels = <&adc 0>, <&adc 7>;
};
```

- Index 0: soil sensor (SAADC channel 0).
- Index 1: battery (SAADC channel 7).

**`prj.conf` additions:**

- `CONFIG_REGULATOR=y` — `vbat_pwr` load switch.
- `CONFIG_PM_DEVICE=y` — regulator power management.

## Build and flash

From the NCS workspace (adjust paths if your layout differs):

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
west flash
```

Use `--pristine` after switching branches so `app_version.h` matches the built commit.

Or use the helper script from the pouch repo root:

```powershell
.\scripts\build_ble_gatt_v010.ps1 -SkipWestUpdate
```

## Expected serial output

**Boot:**

```text
<inf> main: Soil sensor ADC channel configured
<inf> battery: Battery ADC and regulator configured
```

**Each gateway uplink:**

```text
<inf> battery: Battery sample: divider_mv ..., battery_mv ..., percent ...
<inf> battery: Writing battery uplink: path .s/battery, content_type 50, len ...
```

Soil lines (`.s/sensor`, button queue) should behave as on `soil_sensor`.

## Golioth payload

**Path:** `.s/battery`

**Success example:**

```json
{"battery_mv":3950,"battery_percent":79}
```

**Error example:**

```json
{"battery_error":-19}
```

View in the Golioth console under the device stream/lightDB path for `.s/battery`.

## Battery-only power (optional)

If the board fails to boot on LiPo with USB serial enabled, Seeed recommends disabling UART
in the default `prj.conf` and using a `prj_uart.conf` overlay only for USB bench debugging.
See the wiki “Scenario A / Scenario B” under battery-powered board. USB bench testing for
this feature does not require that split.

## Files touched

| File | Role |
|------|------|
| `examples/ble_gatt/src/battery.c` | Regulator + ADC read, JSON, uplink write |
| `examples/ble_gatt/src/battery.h` | Public API |
| `examples/ble_gatt/src/main.c` | `setup_battery()`, battery in `do_uplink()` |
| `examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay` | ADC ch0 + ch7 |
| `examples/ble_gatt/prj.conf` | Regulator / PM |

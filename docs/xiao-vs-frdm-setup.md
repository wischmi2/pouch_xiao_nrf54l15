# XIAO Device vs FRDM-RW612 Gateway Setup

This project uses two pieces of hardware with different roles.

## XIAO nRF54L15: Pouch Device

The XIAO runs the BLE GATT peripheral application:

```text
pouch/examples/ble_gatt
```

Responsibilities:

- Samples the capacitive soil sensor connected to XIAO `A0`.
- Sends the soil sensor reading through Pouch uplink data.
- Stores its device certificate and private key in LittleFS.
- Advertises the Pouch BLE GATT service so the gateway can connect.

Build target:

```text
xiao_nrf54l15/nrf54l15/cpuapp
```

Build command:

```powershell
west config manifest.file west-ncs.yml
west update
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

Flash command used:

```powershell
pyocd flash -t nrf54l C:/ncs_pouch_soil/pouch/examples/ble_gatt/build/zephyr/zephyr.hex
pyocd reset -t nrf54l
```

Credentials expected on the XIAO:

```text
/lfs1/credentials/crt.der
/lfs1/credentials/key.der
```

Upload over the XIAO USB serial port, for example `COM8`:

```powershell
smpmgr --port COM8 --mtu 128 file upload C:/path/to/device.crt.der /lfs1/credentials/crt.der
smpmgr --port COM8 --mtu 128 file upload C:/path/to/device.key.der /lfs1/credentials/key.der
```

Notes:

- The XIAO app currently builds with `--no-sysbuild`.
- MCUboot/sysbuild currently fails for this board because MCUboot cannot determine the flash device metadata.
- Direct flashing is enough for app bring-up, but MCUboot must be fixed later for signed DFU/OTA flows.

## FRDM-RW612: Pouch Gateway

The FRDM-RW612 runs the gateway application:

```text
pouch/examples/gateway
```

Responsibilities:

- Connects to WiFi.
- Connects to Golioth using gateway credentials.
- Scans for Pouch BLE devices.
- Connects to the XIAO and performs Pouch synchronization.
- Forwards the XIAO uplink data to Golioth.

Build target:

```text
frdm_rw612
```

Build command:

```powershell
west config manifest.file west-zephyr.yml
west update
west patch apply
west blobs fetch -a hal_nxp

west build -p always -b frdm_rw612 pouch/examples/gateway `
  --build-dir C:/ncs_pouch_soil/build-gateway-frdm_rw612 `
  -- -DEXTRA_CONF_FILE=boards/frdm_rw612_wifi.conf
```

Flash command used:

```powershell
west flash --build-dir C:/ncs_pouch_soil/build-gateway-frdm_rw612 --dev-id 1060877759
```

Provision Golioth PSK credentials on the FRDM gateway shell:

```text
settings set golioth/psk-id <psk-id>
settings set golioth/psk <psk>
kernel reboot
```

Provision WiFi credentials on the FRDM gateway shell:

```text
wifi cred add -s <your-wifi-ssid> -p <your-wifi-password> -k 1
wifi cred auto_connect
```

Notes:

- The gateway firmware must come from the same Pouch protocol version as the XIAO firmware.
- A gateway built from an older Pouch revision may connect over BLE but fail during certificate exchange.
- `frdm_rw612` uses `west-zephyr.yml` because it needs Zephyr/NXP HAL support that is not included by `west-ncs.yml`.

## Quick Role Summary

| Item | XIAO nRF54L15 | FRDM-RW612 |
|---|---|---|
| Role | Pouch BLE device | Pouch gateway |
| App | `examples/ble_gatt` | `examples/gateway` |
| Board | `xiao_nrf54l15/nrf54l15/cpuapp` | `frdm_rw612` |
| Manifest | `west-ncs.yml` | `west-zephyr.yml` |
| Network | BLE only | WiFi + BLE |
| Golioth credentials | Device cert/key in LittleFS | Gateway PSK in settings |
| Soil sensor | A0 ADC input | None |

# Pouch Node Compiled CA Trust Anchor

This note explains how to get the Pouch server CA certificate compiled into the
XIAO nRF54L15 Pouch node firmware.

The node uses two different kinds of certificates:

- Device identity credentials: uploaded to LittleFS at runtime as
  `/lfs1/credentials/crt.der` and `/lfs1/credentials/key.der`.
- Server CA trust anchor: compiled into the firmware image at build time and
  used to verify the Pouch server certificate sent by the gateway.

If the node logs this error, the runtime device cert/key may be present, but the
compiled server CA is wrong or missing:

```text
<err> cert: Failed verifying server cert: 0x2700, 8
<err> saead_session: Missing server key
<err> saead_uplink: Session key generation failed
```

Flag `8` means the received server certificate is not trusted by the CA that was
compiled into the node firmware.

## 1. Get the Correct CA Certificate

For the production gateway that connects to `coap.golioth.io`, the node needs
the production Golioth/Pouch server CA certificate in DER format.

The default firmware setting expects this file:

```text
C:/ncs_pouch_soil/pouch/src/goliothrootx1.der
```

Do not confuse this with the node device certificate and key in `certs/`:

```text
C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.crt.der
C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.key.der
```

Those two files identify the node. They are still uploaded to LittleFS, but they
are not the CA trust anchor used to verify the server cert.

## 2. Place the CA File in the Default Path

Copy the Golioth Root X1 DER file to:

```text
C:/ncs_pouch_soil/pouch/src/goliothrootx1.der
```

The relevant Kconfig default is:

```text
CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"
```

During the Zephyr build, `port/zephyr/CMakeLists.txt` finds that file, generates
`pouch_ca_cert.inc`, and links the bytes into the node firmware.

## 3. Rebuild the XIAO Node Firmware

From PowerShell:

```powershell
cd C:/ncs_pouch_soil/pouch
west config manifest.file west-ncs.yml
west update
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

The build helper builds `examples/ble_gatt` for:

```text
xiao_nrf54l15/nrf54l15/cpuapp
```

It uses `--no-sysbuild`, which is the current working path for this board.

## 4. Flash the XIAO Node

With the XIAO debugger connected:

```powershell
pyocd flash -t nrf54l C:/ncs_pouch_soil/pouch/examples/ble_gatt/build/zephyr/zephyr.hex
pyocd reset -t nrf54l
```

After reboot, the node should still show:

```text
<inf> main: Credentials loaded
<inf> main: Pouch initialized
<inf> main: Advertising started
```

The XIAO LED also reports startup progress with blink groups:

```text
1 blink  - application started and LED GPIO initialized
2 blinks - Bluetooth initialized
3 blinks - credentials loaded and Pouch initialized
4 blinks - button and soil sensor setup completed
5 blinks - BLE advertising started and gateway sync requested
```

If the LED stops before five blinks, check the serial log for the failing stage.

## 5. Re-upload the Node Device Cert and Key if Needed

Flashing the app normally should not erase LittleFS, but if the filesystem was
erased or reformatted, upload the node cert and key again.

Close the COM8 serial terminal first, then run from PowerShell:

```powershell
cd C:/ncs_pouch_soil/pouch
& "$env:APPDATA\Python\Python312\Scripts\smpmgr.exe" --port COM8 --mtu 128 file upload .\certs\chocolate-voiceless-mastodon.crt.der /lfs1/credentials/crt.der
& "$env:APPDATA\Python\Python312\Scripts\smpmgr.exe" --port COM8 --mtu 128 file upload .\certs\chocolate-voiceless-mastodon.key.der /lfs1/credentials/key.der
```

To check whether they are present:

```powershell
& "$env:APPDATA\Python\Python312\Scripts\smpmgr.exe" --port COM8 --mtu 128 file list /lfs1/credentials
```

## 6. Retry the Gateway Sync

Reboot both boards:

```text
kernel reboot
```

Expected node behavior after the gateway connects:

```text
<inf> main: BT security changed to level 2
```

The node should no longer print:

```text
<err> cert: Failed verifying server cert: 0x2700, 8
<err> saead_session: Missing server key
```

## Troubleshooting

If `0x2700, 8` continues, rebuild from a pristine build directory and confirm the
CA file is present before building:

```powershell
Test-Path C:/ncs_pouch_soil/pouch/src/goliothrootx1.der
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

If the gateway is changed to a non-production Golioth endpoint, such as a dev
endpoint, the node must be built with the matching CA and expected server common
name. The production defaults are for `coap.golioth.io` and `pouch.golioth.io`.

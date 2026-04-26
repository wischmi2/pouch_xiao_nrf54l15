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

The current stable soil sensor node image also uses:

```text
CONFIG_POUCH_THREAD_STACK_SIZE=8192
CONFIG_POUCH_UPLINK_PROCESSING_STACK_SIZE=8192
CONFIG_GOLIOTH_SETTINGS=n
CONFIG_GOLIOTH_OTA=n
```

The larger Pouch stacks prevent the callback/uplink path from corrupting the
node during soil sensor sync. `CONFIG_GOLIOTH_SETTINGS=y` currently reproduces a
`pouch_work` crash after server certificate verification, so cloud Settings are
disabled for the working soil sensor build. With Settings disabled, Golioth
cannot send Settings-based commands to the node, but normal soil data uplinks
continue to work.

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
$env:PYTHONUTF8='1'
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file upload .\certs\chocolate-voiceless-mastodon.crt.der /lfs1/credentials/crt.der
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file upload .\certs\chocolate-voiceless-mastodon.key.der /lfs1/credentials/key.der
```

To check whether they are present:

```powershell
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file read-size /lfs1/credentials/crt.der
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file read-size /lfs1/credentials/key.der
```

Expected sizes for the two current XIAO certificates are `380` bytes for
`crt.der` and `138` bytes for `key.der`. If LittleFS reformats after flashing,
the node will stop at:

```text
<err> main: Failed to load certificate (err -2)
```

Re-upload the cert and key, then reset the node.

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

The known-working production gateway path is to let the gateway download the
server certificate chain from Golioth at runtime and send that chain to the node.
The gateway config for the FRDM-RW612 WiFi build therefore keeps:

```text
CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n
```

Do not force the bundled gateway server certificate for this setup. That can make
the node verify the certificate successfully while still encrypting the Pouch
uplink to a public key that Golioth does not accept. The gateway symptom is:

```text
<err> uplink: Uplink CoAP response: 4.00
```

The expected successful gateway logs are:

```text
<inf> cert: Device cert cloud set CoAP response: 2.05
<inf> cert: Golioth accepted node device cert
<inf> uplink: Sending uplink block 0: len 111, last 1, closed 1
<inf> uplink: Delivered uplink block 0: path pouch, block_size 1024
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

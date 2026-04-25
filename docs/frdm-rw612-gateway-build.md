# FRDM-RW612 Gateway Build Notes

This records the steps needed to build the in-tree `pouch/examples/gateway` application for the FRDM-RW612 gateway hardware on Windows.

## Why Use `west-zephyr.yml`

The FRDM-RW612 gateway is built by the project CI with `west-zephyr.yml`, not `west-ncs.yml`.

The `west-ncs.yml` manifest does not import `hal_nxp`, so the build fails while preprocessing the board devicetree:

```text
fatal error: nxp/rw/RW612-pinctrl.h: No such file or directory
```

Switch the workspace manifest:

```powershell
west config manifest.file west-zephyr.yml
west update
west patch apply
```

## Fetch NXP Blobs

The RW612 board needs NXP HAL binary blobs.

```powershell
west blobs fetch -a hal_nxp
```

If this fails with `No module named 'requests'`, install `requests` and retry:

```powershell
python -m pip install requests
west blobs fetch -a hal_nxp
```

## Install Zephyr Python Requirements

The Zephyr manifest uses Zephyr 4.3.0 and needs the base script requirements:

```powershell
python -m pip install -r C:/ncs_pouch_soil/zephyr/scripts/requirements-base.txt
```

This fixed errors such as:

```text
ModuleNotFoundError: No module named 'jsonschema'
```

## Windows Build Fix

`examples/gateway/CMakeLists.txt` originally generated `git_describe.h` by running a shell script and `chmod`:

```text
'chmod' is not recognized as an internal or external command
```

The build was fixed by replacing that shell-script step with a cross-platform CMake script that writes `git_describe.h`.

## Build Command

For FRDM-RW612 using WiFi:

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
$env:Path = "C:/Users/Brian/AppData/Local/Programs/Python/Python312/Scripts;$env:Path"

west build -p always -b frdm_rw612 pouch/examples/gateway `
  --build-dir C:/ncs_pouch_soil/build-gateway-frdm_rw612 `
  -- -DEXTRA_CONF_FILE=boards/frdm_rw612_wifi.conf
```

This build completed successfully.

This FRDM WiFi configuration uses
`CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n`, so the gateway downloads the Pouch
server certificate chain from Golioth at runtime and sends that chain to Pouch
nodes. This is the known-working path for the soil sensor build.

Do not force the bundled `src/gateway/server-prod.pem` certificate chain for this
setup. With the bundled chain, the node certificate was accepted by Golioth, but
uplinks to `.g/pouch` failed with CoAP `4.00 Bad Request`. With the
Golioth-downloaded chain, the successful gateway log includes:

```text
<inf> cert: Device cert cloud set CoAP response: 2.05
<inf> cert: Golioth accepted node device cert
<inf> uplink: Delivered uplink block 0: path pouch, block_size 1024
```

## Flash Command

When both the XIAO debugger and FRDM debugger are connected, list probes first:

```powershell
pyocd list
```

The FRDM board showed up as the SEGGER/J-Link MCU-Link probe:

```text
Segger J-Link MCU-Link  1060877759
```

Flash the gateway build to that probe:

```powershell
west flash --build-dir C:/ncs_pouch_soil/build-gateway-frdm_rw612 --dev-id 1060877759
```

## Switching Back to XIAO/NCS Builds

The XIAO app build uses the NCS manifest:

```powershell
west config manifest.file west-ncs.yml
west update
```

After switching manifests, rebuild the XIAO app with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

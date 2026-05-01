# XIAO Debug Mate Setup for XIAO nRF54L15

This document captures the exact local setup for debugging the XIAO nRF54L15
through a XIAO Debug Mate in this workspace.

## What This Workspace Uses

- Board target: `xiao_nrf54l15/nrf54l15/cpuapp`
- App used here: `pouch/examples/ble_gatt`
- Probe transport: CMSIS-DAP over SWD
- Debug server: OpenOCD
- GDB: Zephyr SDK `arm-zephyr-eabi-gdb`

The Zephyr board support for this target already defaults to CMSIS-DAP and SWD
through OpenOCD, so the Debug Mate path matches the board support in this repo.

## Hardware Setup

1. Plug the XIAO nRF54L15 into the XIAO Debug Mate in the correct XIAO socket
   orientation.
2. Connect the Debug Mate to the PC with USB.
3. If the target does not power up from the Debug Mate alone, also connect the
   XIAO nRF54L15 USB port for power.
4. Confirm the host sees the debug probe:

   ```powershell
   pyocd list
   ```

Expected output on this machine looked like:

```text
0   Espressif Systems CMSIS-DAP   RM6N5DPKBWFA5C7GXATDCFGMSCWVFJMW   n/a
```

If `pyocd list` does not show a CMSIS-DAP probe, stop there and fix USB,
power, cable, or driver issues before trying OpenOCD or VS Code.

## Prerequisites

Install the VS Code extension:

- `marus25.cortex-debug`

This workspace now also includes these VS Code files:

- `C:/ncs_pouch_soil/.vscode/launch.json`
- `C:/ncs_pouch_soil/.vscode/tasks.json`

Required local tools already referenced by this workspace:

- OpenOCD:
  `C:/Users/Brian/AppData/Local/Microsoft/WinGet/Packages/xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe/xpack-openocd-0.12.0-7/bin/openocd.exe`
- GDB:
  `C:/Users/Brian/zephyr-sdk-0.17.2/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb.exe`

## Build the Firmware

If the BLE GATT app is not already built, build it with:

```powershell
west build -b xiao_nrf54l15/nrf54l15/cpuapp C:/ncs_pouch_soil/pouch/examples/ble_gatt --build-dir C:/ncs_pouch_soil/pouch/examples/ble_gatt/build
```

The debugger uses this ELF file:

```text
C:/ncs_pouch_soil/pouch/examples/ble_gatt/build/zephyr/zephyr.elf
```

## Debug from the Terminal First

Before using VS Code, verify that OpenOCD can attach through west:

```powershell
west debug --build-dir C:/ncs_pouch_soil/pouch/examples/ble_gatt/build
```

This workspace already configures OpenOCD as the default debug runner for the
XIAO nRF54L15 build.

If this works, the launch configuration in `.vscode/launch.json` should also
work.

## What We Found While Debugging the Debugger

The initial VS Code attach failure was not caused by a bad `launch.json` path,
missing ELF, or missing extension.

Two separate failure modes were reproduced.

### Failure 1: Stale Debugger Processes

Stale debugger processes left behind from a prior failed or aborted session can
block the next attach. When those stale processes were still running, OpenOCD
failed with:

```text
Error: CMSIS-DAP command mismatch. Sent 0x0 received 0x5
Error: CMSIS-DAP command CMD_INFO failed.
```

At the same time, Windows still showed old `openocd` and
`arm-zephyr-eabi-gdb` processes alive.

After killing those processes and retrying the same OpenOCD startup command,
the probe connected cleanly and OpenOCD detected the nRF54L15 application CPU.

The working conclusion for that case is:

- the Debug Mate probe is visible to the host
- the board OpenOCD config can start correctly
- stale OpenOCD or GDB processes can block the next session from attaching

### Failure 2: OpenOCD Rejected the GDB Attach

Even after OpenOCD started cleanly, the GDB connection was still rejected.

The OpenOCD output showed:

```text
Error: Failed to read memory at 0x00ff0140
Error: Failed to read memory at 0x00ff020c
Error: Failed to read memory at 0x10000100
Error: Failed to read memory at 0x1000005c
Error: Couldn't read FICR CONFIGID register
Error: auto_probe failed
Error: Connect failed. Consider setting up a gdb-attach event for the target to prepare target for GDB connect, or use 'gdb_memory_map disable'.
Error: attempted 'gdb' connection rejected
```

That means OpenOCD was alive and the probe was connected, but OpenOCD could not
auto-probe the target memory map during the GDB attach.

The fix that worked was to disable OpenOCD's GDB memory map probing.

With `gdb memory_map disable` added, manual GDB attach succeeded and broke at
`main`.

## VS Code Protection Against Stale Debug Processes

The launch configuration now runs a pre-launch task before every debug session:

- `Kill stale debug processes`

That task forcibly stops any old instances of:

- `openocd`
- `pyocd`
- `arm-zephyr-eabi-gdb`
- `gdb`

This reduces the chance of hitting the CMSIS-DAP mismatch on the next launch.

## VS Code Protection Against GDB Attach Rejection

The launch configuration now also passes this OpenOCD server argument:

- `-c "gdb memory_map disable"`

That disables the target memory map auto-probe that was causing the GDB
connection to be rejected on this nRF54L15 setup.

## VS Code Launch Mode Used Here

The current workspace launch does not let Cortex-Debug manage OpenOCD directly.

Instead it uses:

- a background VS Code task to start OpenOCD
- `servertype: "external"` in Cortex-Debug
- `gdbTarget: "localhost:3333"`
- `request: "attach"` instead of `request: "launch"`

This was chosen because the manual OpenOCD plus GDB flow was proven to work,
while the managed OpenOCD path inside Cortex-Debug remained unreliable.

The key detail is that the working manual flow behaved like an attach session:
connect to the already-running OpenOCD GDB server, send `monitor reset halt`,
then set a temporary breakpoint on `main` and continue.

So the current Run and Debug flow is:

1. kill stale debugger processes
2. start OpenOCD as a background task
3. attach Cortex-Debug to `localhost:3333`
4. send `monitor reset halt`
5. set a temporary breakpoint on `main`
6. continue to `main`

## Debug in VS Code

Steps:

1. Open the workspace root `C:/ncs_pouch_soil` in VS Code.
2. Install the `Cortex-Debug` extension if it is not already installed.
3. Make sure `pouch/examples/ble_gatt/build/zephyr/zephyr.elf` exists.
4. Open the Run and Debug view.
5. Select `XIAO nRF54L15 OpenOCD`.
6. Press `F5`.
7. VS Code should start OpenOCD, connect GDB, load symbols from the ELF, and
   stop at `main`.

This launch now also:

- enables raw Cortex-Debug backend output in the debug console
- disables OpenOCD GDB memory map probing for this target
- uses Cortex-Debug external-server mode instead of managed OpenOCD mode
- uses attach mode instead of launch mode
- forces `monitor reset halt` during attach, reset, and restart
- sets a temporary breakpoint on `main` and continues automatically

If a session still fails, first stop the current debug session completely, then
retry `F5`. The pre-launch cleanup task should handle the common stale-process
case automatically.

## Common Failure: AP Lock / APPROTECT

If `west debug` or VS Code fails with messages like these:

```text
Error: Failed to read memory at 0xe000ed00
Error: [nrf54l.cpu] Examination failed
Error: [nrf54l.cpu] DP initialisation failed
```

then the device is likely AP-locked.

In this workspace, the local board OpenOCD config already contains the nRF54L15
CTRL-AP mass erase recovery logic, so retrying through the local board config is
the intended recovery path.

If the device still does not recover, see:

- `pouch/docs/xiao-nrf54l15-locked-recovery.md`

Important: recovery mass-erases the device.

That means:

- application flash is erased
- LittleFS contents are erased
- node credentials must be uploaded again afterward

## After Recovery

If this Pouch node image was recovered or mass-erased, re-upload the credentials
to LittleFS before expecting the node to authenticate:

```powershell
smpmgr --port COM11 --timeout 10 --line-length 128 --line-buffers 2 file upload C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.crt.der /lfs1/credentials/crt.der
smpmgr --port COM11 --timeout 10 --line-length 128 --line-buffers 2 file upload C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.key.der /lfs1/credentials/key.der
```

Then reset the target if needed:

```powershell
pyocd reset -t nrf54l --probe RM6N5DPKBWFA5C7GXATDCFGMSCWVFJMW
```

## Summary

The shortest reliable path is:

1. Confirm `pyocd list` shows the Debug Mate as CMSIS-DAP.
2. Build `pouch/examples/ble_gatt` for `xiao_nrf54l15/nrf54l15/cpuapp`.
3. Prove the connection with `west debug`.
4. Launch `XIAO nRF54L15 OpenOCD` from VS Code.
5. If attach fails, use the AP-lock recovery path and then re-upload
   credentials.
6. If the error mentions CMSIS-DAP mismatch or the probe behaves busy, kill any
   stale `openocd` and `gdb` processes and retry.
7. If OpenOCD starts but rejects the GDB connection, disable GDB memory map
   probing. The workspace launch configuration now already does this.
# Pouch verified firmware builds

Pre-built XIAO nRF54L15 (`ble_gatt`) images keyed by **Pouch commit SHA**. Each subdirectory is one reproducible build from [`docs/verified-workspace-snapshots.md`](../docs/verified-workspace-snapshots.md).

| Folder | Pouch commit | Notes |
|--------|--------------|--------|
| [`de25234/`](de25234/) | `de25234` (`v0.1.0-216`) | Last commit **before** `cert.c` / link workaround; NCS 3.2.3 + Zephyr 4.2.99 |
| [`b539f2f/`](b539f2f/) | `b539f2f` (`v0.1.0-223`) | Last likely-good field stack; soil moisture **percent**; build with `CONFIG_MBEDTLS_X509_REMOVE_INFO=n` |

## XIAO dual flash images (UART + battery)

Rebuild both with:

```powershell
.\scripts\build_ble_gatt_xiao_dual.ps1
```

| Folder | Use when |
|--------|----------|
| [`xiao-ble-gatt-uart/`](xiao-ble-gatt-uart/) | **Scenario A** — USB connected: flash, serial logs, `smpmgr` credential upload |
| [`xiao-ble-gatt-battery/`](xiao-ble-gatt-battery/) | **Scenario B** — LiPo-only field deploy (no UART) |

Flash with **pyOCD** (recommended — preserves LittleFS better than OpenOCD `west flash`):

```powershell
.\scripts\flash_xiao_pyocd.ps1 -Variant uart
.\scripts\flash_xiao_pyocd.ps1 -Variant battery
```

OpenOCD via `west flash` can trigger **mass erase** on AP-lock recover and wipe credentials.

Typical workflow: flash **uart** → upload `crt.der` / `key.der` → flash **battery**.

See [`docs/xiao-battery-hardware-and-charging.md`](../docs/xiao-battery-hardware-and-charging.md).

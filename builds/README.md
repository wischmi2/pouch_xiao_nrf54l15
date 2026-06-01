# Pouch verified firmware builds

Pre-built XIAO nRF54L15 (`ble_gatt`) images keyed by **Pouch commit SHA**. Each subdirectory is one reproducible build from [`docs/verified-workspace-snapshots.md`](../docs/verified-workspace-snapshots.md).

| Folder | Pouch commit | Notes |
|--------|--------------|--------|
| [`de25234/`](de25234/) | `de25234` (`v0.1.0-216`) | Last commit **before** `cert.c` / link workaround; NCS 3.2.3 + Zephyr 4.2.99 |

Flash `zephyr.bin` or `zephyr.hex` with your usual XIAO app-only flow (see `docs/v0.1.0-build-steps.md` §8–9 on `soil_sensor`).

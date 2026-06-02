# Pouch verified firmware builds

Pre-built XIAO nRF54L15 (`ble_gatt`) images keyed by **Pouch commit SHA**. Each subdirectory is one reproducible build from [`docs/verified-workspace-snapshots.md`](../docs/verified-workspace-snapshots.md).

| Folder | Pouch commit | Notes |
|--------|--------------|--------|
| [`de25234/`](de25234/) | `de25234` (`v0.1.0-216`) | Last commit **before** `cert.c` / link workaround; NCS 3.2.3 + Zephyr 4.2.99 |
| [`b539f2f/`](b539f2f/) | `b539f2f` (`v0.1.0-223`) | Last likely-good field stack; soil moisture **percent**; build with `CONFIG_MBEDTLS_X509_REMOVE_INFO=n` |

Flash `zephyr.bin` or `zephyr.hex` with your usual XIAO app-only flow (see `docs/v0.1.0-build-steps.md` §8–9 on `soil_sensor`).

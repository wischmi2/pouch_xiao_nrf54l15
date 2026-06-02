# Golioth `pouch` changes since `d161705` and cert error `0x2700, 8`

This note maps what happened on **upstream** [`github.com/golioth/pouch`](https://github.com/golioth/pouch) after your fork split, and what matters for the node error:

```text
<err> cert: Failed verifying server cert: 0x2700, 8
<err> cert: Server cert verify flags:
          ! The certificate is not correctly signed by the trusted CA
```

## Where your tree sits

| Reference | Commit | Date (author tz) | Meaning |
|-----------|--------|------------------|---------|
| **Last shared Golioth commit** | [`d161705`](https://github.com/golioth/pouch/commit/d1617059a167d63d28348df1b6093903b658535c) | 2026-04-24 | Last `golioth/pouch` `main` commit that is also an ancestor of your firmware |
| **Your fork tip (checked out)** | [`00443f2`](https://github.com/wischmi2/pouch_xiao_nrf54l15/commit/00443f2486e053b07ea5f85dc8c405fb2b25cbb8) | 2026-04-25 | +11 commits on your fork after the split |
| **Golioth `main` today** | [`ec6d183`](https://github.com/golioth/pouch/commit/ec6d183c2392bb6e459339c463ce04749b037d18) | 2026-06-02 | +93 commits on Golioth after the split (you do **not** have these) |

**Compare on GitHub (what you are missing from Golioth):**

https://github.com/golioth/pouch/compare/d161705...main

**Compare on GitHub (what only your fork added):**

https://github.com/wischmi2/pouch_xiao_nrf54l15/compare/d161705...00443f2

---

## Summary: does upstream fix `0x2700, 8`?

**No.** Between `d161705` and Golioth `main` (`ec6d183`), there is **no commit** that adds `CONFIG_MBEDTLS_PEM_PARSE_C`, PEM-aware server-chain parsing on the node, or other targeted fix for BLE-delivered PEM chains.

Upstream `src/cert.c` at `golioth/main` still uses a single call:

```c
mbedtls_x509_crt_parse(out, cert->buffer, cert->size);
```

with no PEM NUL handling. Upstream `examples/zephyr/ble_gatt/prj.conf` also does **not** enable the explicit SHA-384 / secp384r1 / PEM options your fork added.

The `0x2700, 8` issue on the XIAO node is explained by **how the gateway forwards the server chain** plus **node mbedTLS Kconfig**, not by missing a Golioth fix from April–June 2026.

---

## What Golioth changed since `d161705` (93 commits)

High-level themes on `golioth/pouch` `main` (Apr 24 → Jun 2, 2026). Full list:

```bash
git log --oneline d161705..golioth/main
```

### 1. ESP-IDF port and examples (majority of work)

- New **`examples/esp_idf/ble_gatt`**, HTTP client, OTA, settings, runtime provisioning, pytest
- **`port/esp_idf`**: BLE GATT transport, delayable work, semaphores, component packaging for `golioth_sdk`
- CI: ESP-IDF unit tests, pinned Python requirements, README updates

**Relevance to XIAO / `0x2700, 8`:** None directly (nRF54L15 uses Zephyr `examples/ble_gatt`, not ESP-IDF).

### 2. Example layout move (Zephyr)

- **`8f066f3`** — `examples/ble_gatt` → `examples/zephyr/ble_gatt`
- Gateway example → `examples/zephyr/gateway`
- HTTP client → `examples/zephyr/http_client`

**Relevance:** Paths in docs/scripts must use `examples/zephyr/...` on upstream; your fork still uses `examples/ble_gatt` at repo root.

### 3. Gateway / transport refactor

| Commit | Subject | Cert / trust impact |
|--------|---------|---------------------|
| `1f1be19` | Gateway: Split transport | Structural; no change to trust-anchor logic |
| `c07aa3a` | Transport: default window 3 | BLE throughput only |
| `aab1c3a` | golioth_sdk: rename log module | Build/logging only |
| `7f754a0` | examples: zephyr: gateway: fix path for frdm_rw612_wifi | Path fix only |

Upstream also **removed** the old top-level `examples/gateway/` tree from that directory layout (gateway lives under `examples/zephyr/gateway` now). Your fork still has `examples/gateway/` as before `d161705`.

### 4. Core library / SAR / uplink

- SAR sender/receiver fixes, `pouch_work_delayable`, uplink session end (`2f665ef`), buffer queue mutex, portable logging (`56a068d`)

**Relevance:** May affect uplink **after** cert verify succeeds; not the root cause of `0x2700, 8`.

### 5. Only “CA” touch on upstream in this range

| Commit | Change |
|--------|--------|
| `7cb06e8` | **Typo fix** in `examples/zephyr/http_client` Kconfig (“server” spelling) — **not** XIAO BLE |

### 6. Small shared-file diffs (same period)

**`src/cert.c` (upstream vs `d161705`):** include moves + `%zu` for serial size — **no** verify/parse behavior change.

**`src/gateway/cert.c`:** logging macro portability (`LOG_*` → `POUCH_LOG_*`) — **no** change to cert bytes sent over BLE.

**`src/gateway/Kconfig`:** consolidates GATT window sizes, adds transport ACK timeout — unrelated to trust.

---

## What your fork added (11 commits since `d161705`)

These exist **only** on `wischmi2/pouch_xiao_nrf54l15`, not on `golioth/pouch`:

| Commit | Subject | Cert / XIAO relevance |
|--------|---------|------------------------|
| `33412d8` | v0.1.0 build workflow docs/script | Build reproducibility |
| `ddba5e6` | Merge `origin/main` into nrf54l15 | Sync |
| `f1d32b4` | XIAO app-only build path docs | Build |
| `b965e36` | XIAO soil sensor ADC uplink | Feature |
| `de25234` | XIAO LittleFS overlay | Credentials path |
| `0c53b2a` | XIAO soil bring-up docs | Docs |
| `d42c790` | Working soil + gateway path | Docs |
| `3ae0ff9` | Manual uplinks, multiple XIAO bonds | BLE |
| `1639474` | BLE security retry cooldown | BLE stability |
| `d8f79b3` | OpenOCD recovery patch | Flash/recovery |
| `00443f2` | Stabilize XIAO soil sensor uplinks | Integration |

### Fork changes that touch certificates

**`examples/ble_gatt/prj.conf` (committed at `00443f2`):**

- Larger Pouch thread stacks
- Explicit `CONFIG_PSA_WANT_ALG_SHA_384`, secp256r1/384, `MBEDTLS_ECDSA_C`, `MBEDTLS_ECP_*` (needed for Golioth server chain signatures)
- `CONFIG_GOLIOTH_SETTINGS=n`, `CONFIG_GOLIOTH_OTA=n` (avoids post-verify crash documented in `docs/pouch-node-compiled-ca.md`)

**`src/cert.c` (committed at `00443f2`):**

- Logs CA load, received chain, verify flags, success CN

**`src/gateway/cert.c` (committed at `00443f2`):**

- `CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN` path (embed `server-prod.pem`) **in addition to** cloud download
- More gateway cert logging

**Docs added:** `docs/pouch-node-compiled-ca.md`, `docs/frdm-rw612-gateway-build.md`, `docs/xiao-vs-frdm-setup.md`, etc.

---

## Root cause of `0x2700, 8` (flag `8` = not trusted)

### Three certificate roles (do not mix them up)

| Role | Source | Purpose |
|------|--------|---------|
| **Node identity** | LittleFS `/lfs1/credentials/crt.der` + `key.der` | Proves the node to the gateway / Golioth |
| **Trust anchor (CA)** | Compiled at build: `src/goliothrootx1.der` via `CONFIG_POUCH_CA_CERT_FILENAME` | Verifies the **server chain** from the gateway |
| **Server chain** | Gateway → BLE GATT server-cert characteristic | Leaf + intermediates for `pouch.golioth.io`; node extracts server public key for SAead |

### Expected chain (production)

Bundled gateway file `src/gateway/server-prod.pem` is **1478 bytes**, **PEM**, **2 certificates**:

1. **Leaf** — `CN=pouch.golioth.io` (signed by `CN=E1`)
2. **Intermediate** — `CN=E1` (signed by `CN=Golioth Root X1`)

Trust anchor in firmware must be **`goliothrootx1.der`** (Golioth Root X1, ECDSA P-384), **not** ISRG/Let’s Encrypt, for this chain.

### Why verification fails on the node today

1. **Gateway sends PEM** (builtin `server-prod.pem` or Golioth download — raw bytes, no DER conversion).
2. **Node had `CONFIG_MBEDTLS_PEM_PARSE_C` disabled** — mbedTLS treats the buffer as DER; PEM is mis-parsed → broken chain → `mbedtls_x509_crt_verify` → **`0x2700`** with flag **`8`** (`BADCERT_NOT_TRUSTED`).
3. **Builtin vs runtime gateway cert:** `CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=y` forces embedded PEM; `=n` uses Golioth at runtime (preferred in your docs so uplink public key matches cloud). Both are still typically **PEM**; the node must parse PEM either way.

### Fixes (local, not yet on Golioth upstream)

**Uncommitted in your working tree** (as of this doc); should be committed after verify:

| Change | File | Why |
|--------|------|-----|
| `CONFIG_MBEDTLS_PEM_PARSE_C=y` | `examples/ble_gatt/prj.conf` | Enable PEM parsing in mbedTLS |
| `CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"` | same | Explicit production CA (default in Kconfig, but set for clarity) |
| PEM NUL + chain length log | `src/cert.c` | PEM requires null-terminated buffer; log should show **2** certs in server chain |

**Do not use** for this chain unless you know the cloud path changed:

- `src/isrgrootx1.der` / `src/isrg_golioth_ca.der` — ISRG Root X1 is RSA-4096; wrong anchor for `server-prod.pem` and needs extra RSA MPI Kconfig.

**After flash, expect in serial log:**

```text
<err> cert: Server chain: 2 certificate(s) in chain
<err> cert: Server chain[0]: subject=...
<err> cert: Server chain[1]: subject=CN=..., O=Golioth...
<inf> cert: Loaded Pouch CA cert (548 bytes)
<inf> cert: Server cert verified against CN pouch.golioth.io
```

(`log_chain_summary` uses `ERR` level so it is not dropped when the log buffer fills.)

**If you see `Server chain: 1 certificate(s)`** the intermediate `E1` cert was not parsed (PEM/NUL/`PEM_PARSE` issue) → verify fails with flag `8`.

**Bundled `server-prod.pem` leaf expires 2025-11-23.** If the gateway uses `CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=y`, the chain may be expired on a device with correct time. Prefer `BUILTIN=n` and the runtime Golioth download for a current leaf.

If you still see `0x2700, 8` **after** `Parsed 2 certificate(s)`, the problem is no longer PEM parsing — check CA file in build (`pouch_ca_cert.inc`), `CONFIG_POUCH_SERVER_CERT_CN`, and that the gateway chain matches production (runtime download vs stale builtin).

---

## Gateway settings (FRDM-RW612 WiFi)

From `docs/pouch-node-compiled-ca.md` and `examples/gateway/boards/frdm_rw612_wifi.conf`:

```text
CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n
```

Use the chain **downloaded from Golioth** so the node encrypts to the same key the cloud expects. Forcing builtin `server-prod.pem` can verify locally but cause gateway uplink `4.00` if the cloud key rotated.

Rebuild gateway when changing this:

```powershell
west build -p always -b frdm_rw612 pouch/examples/gateway `
  --build-dir C:/Users/Brian/ncs/build-gateway-frdm_rw612 `
  -- -DEXTRA_CONF_FILE=boards/frdm_rw612_wifi.conf
```

---

## Node rebuild checklist (XIAO)

From NCS workspace (`west-ncs.yml`), not the standalone Zephyr tree:

```powershell
cd C:\Users\Brian\ncs\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild `
  -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

Confirm in `build/zephyr/.config`:

```text
CONFIG_MBEDTLS_PEM_PARSE_C=y
CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"
CONFIG_PSA_WANT_ALG_SHA_384=y
CONFIG_MBEDTLS_ECP_DP_SECP384R1_ENABLED=y
```

Flash:

```powershell
pyocd flash -u <probe-id> -t nrf54l build\zephyr\zephyr.hex
```

---

## Should you merge Golioth `main` now?

| If you need… | Action |
|--------------|--------|
| Fix `0x2700, 8` only | Apply PEM + CA fixes above; **no** need to merge 93 upstream commits |
| ESP-IDF / new Zephyr example paths | Merge or cherry-pick; resolve `examples/ble_gatt` → `examples/zephyr/ble_gatt` move |
| Latest SAR/uplink fixes on Zephyr | Cherry-pick relevant commits (e.g. `2f665ef`, SAR fixes) and test on XIAO |

Merging all of `golioth/main` into your fork is a **large** integration (layout + gateway paths + ESP-IDF). It does **not** substitute for the PEM parse fix.

---

## Related docs in this repo

- [`docs/pouch-node-compiled-ca.md`](pouch-node-compiled-ca.md) — CA vs device cert, build, flash, `0x2700, 8` symptom
- [`docs/xiao-vs-frdm-setup.md`](xiao-vs-frdm-setup.md) — two-board setup, `BUILTIN=n`
- [`docs/frdm-rw612-gateway-build.md`](frdm-rw612-gateway-build.md) — gateway build

---

## Revision

- **Generated:** 2026-06-02
- **Fork tip referenced:** `00443f2` (+ uncommitted `prj.conf` / `cert.c` PEM fixes)
- **Upstream tip referenced:** `ec6d183` on `golioth/main`

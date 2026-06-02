# Fix: `Failed verifying server cert: 0x2700, 8` (XIAO nRF54L15)

This document records how we fixed Pouch node server-certificate verification and got
end-to-end uplinks showing in Golioth. Use it to reproduce the fix on a clean machine.

**Date fixed:** 2026-06-02  
**Hardware:** Seeed XIAO nRF54L15 (node) + NXP FRDM-RW612 (gateway) + Golioth production (`coap.golioth.io`)

---

## Executive summary

### What was broken

The node logged:

```text
<err> cert: Failed verifying server cert: 0x2700, 8
<err> cert: Server cert verify flags:
          ! The certificate is not correctly signed by the trusted CA
<err> saead_session: Missing server key
<err> saead_uplink: Session key generation failed
```

- `0x2700` = `MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`
- Flag `8` = `MBEDTLS_X509_BADCERT_NOT_TRUSTED` — chain could not be cryptographically validated against the compiled trust anchor.

### What actually fixed it (node)

**The decisive fix:** enable **`CONFIG_MBEDTLS_USE_PSA_CRYPTO=y`** so mbedTLS X.509 signature verification uses the **PSA/CRACEN** path on nRF54L15. The legacy mbedTLS ECDSA path could **parse** the Golioth chain (leaf + `E1` intermediate + `Golioth Root X1` trust anchor) but **failed to verify** the `E1` certificate signature against the root.

**Also required:** **`CONFIG_MBEDTLS_PEM_PARSE_C=y`** because the gateway sends the server chain as **PEM** (~1478 bytes), not DER.

**Trust anchor:** keep **`src/goliothrootx1.der`** (not ISRG/Let’s Encrypt). The production chain is:

```text
leaf (SAN: pouch.golioth.io) → intermediate CN=E1 → Golioth Root X1
```

### What fixed end-to-end Golioth data (gateway)

After the node verified the cert, uplinks still failed with **`Uplink CoAP response: 4.00`** while the gateway log showed **`Loaded builtin server cert`**.

**Gateway fix:** rebuild with **`CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n`** so the FRDM downloads the **current** server cert from Golioth at runtime (see `examples/gateway/boards/frdm_rw612_wifi.conf`). The bundled `server-prod.pem` leaf had **expired** and did not match the key Golioth expects for decrypting pouch uplinks.

### Success criteria

**Node serial:**

```text
<inf> cert: Server cert verified against CN pouch.golioth.io
<inf> main: Writing soil sensor uplink: path .s/sensor, ...
```

**Gateway serial:**

```text
<inf> cert: Golioth accepted node device cert
<inf> uplink: Delivered uplink block 0: path pouch, block_size 1024
```

(No `Loaded builtin server cert`, no `4.00`.)

---

## Software bill of materials (exact versions used when fixed)

| Component | Version / ID | Notes |
|-----------|----------------|-------|
| **Pouch repo (fork)** | `00443f2486e053b07ea5f85dc8c405fb2b25cbb8` (`00443f2`) | Detached HEAD; **plus uncommitted changes below** |
| **Pouch vs upstream Golioth** | Last shared commit [`d161705`](https://github.com/golioth/pouch/commit/d1617059a167d63d28348df1b6093903b658535c) (2026-04-24); then **11 fork-only commits** to `00443f2` |
| **Pouch remote** | `https://github.com/wischmi2/pouch_xiao_nrf54l15.git` | |
| **Golioth firmware SDK** | `d703b1f8805c7584a44dabc31bdf09164637d888` | From `west-ncs.yml` / `west-zephyr.yml` in Pouch |
| **NCS (XIAO builds)** | **v3.2.3** (`west-ncs.yml`) | Zephyr **4.2.99** (`KERNEL_VERSION_STRING` in node build) |
| **Zephyr (gateway builds)** | **v4.3.0** (`west-zephyr.yml`) | Required for `hal_nxp` / FRDM-RW612 |
| **nRF Connect SDK path** | `C:\Users\Brian\ncs` | Workspace; `pouch` is west `self` project |
| **Zephyr SDK (XIAO)** | Toolchain `fd21892d0f` → `C:\ncs\toolchains\fd21892d0f\opt\zephyr-sdk` | |
| **Node board** | `xiao_nrf54l15/nrf54l15/cpuapp` | `--no-sysbuild` |
| **Gateway board** | `frdm_rw612` | `--no-sysbuild`, `EXTRA_CONF_FILE=boards/frdm_rw612_wifi.conf` |
| **Node flash** | `pyocd` probe `WYKL7D7EW6T3VCGIXBB6CHE433NVRQIN`, target `nrf54l` | |
| **Gateway flash** | `west flash`, J-Link `dev-id 1060877759` | |
| **Golioth** | Production `coap.golioth.io`, server CN `pouch.golioth.io` | |
| **Compiled CA file** | `src/goliothrootx1.der` (548 bytes, SHA256 `f03fec5a25b8d5dda45524c871a75913ef55fbb69408e8a2ddcc935f6fca98a0`) | Default in Kconfig; do **not** use `isrgrootx1.der` for this chain |

### West manifests (two workspaces, same `pouch` tree)

| Target | `west config manifest.file` | Zephyr |
|--------|----------------------------|--------|
| **XIAO node** | `west-ncs.yml` (in `pouch/`) | 4.2.99 via NCS 3.2.3 |
| **FRDM gateway** | `west-zephyr.yml` (in `pouch/`) | 4.3.0 |

After building the gateway, switch back to `west-ncs.yml` before rebuilding the node:

```powershell
cd C:\Users\Brian\ncs
west config manifest.file west-ncs.yml
west update
```

---

## Code changes required (not yet committed at time of writing)

Base commit: **`00443f2`**. These files must contain the changes below (commit them to reproduce from git alone).

### 1. `examples/ble_gatt/prj.conf`

Add or confirm (remove duplicate `CONFIG_MBEDTLS_USE_PSA_CRYPTO` if present twice):

```ini
# Pouch server certificates chain to Golioth Root X1 using ECDSA/SHA-384 / secp384r1
CONFIG_PSA_WANT_ALG_SHA_384=y
CONFIG_PSA_WANT_ECC_SECP_R1_256=y
CONFIG_PSA_WANT_ECC_SECP_R1_384=y
CONFIG_PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY=y
CONFIG_MBEDTLS_ECDSA_C=y
CONFIG_MBEDTLS_ECP_C=y
CONFIG_MBEDTLS_ECP_DP_SECP256R1_ENABLED=y
CONFIG_MBEDTLS_ECP_DP_SECP384R1_ENABLED=y

# Gateway sends PEM over BLE; without this mbedTLS treats PEM as DER.
CONFIG_MBEDTLS_PEM_PARSE_C=y

# REQUIRED FIX for 0x2700,8 on nRF54L15: ECDSA P-384 signature verify via PSA/CRACEN.
CONFIG_MBEDTLS_USE_PSA_CRYPTO=y

CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"

CONFIG_MBEDTLS_PSA_P256M_DRIVER_ENABLED=n
```

`00443f2` already had most of the ECDSA/SHA-384 lines and large Pouch stacks; the **new** lines for this fix are **`PEM_PARSE`**, **`USE_PSA_CRYPTO`**, and explicit **`POUCH_CA_CERT_FILENAME`**.

### 2. `src/saead/mbedtls_config.h`

Keep **parse-only** defines; do **not** put `MBEDTLS_PSA_ACCEL_*` or duplicate `MBEDTLS_ECDSA_C` here (caused confusion; verify belongs in `prj.conf`):

```c
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
```

### 3. `src/cert.c`

- PEM buffers: allocate `size+1`, NUL-terminate, then `mbedtls_x509_crt_parse()` (gateway PEM has no trailing `\0` in BLE buffer).
- Optional but useful: `log_chain_summary()`, `log_verify_failure_details()` (proved failure was **E1 vs root signature**, not missing certs).

### 4. `examples/gateway/boards/frdm_rw612_wifi.conf`

```ini
CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n
```

### Files **not** needed for the fix

- `src/isrgrootx1.der`, `src/isrg_golioth_ca.der` — wrong trust path for this chain; caused RSA-4096 parse errors during experiments.

---

## Diagnostic timeline (what we tried)

| Step | Symptom / error | Lesson |
|------|-----------------|--------|
| Start | `0x2700, 8` with default `goliothrootx1.der` | Verify failed, not “missing CA file” |
| Switch to ISRG root | `0x262e` / `0x3b00` parse errors | ISRG is RSA-4096; wrong anchor + MPI limits |
| Enable `MBEDTLS_RSA_C` + combined CA bundle | Still `0x2700, 8` or parse errors | Chain does not anchor at ISRG for this PEM |
| Enable `CONFIG_MBEDTLS_PEM_PARSE_C` | Chain shows **2 certs** (leaf + E1), still `0x2700, 8` | PEM parsing was necessary but **not sufficient** |
| Log: `Intermediate vs trust anchor only: ret=0x2700 flags=0x8` | Names match; **signature verify** fails on device | Led to PSA crypto fix |
| **`CONFIG_MBEDTLS_USE_PSA_CRYPTO=y`** | **`Server cert verified against CN pouch.golioth.io`** | **Root fix for 0x2700, 8** |
| Gateway still `Loaded builtin server cert` | Node OK; **`Uplink CoAP 4.00`** | Stale server key vs cloud |
| Gateway rebuild `BUILTIN=n` | **`Delivered uplink block 0`** | End-to-end fix |

### OpenSSL cross-check (PC)

Same files as on device:

```powershell
# E1 signed by Golioth Root X1 — trust path OK (ignore leaf expiry for this test)
openssl verify -no_check_time -CAfile src/goliothrootx1.der -untrusted src/gateway/_tmp/e1.pem src/gateway/_tmp/leaf.pem
# → OK

# Bundled leaf in server-prod.pem expired 2025-11-23
openssl x509 -in src/gateway/_tmp/leaf.pem -noout -dates
```

---

## Step-by-step reproduction

### A. One-time: NCS workspace for XIAO (node)

```powershell
cd C:\Users\Brian\ncs
west config manifest.file west-ncs.yml
west update
```

Pouch tree: `C:\Users\Brian\ncs\pouch` (or clone `wischmi2/pouch_xiao_nrf54l15`).

Checkout **`00443f2`** (or branch containing the file changes above), apply uncommitted cert fixes, commit.

### B. Build and flash XIAO node

```powershell
cd C:\Users\Brian\ncs\pouch\examples\ble_gatt

west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild `
  -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

Confirm in `build/zephyr/.config`:

```text
CONFIG_MBEDTLS_PEM_PARSE_C=y
CONFIG_MBEDTLS_USE_PSA_CRYPTO=y
CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"
CONFIG_PSA_WANT_ALG_SHA_384=y
CONFIG_MBEDTLS_ECP_DP_SECP384R1_ENABLED=y
```

Flash (two probes connected — use long flags + env so pyOCD does not prompt for a number):

```powershell
$env:PYOCD_PROBE = "cmsisdap:WYKL7D7EW6T3VCGIXBB6CHE433NVRQIN"
cd C:\Users\Brian\ncs\pouch\examples\ble_gatt
pyocd flash --target nrf54l --uid WYKL7D7EW6T3VCGIXBB6CHE433NVRQIN build\zephyr\zephyr.hex
pyocd reset --target nrf54l --uid WYKL7D7EW6T3VCGIXBB6CHE433NVRQIN
```

If pyOCD still lists probes interactively, pick **0** (Espressif CMSIS-DAP / XIAO), not **1** (J-Link / gateway).

Node still needs LittleFS device creds (`/lfs1/credentials/crt.der`, `key.der`) — see `docs/pouch-node-compiled-ca.md`.

### C. One-time: NCS workspace for FRDM (gateway)

```powershell
cd C:\Users\Brian\ncs
west config manifest.file west-zephyr.yml
west update
west patch apply
# west blobs fetch hal_nxp   # if RW612 build complains about blobs
```

### D. Build and flash gateway

```powershell
cd C:\Users\Brian\ncs\pouch\examples\gateway

west build -p always -b frdm_rw612 `
  --build-dir C:\Users\Brian\ncs\build-gateway-frdm_rw612 `
  --no-sysbuild `
  -- "-DEXTRA_CONF_FILE=boards/frdm_rw612_wifi.conf"
```

Confirm in `build-gateway-frdm_rw612/zephyr/.config`:

```text
# CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN is not set
```

Flash:

```powershell
west flash --build-dir C:\Users\Brian\ncs\build-gateway-frdm_rw612 --dev-id 1060877759
```

Provision WiFi on gateway if needed (`CONFIG_NET_L2_WIFI_SHELL=y`).

### E. Run and verify logs

1. Power gateway; wait for `Golioth CoAP client connected`.
2. Gateway must **not** log `Loaded builtin server cert`.
3. Gateway should log `Loaded gateway server cert chain (... bytes)` from runtime download only.
4. Reset XIAO; let it advertise; gateway connects and pairs.
5. **Node:** `Server cert verified against CN pouch.golioth.io`
6. **Gateway:** `Delivered uplink block 0: path pouch, block_size 1024`
7. **Golioth console:** pouch / stream data visible.

If BLE reconnect fails after a bad session, clear bonds on both sides and pair again (gateway may use 30s security cooldown).

### F. Switch manifest back for node development

```powershell
cd C:\Users\Brian\ncs
west config manifest.file west-ncs.yml
west update
```

---

## Architecture reminder (three cert roles)

| Role | Where | Purpose |
|------|--------|---------|
| **Node identity** | LittleFS `crt.der` + `key.der` | Device auth to gateway/Golioth |
| **Trust anchor** | Compiled `src/goliothrootx1.der` | Verify server chain from gateway |
| **Server chain** | Gateway → BLE (~1478 B PEM) | Leaf pubkey for SAead uplink encryption |

Confusing these or using the wrong anchor (ISRG vs Golioth Root) wastes hours. The gateway **format** (PEM) and **source** (builtin vs Golioth download) are separate from the node **verify crypto** (PSA).

---

## Related docs

- [`docs/pouch-node-compiled-ca.md`](pouch-node-compiled-ca.md) — CA vs device cert, LittleFS, `BUILTIN=n` and `4.00`
- [`docs/frdm-rw612-gateway-build.md`](frdm-rw612-gateway-build.md) — `west-zephyr.yml`, blobs, gateway build
- [`docs/golioth-pouch-since-d161705-and-cert-2700.md`](golioth-pouch-since-d161705-and-cert-2700.md) — upstream Pouch changes after fork split (did **not** contain this fix)

---

## Recommended git commit (fork)

After verifying on hardware, commit on top of `00443f2`:

```text
fix(ble_gatt): verify Golioth server PEM chain on nRF54L15

Enable MBEDTLS_PEM_PARSE_C and MBEDTLS_USE_PSA_CRYPTO so the node can
parse the gateway PEM chain and verify ECDSA-SHA384 (secp384r1) signatures
against goliothrootx1.der. Add PEM NUL handling and cert debug logs.

Gateway WiFi board already uses CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n.
```

---

## Quick “what was the main thing?”

**For `0x2700, 8` on the XIAO:** `CONFIG_MBEDTLS_USE_PSA_CRYPTO=y` (+ `CONFIG_MBEDTLS_PEM_PARSE_C=y` + `goliothrootx1.der`).

**For data in Golioth:** gateway `CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n` and rebuild flash.

Both were required for what you see working now.

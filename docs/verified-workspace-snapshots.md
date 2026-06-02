# Verified workspace snapshots

This file records **reproducible build environments** we have actually checked out and
verified on `C:/ncs_pouch_soil`. Use it when debugging `0x2700, 8`, comparing ROM
strings to unit 2/3/4, or answering “what exact NCS/Zephyr/Pouch did we have then?”

Related docs:

- [`v0.1.0-build-steps.md`](v0.1.0-build-steps.md) — historical runbook **frozen at the commit where it was added** (do not edit that file for new verifications; record results here only)
- [`build-summary.md`](build-summary.md) — one-page version/build/`0x2700`/unit 2 table
- [`2700-8-debug.md`](2700-8-debug.md) — branch history and `0x2700, 8` context
- [`pouch-node-compiled-ca.md`](pouch-node-compiled-ca.md) — cert failures A/B

**Important:** Git stores **Pouch** at a commit SHA. **West** only pins **tags/branches**
in `west-ncs.yml`; sibling repos (`nrf`, `zephyr`, …) resolve when you run
`west update`. To reproduce a snapshot, always:

1. Check out the **Pouch commit** listed below.
2. Run **`west update`** from `C:/ncs_pouch_soil`.
3. Record **`west list`** output (copy into a new snapshot block if re-verifying later).
4. Build with the **documented command** and capture **ROM strings** from `zephyr.bin`.

---

## How to add a new snapshot

After a successful or failed verification:

```powershell
# 1) Pouch commit
cd C:\ncs_pouch_soil\pouch
git checkout <SHA-or-branch>
git show -s --oneline --decorate HEAD
git describe --tags --always

# 2) West manifest at this commit
git show HEAD:west-ncs.yml

# 3) Align workspace
cd C:\ncs_pouch_soil
west update
west list | Sort-Object

# 4) Per-repo SHAs (quick)
foreach ($p in @('nrf','zephyr','mcuboot','mbedtls','pouch')) {
  Write-Host "$p : $(git -C $p rev-parse HEAD) $(git -C $p describe --tags --always 2>$null)"
}

# 5) Build (example)
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b <board> --pristine   # or --no-sysbuild for XIAO app-only

# 6) ROM strings
python -c @"
from pathlib import Path
b=Path(r'build/ble_gatt/zephyr/zephyr.bin')
if not b.exists(): b=Path(r'build/zephyr/zephyr.bin')
s=b.read_bytes().decode('latin1',errors='ignore')
for n in ['nRF Connect SDK','Zephyr OS','Pouch SDK','Booting Pouch']:
    i=s.find(n)
    if i>=0: print(n, s[i:i+85].split(chr(0))[0])
print('size', b.stat().st_size if b.exists() else 'missing')
"@
```

Copy the results into a new **Snapshot** section below (keep **chronological** order: oldest first).

### Snapshot template (copy for each verification)

```markdown
## Snapshot: `<short-name>` @ `<pouch-SHA>` (verified YYYY-MM-DD)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK / ❌ Build failed / ⚠️ Build OK, wrong ROM |
| **Verified by** | |
| **Pouch checkout** | `<SHA>` — `<subject>` |
| **git describe** | |
| **Branch / note** | detached / `soil_sensor` / tag `v0.1.0` |
| **Parent of doc-only commits** | |

### West manifest (`west-ncs.yml` at this Pouch commit)

\`\`\`yaml
(paste west-ncs.yml)
\`\`\`

### Workspace after `west update`

| Project | Revision (west list) | Git SHA | `git describe` |
|---------|----------------------|---------|----------------|
| nrf | | | |
| zephyr | | | |
| mcuboot | | | |
| mbedtls | | | |
| pouch | | | |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `manifest.path=pouch`, `manifest.file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `examples/ble_gatt` |
| **Command** | |
| **Board** | |
| **Sysbuild** | yes / `--no-sysbuild` |
| **Result** | |
| **`zephyr.bin` size** | |
| **Key outputs** | |

### ROM banner strings (from `zephyr.bin`)

\`\`\`text
(paste nRF Connect / Zephyr / Pouch SDK lines)
\`\`\`

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | | |
| `nrf54l15dk/nrf54l15/cpuapp` | | |

### Notes

- …
```

---

## Snapshot: `v0.1.0` tag @ `73662de` (equivalent to `33412d8` for build)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ Not re-verified on 2026-05-30; **expected same as `33412d8`** for west + build |
| **Pouch checkout** | `73662de` — tag **`v0.1.0`** |
| **git describe** | `v0.1.0` |
| **Difference vs `33412d8`** | No `docs/v0.1.0-build-steps.md` or `scripts/build_ble_gatt_v010.ps1` |

Use **`33412d8` snapshot** for west SHAs and ROM strings unless you need the bare tag tree only.

---

## Snapshot 33412d8 — v0.1.0 runbook commit (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK (`nrf52840dk`) |
| **Pouch checkout** | `33412d8` — *Add reproducible v0.1.0 build workflow docs and script.* |
| **git describe** | `v0.1.0-1-g33412d8` |
| **Branch / note** | Commit is on `pouch_xiao_v010`, `soil_sensor`, `main`, `nrf54l15`; parent is tag tree |
| **Parent** | `73662de` (`v0.1.0` tag) — **Pouch app/SDK source = tag + docs/script only** |

### West manifest (`west-ncs.yml` at `33412d8`)

```yaml
manifest:
  projects:
    - name: nrf
      revision: v3.0.1
      url: http://github.com/nrfconnect/sdk-nrf
      import:
        name-allowlist:
          - nrf
          - zephyr
          - cmsis
          - hal_nordic
          - nrfxlib
          - mbedtls
          - mcuboot
          - segger
          - tfm-mcuboot
          - oberon-psa-crypto
          - trusted-firmware-m
          - littlefs
          - zcbor
  self:
    path: pouch
```

No separate `zephyr` or `golioth-firmware-sdk` project — Zephyr comes from **nrf import**.

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.0.1` | `9eb5615da6` | `v3.0.1` |
| **zephyr** | `v4.0.99-ncs1-1` | `77f865b8f8d` | `v4.0.99-ncs1-1` |
| **mcuboot** | `v2.1.0-ncs5-1` | (see `west list`) | |
| **mbedtls** | `v3.6.3-ncs1-1` | (see `west list`) | |
| **pouch** | — | `33412d8` | `v0.1.0-1-g33412d8` |

Full `west list` after update (representative):

```text
nrf          v3.0.1
zephyr       v4.0.99-ncs1-1
mcuboot      v2.1.0-ncs5-1
mbedtls      v3.6.3-ncs1-1
```

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `pouch/examples/ble_gatt` |
| **Command** | `west build -b nrf52840dk/nrf52840 --pristine` |
| **Board** | `nrf52840dk/nrf52840` |
| **Sysbuild** | **yes** (default; MCUboot + `merged.hex`) |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **356,908** bytes (`build/ble_gatt/zephyr/zephyr.bin`) |
| **Key outputs** | `merged.hex`, `dfu_application.zip` |

### ROM banner strings (from `zephyr.bin`)

```text
*** Using nRF Connect SDK v3.0.1-9eb5615da66b ***
*** Using Zephyr OS v4.0.99-77f865b8f8d0 ***
Pouch SDK Version: v0.1.0-1-g33412d8f5253
*** Booting Pouch BLE GATT Example v1.0.0-33412d8f5253 ***
```

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **No** | `west build` fails: board not in this Zephyr tree |
| `nrf54l15dk/nrf54l15/cpuapp` | **Yes** | Same SoC family; different pinout than XIAO |

### Reproduce

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout 33412d8

cd C:\ncs_pouch_soil
west update

$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b nrf52840dk/nrf52840 --pristine
```

Or: `scripts/build_ble_gatt_v010.ps1` (defaults to `nrf52840dk`).

### Notes

- **`33412d8` ≠ XIAO field stack.** It is **release `v0.1.0` + NCS 3.0.1** only. For XIAO on NCS **3.2.3** after the main merge, see **`ddba5e6`** snapshot below (matches runbook §8–9 intent; runbook file itself stays at its original commit text).
- **Unit 2 golden ROM** used **`v0.1.0-221-gd8f79b3`** and **Zephyr `v4.3.0`**, not this **4.0.99** stack.
- Re-running `west update` years later should yield the **same manifest revisions**; record SHAs again
  if tags are ever moved (unlikely for `v3.0.1`).

---

## Snapshot ddba5e6 — merge `origin/main` into nrf54l15 (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK (XIAO app-only) |
| **Pouch checkout** | `ddba5e6` — *Merge remote-tracking branch `origin/main` into nrf54l15* |
| **git describe** | `v0.1.0-213-gddba5e6` |
| **Branch / note** | Detached HEAD; **first commit on soil line with upstream Pouch + NCS 3.2.3**; parent includes `33412d8` → `73662de` on other parent of merge |
| **Next commit on XIAO line** | `f1d32b4` (first XIAO-specific doc) |

### West manifest (`west-ncs.yml` at `ddba5e6`)

```yaml
manifest:
  projects:
    - name: nrf
      revision: v3.2.3
      url: http://github.com/nrfconnect/sdk-nrf
      import:
        name-allowlist:
          - cmsis_6
          - hal_nordic
          - libmetal
          - littlefs
          - mbedtls
          - mcuboot
          - nrf
          - nrfxlib
          - oberon-psa-crypto
          - open-amp
          - segger
          - tfm-mcuboot
          - trusted-firmware-m
          - zcbor
          - zephyr
    - name: golioth
      path: modules/lib/golioth-firmware-sdk
      revision: d703b1f8805c7584a44dabc31bdf09164637d888
      url: https://github.com/golioth/golioth-firmware-sdk.git
  self:
    path: pouch
```

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.2.3` | `6f8485d289` | `v3.2.3` |
| **zephyr** | `ncs-v3.2.3` | `c4c75f71e70` | `ncs-v3.2.3` |
| **golioth-firmware-sdk** | `d703b1f8805c7584a44dabc31bdf09164637d888` | `d703b1f` | `d703b1f` |
| **mcuboot** | `ncs-v3.2.3` | (see `west list`) | |
| **mbedtls** | `ncs-v3.2.3` | (see `west list`) | |
| **pouch** | — | `ddba5e6` | `v0.1.0-213-gddba5e6` |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `pouch/examples/ble_gatt` |
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Board** | `xiao_nrf54l15/nrf54l15/cpuapp` |
| **Sysbuild** | **no** (`--no-sysbuild`; sysbuild/MCUboot fails on XIAO per runbook) |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **332,172** bytes (`build/zephyr/zephyr.bin`) |
| **Key outputs** | `build/zephyr/zephyr.elf`, `zephyr.hex` (no `merged.hex` without sysbuild) |

### ROM banner strings (from `zephyr.bin`)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-213-gddba5e672d86
*** Booting Pouch BLE GATT Example v1.0.0-ddba5e672d86 ***
```

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **Yes** | First verified snapshot with XIAO in workspace |
| `nrf54l15dk/nrf54l15/cpuapp` | **Yes** | |

### Reproduce

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout ddba5e6

cd C:\ncs_pouch_soil
west update

$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- **Large merge:** ~329 files vs `v0.1.0` tag — gateway, HTTP, GATT refactor, ESP-IDF port, CI, etc.
- **Not `v0.1.0` + 3.0.1:** This is the stack described in §8–9 of [`v0.1.0-build-steps.md`](v0.1.0-build-steps.md) on `soil_sensor` (that file is **not updated** when adding snapshots here).
- **Zephyr `4.2.99`** here vs unit 2 golden ROM **`4.3.0`** — relevant for `0x2700, 8` comparisons.

---

## Snapshot f1d32b4 — first XIAO build doc (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK (XIAO app-only) |
| **Pouch checkout** | `f1d32b4` — *Document XIAO nRF54L15 app-only build path.* |
| **git describe** | `v0.1.0-214-gf1d32b4` |
| **Branch / note** | Detached HEAD; **first XIAO-focused check-in** (docs + build script only) |
| **Parent** | `ddba5e6` |
| **Pouch diff vs parent** | `docs/v0.1.0-build-steps.md`, `scripts/build_ble_gatt_v010.ps1` only — **no** `ble_gatt` source or `west-ncs.yml` change |

### West manifest (`west-ncs.yml` at `f1d32b4`)

Same as **`ddba5e6`** (NCS `v3.2.3` + Golioth SDK `d703b1f`).

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.2.3` | `6f8485d289` | `v3.2.3` |
| **zephyr** | `ncs-v3.2.3` | `c4c75f71e70` | `ncs-v3.2.3` |
| **golioth-firmware-sdk** | `d703b1f8805c7584a44dabc31bdf09164637d888` | `d703b1f` | `d703b1f` |
| **pouch** | — | `f1d32b4` | `v0.1.0-214-gf1d32b4` |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `pouch/examples/ble_gatt` |
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Board** | `xiao_nrf54l15/nrf54l15/cpuapp` |
| **Sysbuild** | **no** |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **332,172** bytes (`build/zephyr/zephyr.bin`) — **same size as `ddba5e6`** |
| **Key outputs** | `build/zephyr/zephyr.elf`, `zephyr.hex` |

### ROM banner strings (from `zephyr.bin`)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-214-gf1d32b41d2c1
*** Booting Pouch BLE GATT Example v1.0.0-f1d32b41d2c1 ***
```

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **Yes** | |
| `nrf54l15dk/nrf54l15/cpuapp` | **Yes** | |

### Reproduce

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout f1d32b4

cd C:\ncs_pouch_soil
west update

$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- Extends **`v0.1.0-build-steps.md`** at this commit with §8–9 (XIAO / NCS 3.2.3 / `--no-sysbuild`). That file is **historical at `f1d32b4`** — do not edit it when adding later snapshots.
- **`scripts/build_ble_gatt_v010.ps1`** at this commit adds XIAO-oriented defaults/help (see commit diff).
- Firmware behavior matches **`ddba5e6`** except **Pouch git describe** strings in ROM.

---

## Snapshot b965e36 — XIAO soil sensor ADC uplink (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK (XIAO app-only) |
| **Pouch checkout** | `b965e36` — *Add XIAO soil sensor ADC uplink support.* |
| **git describe** | `v0.1.0-215-gb965e36` |
| **Branch / note** | Detached HEAD; **first soil-moisture firmware** on XIAO line |
| **Parent** | `f1d32b4` |
| **Pouch diff vs parent** | `examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay`, `prj.conf` (+`CONFIG_ADC=y`), `src/main.c` (+soil uplink) |

### West manifest (`west-ncs.yml` at `b965e36`)

Same as **`ddba5e6` / `f1d32b4`** (NCS `v3.2.3` + Golioth SDK `d703b1f`).

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.2.3` | `6f8485d289` | `v3.2.3` |
| **zephyr** | `ncs-v3.2.3` | `c4c75f71e70` | `ncs-v3.2.3` |
| **golioth-firmware-sdk** | `d703b1f8805c7584a44dabc31bdf09164637d888` | `d703b1f` | `d703b1f` |
| **pouch** | — | `b965e36` | `v0.1.0-215-gb965e36` |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `pouch/examples/ble_gatt` |
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Board** | `xiao_nrf54l15/nrf54l15/cpuapp` |
| **Sysbuild** | **no** |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **336,532** bytes (`build/zephyr/zephyr.bin`) — **+4,360 B vs `f1d32b4`** |
| **FLASH / RAM** | 23.01% FLASH, 35.40% RAM (per build log) |

### `prj.conf` highlights (vs later soil commits)

| Option | Value at `b965e36` |
|--------|-------------------|
| `CONFIG_ADC` | **y** |
| `CONFIG_GOLIOTH_SETTINGS` | **y** |
| `CONFIG_GOLIOTH_OTA` | **y** |

(OTA/settings turned **off** later at `00443f2`.)

### ROM banner strings (from `zephyr.bin`)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-215-gb965e36
*** Booting Pouch BLE GATT Example v1.0.0-b965e36 ***
```

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **Yes** | Overlay adds `io-channels` for SAADC |
| `nrf54l15dk/nrf54l15/cpuapp` | **Yes** | |

### Reproduce

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout b965e36

cd C:\ncs_pouch_soil
west update

$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- Overlay at this commit is **12 lines** (ADC on `zephyr,user`); LittleFS overlay comes in **`de25234`**.
- Same NCS/Zephyr SHAs as **`f1d32b4`**; behavior change is **application + ADC** only.

---

## Snapshot de25234 — XIAO LittleFS mount overlay (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ✅ Build OK (XIAO app-only) |
| **Pouch checkout** | `de25234` — *Add XIAO LittleFS mount overlay.* |
| **git describe** | `v0.1.0-216-gde25234` |
| **Branch / note** | Detached HEAD; **device cert storage** on `/lfs1` via DTS overlay |
| **Parent** | `b965e36` |
| **Pouch diff vs parent** | `examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay` only (+15 lines: `fstab` / `lfs1` automount) |

### West manifest (`west-ncs.yml` at `de25234`)

Same as **`b965e36`** (NCS `v3.2.3` + Golioth SDK `d703b1f`).

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.2.3` | `6f8485d289` | `v3.2.3` |
| **zephyr** | `ncs-v3.2.3` | `c4c75f71e70` | `ncs-v3.2.3` |
| **golioth-firmware-sdk** | `d703b1f8805c7584a44dabc31bdf09164637d888` | `d703b1f` | `d703b1f` |
| **pouch** | — | `de25234` | `v0.1.0-216-gde25234` |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build

| Item | Value |
|------|--------|
| **App** | `pouch/examples/ble_gatt` |
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Board** | `xiao_nrf54l15/nrf54l15/cpuapp` |
| **Sysbuild** | **no** |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **337,776** bytes — **+1,244 B vs `b965e36`** |
| **FLASH / RAM** | 23.10% FLASH, 35.65% RAM |

### `prj.conf` highlights

Unchanged vs **`b965e36`**: `CONFIG_ADC=y`, `GOLIOTH_SETTINGS=y`, `GOLIOTH_OTA=y`.

### ROM banner strings (from `zephyr.bin`)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-216-gde25234
*** Booting Pouch BLE GATT Example v1.0.0-de25234 ***
```

Mount path **`/lfs1`** present in firmware (LittleFS for `/lfs1/credentials/`).

### Boards

| Board | Available? | Notes |
|-------|------------|--------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **Yes** | Overlay: ADC + LittleFS automount |
| `nrf54l15dk/nrf54l15/cpuapp` | **Yes** | |

### Reproduce

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout de25234

cd C:\ncs_pouch_soil
west update

$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- Required for runtime **device cert** upload to `/lfs1/credentials/` (see [`pouch-node-compiled-ca.md`](pouch-node-compiled-ca.md)).
- Same NCS/Zephyr SHAs as **`b965e36`**; size bump is **LittleFS + partition** in image only.

---

## Snapshot 0c53b2a — XIAO soil bring-up (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `0c53b2a` — *Document XIAO soil sensor bring-up.* |
| **git describe** | `v0.1.0-217-g0c53b2a` |
| **Parent** | `de25234` |

### Pouch diff vs `de25234` (this commit)

| Path | Change |
|------|--------|
| `src/cert.c`, `src/gateway/cert.c`, `src/transport/endpoints/server_cert.c` | server cert verify / debug logging |
| `examples/ble_gatt/prj.conf` | secp384 / SHA-384 explicit; **`GOLIOTH_OTA=y`**, **`GOLIOTH_SETTINGS=y`** |
| `examples/ble_gatt/src/main.c` | soil sensor node logic |
| `examples/gateway/CMakeLists.txt`, `src/gateway/Kconfig` | gateway cert integration |
| `docs/frdm-rw612-gateway-build.md`, `pouch-node-compiled-ca.md`, `xiao-nrf54l15-locked-recovery.md`, `xiao-vs-frdm-setup.md` | bring-up docs |

First commit introducing **`cert.c`** + secp384 `prj.conf` on the XIAO line.

### West manifest

Same as **`de25234`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `0c53b2a` | `v0.1.0-217-g0c53b2a` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### `prj.conf` highlights

| Option | Value |
|--------|--------|
| secp384 / `CONFIG_ADC` | **y** |
| `CONFIG_GOLIOTH_SETTINGS` / `OTA` | **y** / **y** |
| `CONFIG_MBEDTLS_X509_REMOVE_INFO` | **not set** (link fail without workaround) |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ Link error: `mbedtls_x509_crt_info` in `cert.c` |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **347,808** bytes |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-217-g0c53b2a
*** Booting Pouch BLE GATT Example v1.0.0-0c53b2a ***
```

### Reproduce

```powershell
git checkout 0c53b2a
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- **`cert.c`** debug paths require **`CONFIG_MBEDTLS_X509_REMOVE_INFO=n`** until committed at **`7686423`**.
- OTA on → larger images than post-`00443f2` field stack.

---

## Snapshot d42c790 — gateway cert path (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `d42c790` — *Record working soil sensor gateway path.* |
| **git describe** | `v0.1.0-218-gd42c790` |
| **Parent** | `0c53b2a` |

### Pouch diff vs `0c53b2a` (this commit)

| Path | Change |
|------|--------|
| `docs/frdm-rw612-gateway-build.md`, `pouch-node-compiled-ca.md`, `xiao-vs-frdm-setup.md` | gateway cert path documentation |
| `examples/ble_gatt/src/main.c` | node tweaks |
| `examples/gateway/src/main.c`, `src/gateway/cert.c`, `src/gateway/uplink.c`, `src/gateway/bt/device_cert.c` | gateway cert chain / uplink path |
| `examples/gateway/boards/frdm_rw612_wifi.conf` | gateway board config |

### West manifest

Same as **`0c53b2a`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `d42c790` | `v0.1.0-218-gd42c790` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **348,388** bytes (**+580 B vs `0c53b2a` workaround**) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-218-gd42c790
*** Booting Pouch BLE GATT Example v1.0.0-d42c790 ***
```

### Reproduce

```powershell
git checkout d42c790
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- Documents the **working FRDM gateway → XIAO node** cert path used in early field bring-up.

---

## Snapshot 3ae0ff9 — multi-bond + button uplink (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `3ae0ff9` — *Support manual node uplinks and multiple XIAO bonds.* |
| **git describe** | `v0.1.0-219-g3ae0ff9` |
| **Parent** | `d42c790` |

### Pouch diff vs `d42c790` (this commit)

| Path | Change |
|------|--------|
| `examples/ble_gatt/src/main.c` | manual uplink trigger, multi-bond support |
| `examples/gateway/prj.conf` | gateway bonding config |

### West manifest

Same as **`d42c790`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `3ae0ff9` | `v0.1.0-219-g3ae0ff9` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **348,860** bytes (**+472 B vs `d42c790` workaround**) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-219-g3ae0ff9
*** Booting Pouch BLE GATT Example v1.0.0-3ae0ff9 ***
```

### Reproduce

```powershell
git checkout 3ae0ff9
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- OTA/settings still **ON** (pre-`00443f2` stack).

---

## Snapshot 1639474 — BLE scan cooldown (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `1639474` — *Add BLE security retry cooldown.* |
| **git describe** | `v0.1.0-220-g1639474` |
| **Parent** | `3ae0ff9` |

### Pouch diff vs `3ae0ff9` (this commit)

| Path | Change |
|------|--------|
| `examples/gateway/src/main.c` | gateway integration |
| `include/pouch/gateway/bt/scan.h`, `src/gateway/bt/scan.c` | BLE scan cooldown logic |
| `src/gateway/Kconfig` | cooldown Kconfig |

**Node firmware unchanged** — XIAO build identical to **`3ae0ff9`**.

### West manifest

Same as **`3ae0ff9`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `1639474` | `v0.1.0-220-g1639474` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **348,860** bytes (same as `3ae0ff9`) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-220-g1639474
*** Booting Pouch BLE GATT Example v1.0.0-1639474 ***
```

### Reproduce

```powershell
git checkout 1639474
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- **Gateway-only** change; node `zephyr.bin` matches **`3ae0ff9`**.

---

## Snapshot d8f79b3 — unit 2 era / OpenOCD recovery docs (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails**; ✅ **workaround build** (see below) |
| **Pouch checkout** | `d8f79b3` — *Save XIAO OpenOCD recovery patch.* |
| **git describe** | `v0.1.0-221-gd8f79b3` |
| **Branch / note** | **Unit 2 golden source era** (`v0.1.0-221`); commit itself adds recovery **docs/patch only** |
| **Parent chain since `de25234`** | `0c53b2a` (bring-up: `cert.c`, secp384 `prj.conf`, gateway), `3ae0ff9`, `1639474`, `d42c790`, then `d8f79b3` |

### West manifest (`west-ncs.yml` at `d8f79b3`)

Same as **`de25234`** (NCS `v3.2.3` + Golioth SDK `d703b1f`). **No** explicit Zephyr `v4.3.0` project (unit 2 ROM **4.3.0** likely used a **manual Zephyr pin** when that image was built).

### Workspace after `west update` (2026-05-30)

| Project | Revision (`west list`) | Git SHA | `git describe` |
|---------|------------------------|---------|----------------|
| **nrf** | `v3.2.3` | `6f8485d289` | `v3.2.3` |
| **zephyr** | `ncs-v3.2.3` | `c4c75f71e70` | `ncs-v3.2.3` |
| **golioth-firmware-sdk** | `d703b1f8805c7584a44dabc31bdf09164637d888` | `d703b1f` | `d703b1f` |
| **pouch** | — | `d8f79b3` | `v0.1.0-221-gd8f79b3` |

### Tooling

| Item | Value |
|------|--------|
| **Workspace root** | `C:/ncs_pouch_soil` |
| **`.west/config`** | `path=pouch`, `file=west-ncs.yml` |
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### `prj.conf` highlights (tree at `d8f79b3`)

| Option | Value |
|--------|--------|
| secp384 / SHA-384 | **explicit** (server cert verify path) |
| `CONFIG_ADC` | **y** |
| `CONFIG_GOLIOTH_SETTINGS` | **y** |
| `CONFIG_GOLIOTH_OTA` | **y** |
| `CONFIG_MBEDTLS_X509_REMOVE_INFO` | **not set** (defaults remove x509 info symbols) |

`src/cert.c` calls `mbedtls_x509_crt_info` / `mbedtls_x509_crt_verify_info` (from `0c53b2a` bring-up).

### Build A — as checked in (fails)

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ **Link error** — `undefined reference to mbedtls_x509_crt_info` / `mbedtls_x509_crt_verify_info` in `cert.c` |

### Build B — workaround (not in git at this commit; for link only)

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **348,860** bytes |
| **FLASH / RAM** | 23.86% FLASH, 35.72% RAM |

Permanent fix on branch: **`00443f2`** sets `GOLIOTH_OTA=n` and adds `CONFIG_MBEDTLS_X509_REMOVE_INFO=n` to `prj.conf`.

### ROM banner strings (Build B / workaround)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-221-gd8f79b3
*** Booting Pouch BLE GATT Example v1.0.0-d8f79b3 ***
```

Still **Zephyr 4.2.99**, not unit 2 ROM **4.3.0**.

### Boards

| Board | Available? |
|-------|------------|
| `xiao_nrf54l15/nrf54l15/cpuapp` | **Yes** |

### Reproduce (as-is — expect link fail)

```powershell
cd C:\ncs_pouch_soil\pouch
git checkout d8f79b3
cd C:\ncs_pouch_soil
west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- **`firmware/unit2_golden.bin`** on `soil_sensor` tip matches this **Pouch describe** (`-221`) but ROM **Zephyr 4.3.0** — do not assume plain `west update` on `d8f79b3` reproduces unit 2 flash bytes.
- OpenOCD recovery: `docs/xiao-nrf54l15-openocd-recovery.patch` added at this commit.
- Compare **`0x2700, 8`** against Build B or golden bin, not a failed Build A.

---

## Snapshot 00443f2 — stabilize soil uplinks (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (same `cert.c` link as `d8f79b3`); ✅ **workaround build** |
| **Pouch checkout** | `00443f2` — *Stabilize XIAO soil sensor uplinks.* |
| **git describe** | `v0.1.0-222-g00443f2` |
| **Parent** | `d8f79b3` |

### Pouch diff vs `d8f79b3` (this commit)

| Area | Change |
|------|--------|
| `prj.conf` | `GOLIOTH_SETTINGS=n`, `GOLIOTH_OTA=n`; `POUCH_*_STACK_SIZE=8192` |
| `CMakeLists.txt` | `fw_update.c` only if `CONFIG_GOLIOTH_OTA=y` |
| `main.c` | uplink stability tweaks |
| docs | `pouch-node-compiled-ca.md`, `v0.1.0-build-steps.md`, `xiao-vs-frdm-setup.md` |

**Not in `prj.conf` yet:** `CONFIG_MBEDTLS_X509_REMOVE_INFO=n` (added at **`7686423`**).

### West manifest

Same as **`d8f79b3`** (NCS `v3.2.3`, Golioth SDK `d703b1f`).

### Workspace after `west update` (2026-05-30)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `00443f2` | `v0.1.0-222-g00443f2` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### `prj.conf` highlights

| Option | Value |
|--------|--------|
| `CONFIG_GOLIOTH_SETTINGS` / `OTA` | **n** / **n** |
| `CONFIG_POUCH_THREAD_STACK_SIZE` | **8192** |
| `CONFIG_POUCH_UPLINK_PROCESSING_STACK_SIZE` | **8192** |
| secp384 / `CONFIG_ADC` | unchanged vs `d8f79b3` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ Link error: `mbedtls_x509_crt_info` / `mbedtls_x509_crt_verify_info` |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **340,476** bytes (**−8,384 B vs `d8f79b3` workaround** — OTA off) |
| **FLASH / RAM** | 23.28% FLASH, **41.71% RAM** (larger stacks) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-222-g00443f2
*** Booting Pouch BLE GATT Example v1.0.0-00443f2 ***
```

### Reproduce (as-is — expect link fail)

```powershell
git checkout 00443f2
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild
```

### Notes

- Field-stable config (**OTA off**, 8K stacks) first appears here; **`MBEDTLS_X509_REMOVE_INFO=n` committed later** at `7686423`.
- **`soil_sensor` tip** field `prj.conf` is this commit plus crypto/Kconfig from `7686423`.

---

## Snapshot b539f2f — soil moisture percent + debug Mate (verified 2026-05-30)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `b539f2f` — *Add XIAO nRF54L15 debug support and soil moisture percent* |
| **git describe** | `v0.1.0-223-gb539f2f` |
| **Parent** | `00443f2` |

### Pouch diff vs `00443f2` (this commit)

| Path | Change |
|------|--------|
| `examples/ble_gatt/src/main.c` | soil **percent** uplink / debug logging |
| `docs/xiao-debug-mate-debugging.md` | new |
| `.vscode/launch.json`, `.vscode/tasks.json` | OpenOCD/debug tasks |
| `.gitignore` | allow tracked `.vscode` launch/tasks |

`prj.conf` / `west-ncs.yml` **unchanged** vs `00443f2`.

### West manifest

NCS **`v3.2.3`** + Golioth SDK **`d703b1f`** (same SHAs as prior 3.2.3 snapshots).

### Workspace after `west update` (2026-05-30)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `b539f2f` | `v0.1.0-223-gb539f2f` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **340,552** bytes (**+76 B vs `00443f2` workaround** — percent in JSON) |
| **FLASH / RAM** | 23.29% FLASH, 41.71% RAM |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-223-gb539f2f
*** Booting Pouch BLE GATT Example v1.0.0-b539f2f ***
```

Uplink JSON template includes **`soil_moisture_percent`** (not only mv/raw).

Soil JSON in ROM includes **percent** field (vs raw-only at `b965e36`).

### Reproduce

```powershell
git checkout b539f2f
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- First snapshot with **Cursor/VS Code OpenOCD** tasks in-repo.
- Still needs **`CONFIG_MBEDTLS_X509_REMOVE_INFO=n` in git** until `7686423`.

---

## Snapshot 10f6244 — 0x2700 troubleshooting docs (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `10f6244` — *Combine Pouch server cert 0x2700 troubleshooting into one doc.* |
| **git describe** | `v0.1.0-224-g10f6244` |
| **Parent** | `b539f2f` |

### Pouch diff vs `b539f2f` (this commit)

| Path | Change |
|------|--------|
| `docs/pouch-node-compiled-ca.md` | consolidated `0x2700` / Failure A troubleshooting (+233 / −59 lines) |

**Firmware / manifest unchanged** vs `b539f2f`.

### West manifest

Same as **`b539f2f`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `10f6244` | `v0.1.0-224-g10f6244` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **340,552** bytes (same as `b539f2f` workaround) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-224-g10f6244
*** Booting Pouch BLE GATT Example v1.0.0-10f6244 ***
```

### Reproduce

```powershell
git checkout 10f6244
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- **Docs-only** — first commit after last likely-good field firmware (`b539f2f`).

---

## Snapshot 628238f — device-cert 4.12 expiry docs (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ⚠️ **As-is build fails** (`cert.c` link); ✅ **workaround build** |
| **Pouch checkout** | `628238f` — *Document device-cert 4.12 expiry failure and known-good sync reference.* |
| **git describe** | `v0.1.0-225-g628238f` |
| **Parent** | `10f6244` |

### Pouch diff vs `10f6244` (this commit)

| Path | Change |
|------|--------|
| `docs/pouch-node-compiled-ca.md` | Failure B (`4.12` expiry), known-good sync reference (+218 lines) |

**Firmware / manifest unchanged** vs `10f6244`.

### West manifest

Same as **`b539f2f`** (NCS **`v3.2.3`**, Golioth SDK **`d703b1f`**).

### Workspace after `west update` (2026-06-01)

| Project | Git SHA | `git describe` |
|---------|---------|----------------|
| **nrf** | `6f8485d289` | `v3.2.3` |
| **zephyr** | `c4c75f71e70` | `ncs-v3.2.3` |
| **pouch** | `628238f` | `v0.1.0-225-g628238f` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### Build A — as checked in

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ `mbedtls_x509_crt_info` link error |

### Build B — workaround

| Item | Value |
|------|--------|
| **Command** | `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` |
| **Result** | ✅ Success |
| **`zephyr.bin` size** | **340,552** bytes (same as `10f6244` / `b539f2f` workaround) |

### ROM banner strings (Build B)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-225-g628238f
*** Booting Pouch BLE GATT Example v1.0.0-628238f ***
```

### Reproduce

```powershell
git checkout 628238f
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"
```

### Notes

- **Docs-only** commit; node binary matches **`b539f2f`** workaround build.

---

## Snapshot 7686423 — Zephyr 4.3.0 pin + field scripts (verified 2026-06-01)

| Field | Value |
|-------|--------|
| **Status** | ❌ **Build A fails** (Kconfig loop on real NCS 3.3.0 + Zephyr 4.3.0); ✅ **Build B OK** (stale Zephyr 4.2.99 + tip `prj.conf`) |
| **Pouch checkout** | `7686423` — *Pin Zephyr 4.3.0 stack and field scripts for XIAO soil sensor.* |
| **git describe** | `v0.1.0-226-g7686423` |
| **Parent** | `628238f` |
| **Branch / note** | **`soil_sensor` tip** at batch verification |

### Pouch diff vs `628238f` (this commit)

| Path | Change |
|------|--------|
| `west-ncs.yml` | NCS **`v3.3.0`**, explicit **Zephyr `v4.3.0`** project |
| `examples/ble_gatt/prj.conf` | `CONFIG_MBEDTLS_LEGACY_CRYPTO_C=y`, `CONFIG_MBEDTLS_X509_REMOVE_INFO=n`, `CONFIG_MBEDTLS_USE_PSA_CRYPTO=y` |
| `examples/ble_gatt/prj.conf.golden`, `prj.conf.soil*.bak` | field config references |
| `firmware/unit2_golden.bin` | known-good full flash image |
| `patches/zephyr-v4.3.0-disable-mbedtls-builtin.patch`, `scripts/apply-zephyr-433-patch.ps1` | Zephyr 4.3.0 Kconfig workaround |
| `scripts/build_unit3_field.ps1`, `flash_unit2_golden.ps1`, `flash_xiao_full.ps1` | field build/flash helpers |
| `west-ncs-test323.yml`, `west-ncs.yml.bak323` | alternate manifest for 3.2.3 comparison |
| `.vscode/tasks.json`, board YAML tweaks | tooling |

**Node firmware vs `b539f2f`:** only **`prj.conf`** + **`west-ncs.yml`** changed (no `cert.c` / `main.c` in `b539f2f..7686423`).

### West manifest (`west-ncs.yml` at this commit)

NCS **`v3.3.0`** + explicit **Zephyr `v4.3.0`** + Golioth SDK **`d703b1f`**.

### Workspace after `west update` (Build A intent — May 2026)

| Project | Expected revision | Notes |
|---------|-------------------|--------|
| **nrf** | `v3.3.0` | manifest bump |
| **zephyr** | `v4.3.0` | explicit second project |
| **pouch** | `7686423` | `v0.1.0-226-g7686423` |

### Tooling

| Item | Value |
|------|--------|
| **`ZEPHYR_SDK_INSTALL_DIR`** | `C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk` |

### `prj.conf` highlights (committed at this commit)

| Option | Value |
|--------|--------|
| `CONFIG_MBEDTLS_LEGACY_CRYPTO_C` | **y** |
| `CONFIG_MBEDTLS_USE_PSA_CRYPTO` | **y** |
| `CONFIG_MBEDTLS_X509_REMOVE_INFO` | **n** (fixes `cert.c` link when OTA off) |
| `CONFIG_GOLIOTH_SETTINGS` / `OTA` | **n** / **n** (from `00443f2` chain) |
| secp384 / `CONFIG_ADC` | unchanged vs `00443f2` |

### Build A — manifest 3.3.0 + Zephyr 4.3.0 (after `west update`)

| Item | Value |
|------|--------|
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ❌ **Kconfig failure** — mbedTLS / nrf_security dependency loop (observed May 2026) |
| **Note** | Intended field stack; may need `patches/zephyr-v4.3.0-disable-mbedtls-builtin.patch` + `scripts/apply-zephyr-433-patch.ps1` before build succeeds |

### Build B — tip `prj.conf`, stale workspace still on NCS 3.2.3 / Zephyr 4.2.99

| Item | Value |
|------|--------|
| **Setup** | Checkout `7686423` but **do not** run `west update` (keep nrf `6f8485d289` / zephyr `c4c75f71e70` from prior 3.2.3 snapshot) |
| **Command** | `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild` |
| **Result** | ✅ **As-is success** (no CMake workaround needed — `X509_REMOVE_INFO=n` in git) |
| **`zephyr.bin` size** | **346,564** bytes (**+6,012 B vs `b539f2f` workaround** — LEGACY_CRYPTO / PSA options in `prj.conf`) |

### ROM banner strings (Build B / stale 4.2.99 workspace)

```text
*** Using nRF Connect SDK v3.2.3-6f8485d2890d ***
*** Using Zephyr OS v4.2.99-c4c75f71e709 ***
Pouch SDK Version: v0.1.0-226-g7686423
*** Booting Pouch BLE GATT Example v1.0.0-7686423 ***
```

### Reproduce

```powershell
git checkout 7686423
# Build A (expect Kconfig fail unless patch applied):
cd C:\ncs_pouch_soil && west update
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
cd C:\ncs_pouch_soil\pouch\examples\ble_gatt
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild

# Build B (stale 3.2.3 workspace — skip west update after checkout):
# … same build command; as-is OK if zephyr still at c4c75f71e70
```

### Notes

- **`firmware/unit2_golden.bin`** added here; ROM **Zephyr 4.3.0** — not reproduced by Build B.
- Field scripts: `scripts/build_unit3_field.ps1`, `flash_unit2_golden.ps1`.
- Primary **`0x2700, 8` suspect era** after last good field stack (`00443f2` / `b539f2f`); see **Field timeline** below.
---

## Field timeline & `0x2700, 8`

Cross-reference for **`soil_sensor`** field debugging. Full build rows: chronological snapshots above and [`2700-8-debug.md`](2700-8-debug.md).

### Last likely-good field stack

| Item | Detail |
|------|--------|
| **Pouch commits** | **`00443f2`** (OTA/settings off, 8K stacks) and **`b539f2f`** (+ soil moisture **percent**, debug Mate) |
| **Manifest** | NCS **`v3.2.3`**, Zephyr **`4.2.99`** (`c4c75f71e70`) after `west update` |
| **Build** | **Workaround required** — `west build ... -- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"` (not committed until **`7686423`**) |
| **`zephyr.bin`** | **340,476** (`00443f2`) / **340,552** (`b539f2f`) |
| **Field report** | **4 XIAO nodes + FRDM gateway** worked until roughly **`b539f2f`** / **`00443f2`** (May 2026) |

### After last good — commit map

| SHA | Describe | Node firmware change? | Build note |
|-----|----------|----------------------|------------|
| `10f6244` | `-224` | **No** — `0x2700` docs only | Same workaround binary as `b539f2f` |
| `628238f` | `-225` | **No** — `4.12` expiry docs | Same as `10f6244` |
| `7686423` | `-226` | **`prj.conf` + `west-ncs.yml` only** vs `b539f2f` | Real `west update` → **Kconfig fail**; stale 4.2.99 workspace → **346,564 as-is** |

### Suspects for `0x2700, 8` onset

| Suspect | Why |
|---------|-----|
| **Wrong flash image** | As-is build **link-fails** — field may have flashed a **failed or partial** build, or omitted **`X509_REMOVE_INFO=n`** workaround |
| **Zephyr 4.2.99 vs 4.3.0** | Plain `west update` on 3.2.3 → **4.2.99**; unit 2 golden / tip manifest → **4.3.0** — crypto/TLS paths differ |
| **`7686423` crypto Kconfig** | `CONFIG_MBEDTLS_LEGACY_CRYPTO_C`, `CONFIG_MBEDTLS_USE_PSA_CRYPTO`, committed `X509_REMOVE_INFO=n` — changes verify path vs workaround-only builds |
| **Gateway vs node mismatch** | Gateway cert chain changes (`d42c790` … `1639474`) while node at **`00443f2`/`b539f2f`** — less likely if gateway unchanged in field |

### Recommended compare order

1. ROM strings on failing unit vs **Build B** at **`00443f2`** or **`b539f2f`**.
2. If describe is **`-226`**, compare vs **`7686423` Build B** (stale 4.2.99) and **`firmware/unit2_golden.bin`** (4.3.0 golden).
3. Confirm flash used **workaround** or post-`7686423` as-is, not an as-is link-fail artifact.

---

## Planned snapshots (not yet in this registry)

All **`soil_sensor`** commits through **`7686423`** are recorded above (2026-06-01 batch). Re-add a row here only when a **new** Pouch commit needs verification.

| Short name | Pouch SHA | Intended manifest | Typical board | Why verify |
|------------|-----------|-------------------|---------------|------------|
| *(none pending)* | — | — | — | — |

---

*Add new verified snapshots at the **end** of the chronological “Snapshot:” sections (below the template).*

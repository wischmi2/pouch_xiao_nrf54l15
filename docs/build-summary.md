# Build summary — `soil_sensor` verified snapshots

One-page view of **software versions**, **as-checked-in vs workaround builds**, **`0x2700, 8`**, and **unit 2** placement. Detail and reproduce steps live in [`verified-workspace-snapshots.md`](verified-workspace-snapshots.md) (chronological). See also [`2700-8-debug.md`](2700-8-debug.md) and [`pouch-node-compiled-ca.md`](pouch-node-compiled-ca.md).

**Workspace:** `C:/ncs_pouch_soil` · **XIAO build (unless noted):**  
`west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild`  
**Link workaround (when needed):** `-- "-DCONFIG_MBEDTLS_X509_REMOVE_INFO=n"`  
**SDK:** `ZEPHYR_SDK_INSTALL_DIR=C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk`

---

## Reading the table: Pouch SHA vs `git describe`

These are **two ways to name the same Pouch commit** in the `pouch` git repo:

| Column | What it is | Example |
|--------|------------|---------|
| **Pouch SHA** | The commit you check out: `git checkout d8f79b3` | `d8f79b3` |
| **`git describe`** | A **version label** Git prints: `git describe --tags --always` | `v0.1.0-221-gd8f79b3` (often shortened to **`v0.1.0-221`** in tables) |

**How to read `v0.1.0-221`:** nearest release tag **`v0.1.0`**, then **221 commits** on top of that tag, then (in full form) **`-g`** + abbreviated SHA. It is **not** a separate repo or a different checkout — same commit as the SHA in the first column.

That string also appears in **flash ROM** (e.g. `Pouch SDK Version: v0.1.0-221-gd8f79b3` in `zephyr.bin`). Handy when a serial log shows a version but you do not know the short SHA.

---

## Summary table (chronological)

| Pouch SHA | `git describe` | NCS (manifest) | Zephyr after `west update` | Board | As-is build | With workaround / notes | `zephyr.bin` (bytes) | `0x2700, 8` / field |
|-----------|----------------|----------------|----------------------------|-------|-------------|-------------------------|----------------------|---------------------|
| `73662de` | `v0.1.0` | **3.0.1** | **4.0.99** | `nrf52840dk` | ✅ (same as `33412d8`, not re-run) | — | **356,908** | N/A — no XIAO board in tree |
| `33412d8` | `v0.1.0-1` | **3.0.1** | **4.0.99** | `nrf52840dk` + sysbuild | ✅ | — | **356,908** | N/A — not XIAO field stack |
| `ddba5e6` | `v0.1.0-213` | **3.2.3** | **4.2.99** (`ncs-v3.2.3`) | XIAO | ✅ | — | **332,172** | Not on cert-debug path yet |
| `f1d32b4` | `v0.1.0-214` | **3.2.3** | **4.2.99** | XIAO | ✅ | Docs only vs parent | **332,172** | Same |
| `b965e36` | `v0.1.0-215` | **3.2.3** | **4.2.99** | XIAO | ✅ | Soil ADC uplink | **336,532** | Same |
| `de25234` | `v0.1.0-216` | **3.2.3** | **4.2.99** | XIAO | ✅ | LittleFS `/lfs1` | **337,776** | Same |
| `0c53b2a` | `v0.1.0-217` | **3.2.3** | **4.2.99** | XIAO | ❌ link (`cert.c` / `mbedtls_x509_crt_info`) | ✅ `X509_REMOVE_INFO=n` | **347,808** | **Failure A path** in firmware; runtime `0x2700` possible if image flashed without workaround |
| `d42c790` | `v0.1.0-218` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **348,388** | Gateway/cert docs; node +580 B |
| `3ae0ff9` | `v0.1.0-219` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **348,860** | Multi-bond + button uplink |
| `1639474` | `v0.1.0-220` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **348,860** | Gateway-only diff vs `-219` |
| **`d8f79b3`** | **`v0.1.0-221`** | **3.2.3** | **4.2.99** (verified); **4.3.0** on **unit 2 ROM** | XIAO | ❌ link | ✅ workaround | **348,860** | **Unit 2 golden source era** — see [Unit 2](#unit-2-golden-reference) |
| `00443f2` | `v0.1.0-222` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **340,476** | **Last likely-good field stack** (OTA/settings off); 4 nodes + FRDM OK ~here |
| `b539f2f` | `v0.1.0-223` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **340,552** | **Last likely-good** (+ soil %); 4 XIAO nodes OK until ~this commit |
| `10f6244` | `v0.1.0-224` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **340,552** | **Docs only** (`0x2700` troubleshooting); **same binary as `b539f2f`** |
| `628238f` | `v0.1.0-225` | **3.2.3** | **4.2.99** | XIAO | ❌ link | ✅ workaround | **340,552** | **Docs only** (`4.12` expiry); same binary |
| `7686423` | `v0.1.0-226` | **3.3.0** (manifest) | **4.3.0** after real `west update`; **4.2.99** if workspace stale | XIAO | ❌ **Kconfig** on 3.3.0 + 4.3.0 | ✅ as-is only if Zephyr still **4.2.99** | **346,564** (stale 4.2.99) | **Primary suspect era** for `0x2700, 8` after last good — `prj.conf` crypto + west bump; `unit2_golden.bin` added |

**Golioth firmware SDK** at **`d703b1f`** for all NCS **3.2.3** rows (through `628238f`). Tip manifest **`7686423`** keeps that SDK pin with NCS **3.3.0**.

---

## As-checked-in vs what actually links (XIAO)

| Era | Commits | As-is `west build` | What field likely used |
|-----|---------|-------------------|-------------------------|
| Early XIAO | `ddba5e6` … `de25234` | ✅ | Plain build |
| Cert verify logging | `0c53b2a` … `628238f` | ❌ link unless `X509_REMOVE_INFO=n` | **Workaround** (not in git until `7686423`) |
| Tip manifest | `7686423` + real `west update` | ❌ Kconfig (mbedTLS / nrf_security loop) | Need patch or fix Kconfig before **4.3.0** production build |

`0x2700, 8` is a **runtime** server-cert verify failure ([Failure A](pouch-node-compiled-ca.md)), not a link error. A **failed or wrong flash** (as-is link fail, or 4.2.99 vs 4.3.0 mismatch) can still produce it in the field.

---

## Where `0x2700, 8` likely showed up

| Phase | Commits | Build / stack change | `0x2700, 8` relevance |
|-------|---------|----------------------|------------------------|
| Before cert path | `ddba5e6` … `de25234` | No `cert.c` x509 info logging | Unlikely same Failure A signature |
| Cert + gateway work | `0c53b2a` … `1639474` | `cert.c`, secp384, gateway paths; link needs workaround | Verify path exists; **4 nodes still worked** through later `-223` per field report |
| Unit 2 era source | **`d8f79b3`** (`-221`) | Same link pattern; golden ROM **≠** plain `west update` binary | Compare failing units to **`firmware/unit2_golden.bin`** or **4.3.0** ROM, not only 4.2.99 workaround |
| Last good field | **`00443f2`**, **`b539f2f`** | OTA off, 8K stacks, workaround, **4.2.99** | **Baseline** for ROM string / binary compare |
| Docs only | `10f6244`, `628238f` | **No node source diff** vs `b539f2f` | Onset **not** explained by these commits alone |
| Tip | **`7686423`** | `west-ncs.yml` → **3.3.0** + **4.3.0**; `LEGACY_CRYPTO`, PSA, `X509_REMOVE_INFO=n` in git | **Highest suspect** if `west update` + new flash after `-223`; also wrong image if Kconfig/build never succeeded |

**Practical compare order:** failing unit ROM → **Build B** at `00443f2` or `b539f2f` → if describe `-226`, **`7686423` Build B** (stale 4.2.99) and **`firmware/unit2_golden.bin`**.

---

## Unit 2 golden reference

Unit 2 is **not** the same as “whatever `west update` produces at `d8f79b3`.”

| Aspect | Unit 2 (field reference) | Verified build at `d8f79b3` (`-221`) |
|--------|--------------------------|----------------------------------------|
| **Pouch source (SHA)** | Commit **`d8f79b3`** | Same |
| **ROM / `git describe` on device** | **`v0.1.0-221`** (full: `…-221-gd8f79b3`), Zephyr **`v4.3.0`** | Workaround image: Zephyr **`v4.2.99`**, **348,860** B |
| **Manifest at commit** | NCS **`v3.2.3`** only (no explicit Zephyr 4.3 project) | `west update` → **4.2.99** |
| **Commit content** | OpenOCD recovery **docs/patch** only vs prior SDK commits | Bring-up (`cert.c`, secp384) is in **ancestors** (`0c53b2a` …) |
| **Known-good flash image** | **`firmware/unit2_golden.bin`** (added at **`7686423`**) | Emergency compare; **4.3.0** full flash, not reproduced by table row alone |

**Relative to the build table:**

- **Unit 2 “home” commit:** **`d8f79b3`** (`-221`) — golden **source era** and ROM describe.
- **Unit 2 binary on disk:** shipped at **`7686423`** as `firmware/unit2_golden.bin` (matches **-221** describe + **4.3.0**, not **-223** workaround bytes).
- **Last field-working builds** are **newer** than unit 2 source: **`00443f2`** (`-222`) and **`b539f2f`** (`-223`) on **4.2.99** with workaround — **smaller** images (OTA off), **not** unit 2 golden size/stack.

If **`0x2700, 8`** appeared after **`b539f2f`**, suspect flashing **`7686423`** (crypto / manifest), a **4.3.0** attempt, or a **non-workaround** build — not the doc-only `-224`/`-225` commits.

---

## Quick legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Build succeeded as checked in (or expected equivalent) |
| ❌ link | `mbedtls_x509_crt_info` undefined in `cert.c` |
| ❌ **Kconfig** | Configure fails (tip **3.3.0** + **4.3.0**) |
| **Last likely-good** | 4× XIAO + FRDM gateway per May 2026 field report |
| **Unit 2** | Golden ROM / `-221` source; Zephyr **4.3.0** on device |

---

*Source: batch verifications 2026-05-30 / 2026-06-01 in [`verified-workspace-snapshots.md`](verified-workspace-snapshots.md). Update this summary when new snapshots are added at the end of that file.*

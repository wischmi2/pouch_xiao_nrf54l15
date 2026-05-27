# Battery uplink — issues and limitations

This document records problems, risks, and open items discovered while adding battery
measurement and `.s/battery` Golioth uplink on branch `feature/xiao-battery-uplink`.
See [xiao-battery-uplink.md](xiao-battery-uplink.md) for the intended design and build steps.

## Summary

The firmware changes are in place (`battery.c`, overlay, `prj.conf`, `main.c`), but a
**full end-to-end build and on-device validation were not completed** in the environment
where the feature was developed. Several issues are environmental (workspace layout,
Zephyr board pack version) or require verification on your NCS machine and hardware.

---

## 1. Full `west build` did not succeed locally

**What happened:** Running `west build` from `examples/ble_gatt` against a standalone
Zephyr tree (`C:/Users/Brian/zephyr`) failed at Kconfig with warnings treated as errors.

**Examples:**

- `POUCH_UPLINK_PROCESSING_STACK_SIZE` — assigned in `prj.conf` but reported as undefined
  (Pouch Kconfig not fully integrated when the app is not built inside the proper NCS west workspace).
- `MBEDTLS_HEAP_SIZE` — unsatisfied dependency warnings.
- Multiple Pouch Kconfig choice / log-level warnings when `ZEPHYR_EXTRA_MODULES` pointed at this repo.

**Impact:** Could not produce a linked `zephyr.elf` or run `west flash` from this repo path alone.

**Mitigation:** Build from the **NCS west workspace** you already use for soil sensor
(e.g. `C:/ncs_pouch_soil/pouch/examples/ble_gatt`), same as [v0.1.0-build-steps.md](v0.1.0-build-steps.md).
Use `--pristine` after switching to `feature/xiao-battery-uplink`.

**Status:** Open until you confirm a clean build on your machine.

---

## 2. NCS workspace path was not available here

**What happened:** `C:/ncs_pouch_soil/pouch` did not exist on the machine used for implementation.

**Impact:** Could not run the documented `build_ble_gatt_v010.ps1` flow or flash firmware as part of verification.

**Mitigation:** Sync or copy this branch into your NCS `pouch` checkout and build there.

**Status:** Environment-specific; not a code defect.

---

## 3. Board DTS must include `vbat_pwr` and SAADC channel 7

**What happened:** The local Zephyr board pack at `C:/Users/Brian/zephyr/boards/seeed/xiao_nrf54l15/`
(Golioth-era fork) did **not** define:

- `vbat_pwr` (battery load switch / regulator)
- `&adc` channel children (including channel 7 for AIN7)

Generated `zephyr.dts` showed `adc@d5000` with `status = "disabled"` and no `channel@7` until
the overlay only set `&adc { status = "okay"; }` — still **no** `vbat_pwr` without board support.

**Impact:** `battery.c` compiles to the **stub** path (`battery_error: -ENODEV`) if
`DT_NODE_EXISTS(DT_NODELABEL(vbat_pwr))` is false at build time.

**Mitigation:**

- Use **NCS / Zephyr 3.2.3+** board files where `vbat_pwr` and ADC channels live in
  `xiao_nrf54l15_nrf54l15_cpuapp.dts` and `xiao_nrf54l15_common.dtsi` (upstream Seeed layout).
- Do **not** duplicate `vbat_pwr` in the app overlay on those trees (see issue 4).

**Status:** Depends on your west manifest / Zephyr revision. Verify with:

```text
# After west build, in build/zephyr/zephyr.dts:
#   - node vbat_pwr (or vbat-pwr)
#   - adc channel@7 with AIN7
```

---

## 4. Overlay duplicate nodes (resolved by trimming overlay)

**What happened:** An early overlay added `vbat_pwr` and explicit `channel@0` / `channel@7`
under `&adc`. On **newer** Zephyr/NCS board DTS, those nodes already exist.

**Risk:** Devicetree compile error — duplicate label `vbat_pwr` or duplicate `channel@N` nodes.

**Resolution:** Final overlay only adds:

- `zephyr,user { io-channels = <&adc 0>, <&adc 7>; }`
- `&adc { status = "okay"; }`

**If you still see duplicate-label errors:** Remove `&adc { status = "okay"; }` as well if
the board already enables ADC.

**Status:** Addressed in repo; re-check when merging against your exact Zephyr SHA.

---

## 5. ADC index vs SAADC channel number

**What happened:** Seeed wiki sample uses `adc_channels[7]` because their overlay lists
eight `io-channels` (`&adc 0` … `&adc 7`). Our overlay lists only two phandles:

```dts
io-channels = <&adc 0>, <&adc 7>;
```

**Impact:** Firmware must use **`ADC_DT_SPEC_GET_BY_IDX(zephyr_user, 1)`** for battery,
not index 7. Using the wrong index would read the wrong channel or fail at runtime.

**Status:** Implemented correctly in `battery.c` (`BATTERY_ADC_IDX 1`). Documented in plan
and [xiao-battery-uplink.md](xiao-battery-uplink.md).

---

## 6. Stale `APP_BUILD_VERSION` at boot (seen on soil branch)

**What happened:** Serial boot line showed `v0.1.0-221-gd8f79b35e8be` while runtime logs
matched newer code (`soil_moisture_percent`, `len %zu` uplink format).

**Impact:** Boot git hash is **not reliable** for confirming flashed firmware; use runtime log
fingerprints or always `west build --pristine` after branch switches.

**Status:** Process issue; applies to battery branch too.

---

## 7. `regulator-boot-on` on upstream `vbat_pwr`

**What happened:** Upstream Zephyr `xiao_nrf54l15` board DTS defines `vbat_pwr` with
`regulator-boot-on`, which keeps the battery sense path powered at boot. The Seeed wiki
describes enabling `vbat_pwr` only during ADC reads to save power.

**Impact:** Our code calls `regulator_enable()` / `regulator_disable()` around each read, but
boot-on may leave the switch active until first disable — slightly higher idle draw than
wiki “ideal,” usually acceptable.

**Possible follow-up:** Overlay fragment on the feature branch:

```dts
&vbat_pwr {
    /delete-property/ regulator-boot-on;
};
```

Only if you need minimum battery drain and your board DTS includes `vbat_pwr`.

**Status:** Not implemented; behavior unverified on hardware.

---

## 8. LiPo percent mapping is approximate

**What happened:** `battery_percent` uses a linear map between 3000 mV (empty) and 4200 mV
(full). Real cells are non-linear and load-dependent.

**Impact:** Percent is useful for dashboards/trends, not for accurate state-of-charge.

**Status:** By design for v1; calibrate later if needed.

---

## 9. USB vs battery power readings

**What happened:** Not tested on hardware in this session. Wiki notes USB-powered boards may
not reflect true LiPo voltage; battery-only boot may fail if UART is enabled in `prj.conf`.

**Impact:**

- High `battery_mv` / ~100% on USB is expected and does not prove the divider path works.
- LiPo-only deployment may need UART disabled (Seeed “Scenario B”) — documented in wiki, not
  implemented as a separate `prj_uart.conf` in this branch.

**Status:** Requires your bench/LiPo test.

---

## 10. Soil ADC and battery ADC share one SAADC peripheral

**What happened:** Both use the same `&adc` controller (soil ch0, battery ch7). Reads are
sequential in `do_uplink()` (soil first, then battery with regulator gating).

**Risk:** Theoretically channel re-setup latency or interaction if reads overlap; not observed
in code review. Soil uses a persistent channel setup at boot; battery sets up per read.

**Status:** Low risk; monitor serial if soil readings become unstable after enabling battery.

---

## 11. Large unrelated git working tree noise

**What happened:** `git status` showed hundreds of modified files (likely CRLF / line-ending
or wholesale tree differences) beyond the battery feature files.

**Impact:** Easy to accidentally commit unrelated changes; harder to review battery-only diffs.

**Mitigation:** Stage only battery-related paths when committing:

```powershell
git add examples/ble_gatt/src/battery.c examples/ble_gatt/src/battery.h `
        examples/ble_gatt/src/main.c examples/ble_gatt/CMakeLists.txt `
        examples/ble_gatt/prj.conf `
        examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay `
        docs/xiao-battery-uplink.md docs/battery_issues.md
```

**Status:** Repo hygiene; not specific to battery logic.

---

## 12. Golioth / gateway validation not done here

**What happened:** No live check that `.s/battery` appears in the Golioth console after gateway sync.

**Expected when working:**

- Serial: `Writing battery uplink: path .s/battery, ...`
- Cloud: JSON `{"battery_mv":...,"battery_percent":...}` on path `.s/battery`

**Status:** Open — verify after flash on `feature/xiao-battery-uplink`.

---

## 13. Feature branch not merged; `soil_sensor` unchanged

**What happened:** Work lives on `feature/xiao-battery-uplink` branched from `soil_sensor`.
Battery files were staged but a commit was not requested during implementation.

**Impact:** You must build/checkout this branch (or merge) to get battery firmware; `soil_sensor`
remains the known-good baseline.

**Status:** As intended by plan.

---

## Verification checklist (for you)

1. `git checkout feature/xiao-battery-uplink`
2. Build in NCS workspace: `west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild`
3. Confirm `zephyr.dts` contains `vbat_pwr` and ADC `channel@7`
4. Flash and check serial for `Battery ADC and regulator configured`
5. Connect gateway; confirm `.s/battery` and `.s/sensor` in Golioth
6. Repeat with LiPo only if you care about battery-powered deployment

---

## Related docs

- [xiao-battery-uplink.md](xiao-battery-uplink.md) — feature runbook
- [v0.1.0-build-steps.md](v0.1.0-build-steps.md) — NCS / XIAO build environment
- [Seeed wiki — battery-powered board](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/#battery-powered-board)

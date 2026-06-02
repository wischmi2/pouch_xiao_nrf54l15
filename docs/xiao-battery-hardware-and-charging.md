# XIAO nRF54L15 — battery hookup, charging, and firmware

This document covers **hardware** battery connection and charging on the Seeed XIAO nRF54L15,
what the **firmware** configures (voltage read + Golioth uplink), and what you **cannot**
start or stop from software.

For build, flash, and `.s/battery` uplink steps, see [xiao-battery-uplink.md](xiao-battery-uplink.md).
For known pitfalls and validation checklist items, see [battery_issues.md](battery_issues.md).

**Verified on hardware (2026-06):** UART bench image + `smpmgr` credentials → battery-only image →
gateway `Delivered uplink block` and Golioth `.s/battery` on USB and LiPo-only reset. See
[Verified end-to-end workflow](#verified-end-to-end-workflow-what-worked).

Official Seeed reference:
[Battery-powered board](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/#battery-powered-board)

Board schematic (PDF):
[nRF54L15 schematic](https://files.seeedstudio.com/wiki/XIAO_nRF54L15/Getting_Start/nRF54L15_Schematic.pdf)

---

## Quick answers

| Question | Answer |
|----------|--------|
| Does USB charge the LiPo? | **Yes**, automatically, when a cell is on the battery pads. |
| Do we configure start/stop charging in firmware? | **No.** Charging is handled by onboard hardware (SGM40567). |
| What does our firmware control? | **Voltage measurement** only (`vbat_pwr` + ADC), then uplink to Golioth. |
| Safe to use USB + battery together? | **Yes** (Seeed: built-in protection). |
| Will it keep uplinking after USB unplug? | **Yes**, if LiPo is connected, charged enough, and the gateway is in range. |

---

## Hardware overview

The XIAO nRF54L15 has two separate battery-related circuits:

```text
USB Type-C (5 V)
       │
       ▼
  SGM40567 (U1)          ← LiPo charger IC (hardware only)
       │
       ├──► Battery pads (3.7 V LiPo + / −)
       │
       └──► VSYS (powers nRF54L15 and peripherals)

Battery pads
       │
       ▼ (via divider, when enabled)
  TPS22916 (vbat_pwr)    ← Load switch, GPIO P1.15 (VBAT_EN)
       │
       └──► AIN7 / P1.14  ← SAADC channel 7 (firmware reads this)
```

| Component | Part / signal | Role |
|-----------|---------------|------|
| **Charger** | SGM40567-4.2XG/TR | Charges LiPo from USB; terminates at ~4.2 V |
| **Charge current** | ~200 mA (schematic) | Set by resistor on `ICharge_SET`; not firmware-configurable |
| **Sense switch** | TPS22916 (`vbat_pwr`) | Connects divider to ADC only when enabled (saves idle power) |
| **ADC input** | P1.14 / AIN7 (SAADC ch 7) | Half of cell voltage at pin; firmware multiplies by 2 |
| **Enable GPIO** | P1.15 / VBAT_EN | Driven by `regulator_enable()` / `regulator_disable()` in firmware |
| **Charge LED** | Red `CHARGE_LED` | Hardware status; not controlled by Pouch firmware |

Seeed’s wiki uses the term “PMIC” loosely. On this board the **charger is the SGM40567**;
the **TPS22916** is only for gating the voltage-divider sense path.

---

## Physical battery hookup

1. Use a **qualified 3.7 V rechargeable LiPo** (single cell).
2. Solder to the **battery pads** on the back of the XIAO with correct **polarity**.
   Shorting or reversing polarity can damage the cell and board.
3. USB Type-C can remain connected for bench work, charging, and serial debug.

Seeed schematic note: `Battery voltage = ADC sampling voltage × 2.0`

---

## Charging — what happens automatically

### Start charging

**Nothing to configure in firmware.**

When **USB is plugged in** and a LiPo is on the pads:

- The **SGM40567** starts charging automatically.
- The board can run from USB/charger while the cell charges.
- The **red charge LED** indicates state (hardware):

  | LED behavior (Seeed schematic) | Meaning |
  |--------------------------------|---------|
  | Blinking | Actively charging |
  | On for ~51 s, then **off** | Charge complete (then stays off) |
  | **Off** (steady) | Not actively charging — often **already full** |

**Important:** A **dark red charge LED is normal** much of the time. It is **not** a “power on”
indicator. With your readings (~4188 mV / 99%), the cell is near full; the charger has likely
finished and the LED will stay off until the pack drops enough to charge again.

Do not confuse **`CHARGE_LED`** (red, charger status, hardware-only) with **`USER_LED`**
(P2.0, blue/green user LED — your firmware drives this at boot for LED stages).

### Charge LED off — how to tell charging still works

1. **`battery_mv` in Golioth** — plug in USB with a **partially discharged** cell; voltage should
   climb over tens of minutes toward ~4200 mV.
2. **Force a recharge cycle** — unplug USB, run on battery for a while (or use a depleted pack),
   plug USB back in; red LED should **blink** while actively charging.
3. **After fresh plug-in when full** — you may see red ON briefly (~51 s per schematic), then off.

Low USB load: some power banks shut off because the XIAO draws very little current when the
battery is full (similar reports on other XIAO boards). Use a PC USB port or a “always on”
charger if testing.

### Stop charging

Also **automatic**:

- The charger stops (or trickle-maintains) when the cell reaches ~**4.2 V**.
- Unplugging **USB** stops charging; the board then runs from the LiPo if enough charge remains.

There is **no** Kconfig option, shell command, or API in this repo to enable/disable the
SGM40567 charger. To stop charging, **unplug USB** (or remove the battery).

### What firmware does *not* do

- Does not enable/disable the charger IC
- Does not set charge current
- Does not read charger status registers
- Does not drive the charge LED

---

## Power scenarios

### USB only (no LiPo)

- Board runs from USB.
- ADC may still report **plausible** `battery_mv` (often 3.8–4.2 V range) on a floating sense
  net — **not meaningful** as state-of-charge.
- Example without cell: `{"battery_mv":4020,"battery_percent":85}`.

### USB + LiPo (bench / development)

- Board runs from USB; **LiPo charges**.
- `battery_mv` often sits **high and stable** (~4100–4200 mV, 95–100%) while plugged in.
- Safe for development; this is Seeed **Scenario A** (USB bench debugging).

### LiPo only (field deployment)

- Unplug USB after boot; board runs from the cell.
- Golioth uplinks continue over **BLE → gateway** (no USB required).
- Serial monitor stops when USB is removed.
- **Cold boot on battery only** may fail if UART/console is enabled in `prj.conf` — see
  [UART and battery-only boot](#uart-and-battery-only-boot) below.

---

## Firmware — what we configure

Battery support lives on branch **`feature/xiao-battery-uplink`** (see
[xiao-battery-uplink.md](xiao-battery-uplink.md)).

### Measurement flow (`battery.c`)

On each gateway sync (`do_uplink()` in `main.c`):

1. `regulator_enable(vbat_pwr)` — turn on TPS22916 (~100 ms settle)
2. Read SAADC channel 7 via `zephyr,user` io-channel **index 1**
3. Convert to mV, multiply by **2** for cell voltage
4. Map linearly: **3000 mV = 0%**, **4200 mV = 100%**
5. `regulator_disable(vbat_pwr)` — turn off sense path
6. Write JSON to Pouch uplink path **`.s/battery`**

### Devicetree overlay

[`examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay`](../examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay):

```dts
vbat_pwr: vbat-pwr {
    compatible = "regulator-fixed";
    regulator-name = "vbat";
    enable-gpios = <&gpio1 15 GPIO_ACTIVE_HIGH>;
};

zephyr,user {
    io-channels = <&adc 0>, <&adc 7>;  /* index 0 = soil, index 1 = battery */
};
```

**Note:** On NCS 3.2.3+ upstream board DTS, `vbat_pwr` may already exist. If the build
reports a **duplicate label**, remove the `vbat_pwr { ... }` block from the overlay and keep
only `io-channels` (see [battery_issues.md](battery_issues.md) §3–4).

Requires in generated `zephyr.dts`:

- Node `vbat_pwr` (or `vbat-pwr`)
- `&adc` channel `@7` with `NRF_SAADC_AIN7`

### Kconfig (`prj.conf`)

| Option | Purpose |
|--------|---------|
| `CONFIG_ADC=y` | SAADC driver |
| `CONFIG_REGULATOR=y` | `vbat_pwr` load switch |
| `CONFIG_PM_DEVICE=y` | Regulator power management |

These configure **measurement**, not charging.

### Golioth payload

**Path:** `.s/battery`

Success:

```json
{"battery_mv":4188,"battery_percent":99}
```

Hardware not configured at build time:

```json
{"battery_error":-19}
```

---

## UART and battery-only boot

Current `prj.conf` enables USB serial (`CONFIG_UART_CONSOLE=y`, `CONFIG_CONSOLE=y`) for bench
debug (Seeed **Scenario A**). Seeed documents that **LiPo-only cold boot fails** with UART
enabled on XIAO nRF54L15 v1.0.

### Symptom (matches bench testing)

| With USB | Without USB (reset or power cycle) |
|----------|-------------------------------------|
| Golioth updates, gateway syncs OK | **Nothing** on gateway or Golioth |
| Red charge LED may blink then off | No serial, no user LED, looks dead |
| `battery_mv` uplinks | No BLE advertising (or flaky connect + security fail) |

**This is usually not a dead battery.** Charging worked (red LED blinked when you plugged USB
back in). The LiPo has power; the **firmware image** is built for USB bench debug and **does not
boot reliably on battery-only reset**.

| Mode | When | Build |
|------|------|-------|
| **Scenario A** — USB bench | Develop, flash, `smpmgr`, serial logs | Default `prj.conf` |
| **Scenario B** — battery deploy | LiPo only, reset in field | `prj.conf` + [`prj_battery.conf`](../examples/ble_gatt/prj_battery.conf) |

### Battery-only firmware build

Credentials must already be on LittleFS (upload via `smpmgr` over USB using a Scenario A build
first). See [Verified end-to-end workflow](#verified-end-to-end-workflow-what-worked) below.

**Hot unplug** while already running can still fail (brownout reset → Scenario A image won't
restart on LiPo). **Reset without USB** always cold-boots — use Scenario B for that case.

---

## Verified end-to-end workflow (what worked)

This is the sequence bench-tested on **Seeed XIAO nRF54L15** with **FRDM RW612 gateway** and
Golioth. Branch: **`feature/xiao-battery-uplink`**.

### Two flash images

| Image | Config | Flash size (approx.) | Use |
|-------|--------|----------------------|-----|
| **Scenario A — UART** | Default [`prj.conf`](../examples/ble_gatt/prj.conf) | ~326 KB | Flash, serial, `smpmgr` credential upload |
| **Scenario B — battery** | `prj.conf` + [`prj_battery.conf`](../examples/ble_gatt/prj_battery.conf) | ~278 KB | LiPo-only field deploy (**no UART**) |

Rebuild both from repo root:

```powershell
.\scripts\build_ble_gatt_xiao_dual.ps1
```

Outputs: `builds/xiao-ble-gatt-uart/zephyr.hex` and `builds/xiao-ble-gatt-battery/zephyr.hex`.

Or build one variant from `examples/ble_gatt`:

```powershell
cd C:\Users\Brian\ncs\pouch\examples\ble_gatt

# Scenario A (UART bench)
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild

# Scenario B (battery / no UART) — quote EXTRA_CONF_FILE in PowerShell
west build -b xiao_nrf54l15/nrf54l15/cpuapp --pristine --no-sysbuild -- "-DEXTRA_CONF_FILE=prj_battery.conf"
```

### Step 1 — Flash UART image and upload credentials

1. Connect **XIAO debugger** (SWD). Flash Scenario A:

   ```powershell
   west flash --skip-rebuild --hex-file C:\Users\Brian\pouch_xiao_nrf54l15\builds\xiao-ble-gatt-uart\zephyr.hex
   ```

2. Open serial (e.g. COM7/COM9, 115200). Confirm boot logs through **`Advertising started`**
   and **`Credentials loaded`** (no `USAGE FAULT` after stage 5).

3. Upload Golioth device credentials to LittleFS (paths must match `CONFIG_EXAMPLE_CREDENTIALS_DIR`):

   ```text
   /lfs1/credentials/crt.der
   /lfs1/credentials/key.der
   ```

   Use **`smpmgr`** over the USB serial port while Scenario A is flashed.

4. Confirm gateway sync on UART build: serial + gateway log **`Delivered uplink block`**.

### Step 2 — Flash battery image (keep credentials)

```powershell
west flash --skip-rebuild --hex-file C:\Users\Brian\pouch_xiao_nrf54l15\builds\xiao-ble-gatt-battery\zephyr.hex
```

If OpenOCD does **not** report mass erase / AP recovery, LittleFS credentials usually **survive**
this flash. If credentials were wiped, repeat Step 1.

### Step 3 — What success looks like (no serial on Scenario B)

**User LED (P2.0, green)** with `CONFIG_EXAMPLE_BATTERY_POWER_LED=y`:

| Signal | Meaning |
|--------|---------|
| Solid **~1 s** at boot | Firmware reached `main()` |
| **5 slow blink groups** (stages 1–5) | BLE + Pouch + ADC init OK |
| Short flash every **4 s** | Heartbeat — firmware running |
| **Two long blinks** (~500 ms each) | Gateway BLE connected |
| **3×5 fast blinks** at boot | Failed before stage 3 — usually **missing credentials** |

**Red `CHARGE_LED`:** charger status only (Seeed hardware), not app status.

**Gateway log** (node MAC example `D5:13:52:6F:94:32`):

```text
Connected: D5:13:52:6F:94:32 (random)
BT security changed ... to level 2
Golioth accepted node device cert
Delivered uplink block 0: path pouch
```

**LiPo-only cold boot:** unplug USB → press reset → heartbeat continues; gateway should sync
within **~30–60 s** if the pack and RF path are good.

### Firmware behavior that mattered (BLE / stability)

These are in `ble_peripheral.c` / `main.c` on the working branch — **do not regress**:

1. **Sync-request advertising** — `pouch_gatt_adv_req_sync(&service_data, true)` in
   `ble_peripheral_start()` so the gateway scanner connects (Pouch service data flag
   `POUCH_GATT_ADV_FLAG_SYNC_REQUEST`).

2. **Connect LED on a work queue** — never call `k_msleep()` from the Bluetooth `connected`
   callback (caused **`USAGE FAULT`**, `pc = 0`, reboot loop right after stage 5).

3. **Heartbeat without blocking** — 4 s heartbeat uses delayable work (on → 200 ms → off), not
   `k_msleep()` chained from the wrong context.

4. **Do not add `CONFIG_BT_SETTINGS`** without a proper NVS partition layout — previously caused
   `fs_nvs: No GC Done marker` and **USAGE FAULT** at boot.

5. **Do not manually drive `rfsw_pwr` / `rfsw_ctl`** in application code — not required for this
   Pouch example; an experimental RF-switch patch **broke** gateway connectivity on UART and
   battery builds. The stock Zephyr board DTS defines those nodes; leave them alone.

`prj_battery.conf` additionally sets `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y` for stronger advertising
on battery power.

### Gateway log — normal vs worrying

**Normal:** repeated connect → cert → **`Delivered uplink block`** → disconnect `reason 0x13`
(remote ended session).

**Occasional:** `failed to establish. RF noise?`, `BT_SECURITY_ERR_UNSPECIFIED`, **30 s cooldown**,
**Removing stale bond** — often recovers after cooldown; keep node within a few feet of the gateway.

**Worrying:** no `Connected` for minutes, or **3×5 fast blinks** on the node at every boot
(credentials missing).

---


## Interpreting readings

| Observation | Likely cause |
|-------------|--------------|
| ~4000–4200 mV, 85–100% on USB, no LiPo | Floating / USB-fed sense net — ignore percent |
| ~4100–4200 mV, stable on USB + LiPo | Near full; charger holding voltage high |
| Gradual drop over hours on battery-only | Normal discharge — meaningful trend |
| `battery_error` | Build missing `vbat_pwr` or ADC ch7; see [battery_issues.md](battery_issues.md) |

Percent is a **simple linear map**, not a fuel gauge. Good for dashboards and trends, not
precise state-of-charge.

---

## Unplug USB — does the node keep working?

**Yes**, if:

1. LiPo is attached and has enough voltage
2. Firmware is already running (hot unplug)
3. Gateway is powered and in BLE range

The uplink path is **BLE → gateway → Golioth**; USB is not involved after boot.

You lose serial debug when USB is removed. There may be no visible LED activity on battery
unless your application drives one.

---

## Verification checklist

### Hardware / charging

- [ ] LiPo soldered with correct polarity
- [ ] Red charge LED **blinks** when USB is plugged in with a **low** cell (not required when full)
- [ ] `battery_mv` in Golioth rises toward ~4200 over time on USB if cell was low
- [ ] ~4180+ mV with LED off on USB → likely **charge complete**, not a fault

### Firmware / cloud

- [ ] Serial: `Battery ADC and regulator configured` at boot
- [ ] Serial: `Battery sample: divider_mv ..., battery_mv ..., percent ...` each sync
- [ ] Golioth: `.s/battery` updates on gateway sync (~every 20 s after sync)

### Battery-only (Scenario B)

- [ ] Flash battery image after credentials on UART image
- [ ] Solid 1 s → stages 1–5 → 4 s heartbeat (no serial)
- [ ] Gateway: `Connected` + `Delivered uplink block` on USB-powered battery image
- [ ] Unplug USB → reset → heartbeat + gateway sync on LiPo only

---

## Related files

| File | Role |
|------|------|
| [`examples/ble_gatt/src/battery.c`](../examples/ble_gatt/src/battery.c) | Measure + JSON + uplink |
| [`examples/ble_gatt/src/battery.h`](../examples/ble_gatt/src/battery.h) | Public API |
| [`examples/ble_gatt/src/main.c`](../examples/ble_gatt/src/main.c) | Boot LED, `setup_battery()`, `do_uplink()` |
| [`examples/ble_gatt/src/ble_peripheral.c`](../examples/ble_gatt/src/ble_peripheral.c) | Advertising, sync-request flag, connect LED work |
| [`examples/ble_gatt/prj_battery.conf`](../examples/ble_gatt/prj_battery.conf) | Scenario B: no UART, power LED, +8 dBm adv |
| [`scripts/build_ble_gatt_xiao_dual.ps1`](../scripts/build_ble_gatt_xiao_dual.ps1) | Build uart + battery hex into `builds/` |
| [`examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay`](../examples/ble_gatt/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay) | ADC io-channels (soil + battery) |
| [`examples/ble_gatt/prj.conf`](../examples/ble_gatt/prj.conf) | Scenario A: UART, regulator / ADC Kconfig |
| [`builds/README.md`](../builds/README.md) | Prebuilt uart/battery hex layout |

## Related docs

- [xiao-battery-uplink.md](xiao-battery-uplink.md) — build, flash, Golioth runbook
- [battery_issues.md](battery_issues.md) — overlay duplicates, `-ENODEV`, validation gaps
- [v0.1.0-build-steps.md](v0.1.0-build-steps.md) — NCS workspace and XIAO build

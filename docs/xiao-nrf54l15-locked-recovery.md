# XIAO nRF54L15 Locked Debug Recovery

This records the exact recovery path used when the XIAO nRF54L15 was stuck in
an AP-protected state and normal flashing failed through the XIAO debugger.

## Symptom

`west flash` selected the OpenOCD runner, but OpenOCD could not examine the
application CPU:

```text
Error: Failed to read memory at 0xe000ed00
Error: [nrf54l.cpu] Examination failed
Error: [nrf54l.cpu] DP initialisation failed
```

`pyocd flash` also confirmed the lock:

```text
NRF54L15 APPROTECT enabled: will try to unlock via mass erase
Memory transfer fault (SWD/JTAG communication failure (FAULT ACK)) @ 0x00ffc31c-0x00ffc31f
```

`nrfutil device list` could see the XIAO debugger USB device, but it only had
`serialPorts, usb` traits. It did not expose `jlink`, `mcuBoot`, or `nordicDfu`,
so `nrfutil device recover` was not available for this connection mode.

## Fix That Worked

The local Zephyr board OpenOCD config did not include the newer nRF54L CTRL-AP
mass-erase recovery logic. The working fix was to patch:

```text
C:/ncs_pouch_soil/zephyr/boards/seeed/xiao_nrf54l15/support/openocd.cfg
```

The original file was backed up to:

```text
C:/ncs_pouch_soil/zephyr/boards/seeed/xiao_nrf54l15/support/openocd.cfg.bak
```

The patch added nRF54L CTRL-AP recovery support equivalent to the upstream
Zephyr `main` board config:

```tcl
# Define CTRL_AP_NUM explicitly to avoid variable errors.
set CTRL_AP_NUM 2

proc _nrf_check_ap_lock { ctrl_ap_num unlocked_value } {
	set target [target current]
	set dap [$target cget -dap]
	set err [catch {set APPROTECTSTATUS [$dap apreg $ctrl_ap_num 0xc]}]
	if {$err == 0 && $APPROTECTSTATUS < $unlocked_value} {
		echo "\[$target\] device has AP lock engaged, trying recover."
		poll off
		return 1
	}
	return 0
}

proc _nrf_ctrl_ap_recover { ctrl_ap_num {is_cpunet 0} } {
	set target [target current]
	set dap [$target cget -dap]

	set IDR [$dap apreg $ctrl_ap_num 0xfc]
	if {$IDR != 0x32880000} {
		echo "Error: Cannot access nRF54L CTRL-AP! (IDR: 0x$IDR)"
		return
	}

	poll off

	$dap apreg $ctrl_ap_num 4 0
	$dap apreg $ctrl_ap_num 4 1

	set timeout 300
	for {set i 0} {$i < $timeout} {incr i} {
		set ERASEALLSTATUS [$dap apreg $ctrl_ap_num 8]
		if {$ERASEALLSTATUS == 2} {
			break
		}
		if {$ERASEALLSTATUS == 3} {
			echo "Error: Erase failed with ERROR status."
			return
		}
		sleep 100
	}
	if {$i >= $timeout} {
		echo "Error: Timeout waiting for BUSY status."
		return
	}

	for {set i 0} {$i < $timeout} {incr i} {
		set ERASEALLSTATUS [$dap apreg $ctrl_ap_num 8]
		if {$ERASEALLSTATUS == 1} {
			echo "\[$target\] device has been successfully erased and unlocked."
			break
		}
		if {$ERASEALLSTATUS == 3} {
			echo "Error: Erase failed with ERROR status."
			break
		}
		sleep 100
	}
	if {$i >= $timeout} {
		echo "Error: Timeout waiting for READYTORESET status."
		return
	}

	sleep 10

	$dap apreg $ctrl_ap_num 0 2
	sleep 10
	$dap apreg $ctrl_ap_num 0 0

	$dap apreg $ctrl_ap_num 4 0

	if { $is_cpunet } {
		reset init
	} else {
		sleep 200
		$target arp_examine
		poll on
	}
}

lappend _telnet_autocomplete_skip _nrf_check_ap_lock _nrf_ctrl_ap_recover

if { ![using_hla] } {
	$_TARGETNAME configure -event examine-fail {
		global CTRL_AP_NUM
		set target [target current]
		if { [_nrf_check_ap_lock $CTRL_AP_NUM 1] } {
			nrf54l_mass_erase
			$target arp_examine
		}
	}

	proc nrf54l_mass_erase {} {
		global CTRL_AP_NUM
		_nrf_ctrl_ap_recover $CTRL_AP_NUM
	}
	add_help_text nrf54l_mass_erase "Mass erase flash and unlock nRF54L device"
}

flash bank $_CHIPNAME.flash nrf5 0x00000000 0x0017D000 0 0 $_TARGETNAME
```

## Recovery Command

After patching `openocd.cfg`, this command recovered and flashed the board:

```powershell
west flash --build-dir "C:/ncs_pouch_soil/pouch/examples/ble_gatt/build"
```

The successful output included:

```text
Error: [nrf54l.cpu] Examination failed
[nrf54l.cpu] device has AP lock engaged, trying recover.
[nrf54l.cpu] device has been successfully erased and unlocked.
Info : [nrf54l.cpu] Cortex-M33 r1p0 processor detected
...
downloaded 322768 bytes in 26.308025s (11.981 KiB/s)
shutdown command invoked
```

## Important Aftermath

This recovery performs a mass erase. That means the XIAO application flash and
LittleFS contents are erased.

After recovery and flash, the node credentials must be uploaded again:

```powershell
smpmgr --port COM11 --timeout 10 --line-length 128 --line-buffers 2 file upload `
  C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.crt.der `
  /lfs1/credentials/crt.der

smpmgr --port COM11 --timeout 10 --line-length 128 --line-buffers 2 file upload `
  C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.key.der `
  /lfs1/credentials/key.der
```

The app may need a reset before `smpmgr` responds:

```powershell
pyocd reset -t nrf54l --probe RM6N5DPKBWFA5C7GXATDCFGMSCWVFJMW
```

## What Not To Spend Time On

For the XIAO debugger/CMSIS-DAP connection, `nrfutil device recover` is not the
right path unless the device exposes a supported programming trait. In this
case `nrfutil device list` showed only:

```text
Traits serialPorts, usb
```

That is enough for enumeration, but not enough for `nrfutil device recover` or
`nrfutil device program`.

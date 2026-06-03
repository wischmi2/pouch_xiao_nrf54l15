# Flash XIAO nRF54L15 via pyOCD (CMSIS-DAP / Seeed debugger).
# Prefer this over `west flash` (OpenOCD): OpenOCD may mass-erase the chip on AP-lock recover
# and wipe LittleFS credentials.
#
# Usage:
#   .\scripts\flash_xiao_pyocd.ps1 -Variant uart
#   .\scripts\flash_xiao_pyocd.ps1 -Variant battery
#   .\scripts\flash_xiao_pyocd.ps1 -HexFile path\to\zephyr.hex -ProbeUid WYKL7D7E...
#
# Set probe once per session:
#   $env:PYOCD_PROBE_UID = "WYKL7D7EW6T3VCGIXBB6CHE433NVRQIN"

param(
    [ValidateSet("uart", "battery")]
    [string]$Variant = "battery",
    [string]$HexFile = "",
    [string]$ProbeUid = $env:PYOCD_PROBE_UID,
    [string]$Target = "nrf54l"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path $PSScriptRoot -Parent

if (-not $HexFile) {
    $HexFile = Join-Path $RepoRoot "builds\xiao-ble-gatt-$Variant\zephyr.hex"
}

if (-not (Test-Path $HexFile)) {
    throw "Hex not found: $HexFile - build first (see scripts/build_ble_gatt_xiao_dual.ps1)."
}

$probeArg = @()
if ($ProbeUid) {
    $probeArg = @("--uid", $ProbeUid)
    Write-Host "Probe UID: $ProbeUid"
} else {
    Write-Host "Probe: default (set PYOCD_PROBE_UID or -ProbeUid if multiple debuggers connected)"
}

Write-Host "== pyOCD flash XIAO ($Variant) =="
Write-Host "Hex: $HexFile"

# Sector erase updates only touched regions; avoids OpenOCD ERASEALL recover wiping /lfs1.
pyocd flash --target $Target @probeArg $HexFile
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

pyocd reset --target $Target @probeArg
Write-Host "== Done =="

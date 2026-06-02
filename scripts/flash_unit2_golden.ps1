# Flash the known-good unit 2 full-chip image (v0.1.0-221, Zephyr 4.3.0).
# Keeps unit-3 device cert in LittleFS if that region is outside the programmed span.

param(
    [string]$ProbeUid = "HZ4IDM2WVPSLD62N4HSCBAF4G5DHJYP3"
)

$ErrorActionPreference = "Stop"
$PouchRepoPath = "C:/ncs_pouch_soil/pouch"
$Src = "C:/ncs_pouch_soil/unit2_firmware_clone.bin"
$Golden = "$PouchRepoPath/firmware/unit2_golden.bin"

New-Item -ItemType Directory -Force -Path "$PouchRepoPath/firmware" | Out-Null
if (-not (Test-Path $Golden)) {
    Copy-Item -Force $Src $Golden
}

& "$PouchRepoPath/scripts/flash_xiao_full.ps1" -Image $Golden -ProbeUid $ProbeUid

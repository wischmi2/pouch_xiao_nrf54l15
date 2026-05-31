# Build soil_sensor ble_gatt for XIAO nRF54L15 (NCS 3.3.0 / Zephyr 4.3.99).
# For byte-for-byte unit-2 server-cert behavior you need Zephyr 4.3.0 in ROM;
# that requires a matching NCS+Zephyr pair we cannot Kconfig-resolve here yet.
# Use flash_unit2_golden.ps1 when the built image reports 4.3.99 and fails 0x2700.

param(
    [switch]$SkipWestUpdate,
    [switch]$Flash,
    [string]$ProbeUid = "HZ4IDM2WVPSLD62N4HSCBAF4G5DHJYP3"
)

$ErrorActionPreference = "Stop"
$PouchRepoPath = "C:/ncs_pouch_soil/pouch"

& "$PouchRepoPath/scripts/build_ble_gatt_v010.ps1" @PSBoundParameters

$bin = "$PouchRepoPath/examples/ble_gatt/build/zephyr/zephyr.bin"
$dest = "$PouchRepoPath/firmware/soil_sensor_ble_gatt.bin"
New-Item -ItemType Directory -Force -Path "$PouchRepoPath/firmware" | Out-Null
Copy-Item -Force $bin $dest
Write-Host "== Copied to $dest =="

$ascii = [System.Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($bin))
if ($ascii -match "Zephyr OS v4\.3\.0") {
    Write-Host "== ROM reports Zephyr 4.3.0 (matches unit 2 golden) =="
} elseif ($ascii -match "Zephyr OS v4\.3\.99") {
    Write-Warning "ROM reports Zephyr 4.3.99 — may get 0x2700 on server cert. Use flash_unit2_golden.ps1 for unit-2 behavior."
}

if ($Flash) {
    & "$PouchRepoPath/scripts/flash_xiao_full.ps1" -Image $dest -ProbeUid $ProbeUid
}

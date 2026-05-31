param(
    [string]$Image = "C:/ncs_pouch_soil/pouch/firmware/unit2_golden.bin",
    [string]$ProbeUid = "HZ4IDM2WVPSLD62N4HSCBAF4G5DHJYP3",
    [int]$Size = 0
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Image)) {
    throw "Image not found: $Image"
}

if ($Size -le 0) {
    $Size = (Get-Item $Image).Length
}

$probeArg = @()
if ($ProbeUid -ne "") {
    $probeArg = @("-u", $ProbeUid)
}

Write-Host "== Full flash XIAO nRF54L15 =="
Write-Host "Image : $Image"
Write-Host "Size  : $Size bytes @ 0x0"

pyocd flash -t nrf54l @probeArg --erase sector --format bin -a 0 $Image
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

pyocd reset -t nrf54l @probeArg
Write-Host "== Done. Power-cycle gateway + node. Re-provision /lfs1/credentials if needed. =="

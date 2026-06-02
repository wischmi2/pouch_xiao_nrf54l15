# Build two flash-ready ble_gatt images for XIAO nRF54L15:
#   builds/xiao-ble-gatt-uart/     — USB bench (smpmgr credentials, serial logs)
#   builds/xiao-ble-gatt-battery/  — LiPo-only field deploy (no UART)
#
# Usage (from repo root or scripts/):
#   .\scripts\build_ble_gatt_xiao_dual.ps1
#
# Flash:
#   west flash --hex-file builds/xiao-ble-gatt-uart/zephyr.hex
#   west flash --hex-file builds/xiao-ble-gatt-battery/zephyr.hex

param(
    [string]$BleGattDir = "",
    [string]$Board = "xiao_nrf54l15/nrf54l15/cpuapp",
    [switch]$SkipFlashCopyOnly
)

$ErrorActionPreference = "Stop"

function Resolve-BleGattDir {
    param([string]$Hint)
    if ($Hint -and (Test-Path $Hint)) {
        return (Resolve-Path $Hint).Path
    }
    $candidates = @(
        "C:\Users\Brian\ncs\pouch\examples\ble_gatt"
        (Join-Path (Split-Path $PSScriptRoot -Parent) "examples\ble_gatt")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            return (Resolve-Path $c).Path
        }
    }
    throw "ble_gatt directory not found. Pass -BleGattDir."
}

function Get-RepoRoot {
    param([string]$BleGatt)
    $p = Split-Path $BleGatt -Parent
    while ($p) {
        if (Test-Path (Join-Path $p ".git")) {
            return (Resolve-Path $p).Path
        }
        $p = Split-Path $p -Parent
    }
    return Split-Path $BleGatt -Parent
}

function Build-Variant {
    param(
        [string]$BleGatt,
        [string]$Board,
        [string]$VariantName,
        [string]$ExtraConfArg,
        [string]$OutHex,
        [string]$OutBin
    )

    Write-Host ""
    Write-Host "== Building $VariantName ==" -ForegroundColor Cyan

    Push-Location $BleGatt
    try {
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        if ($ExtraConfArg) {
            & west build -b $Board --pristine --no-sysbuild -- $ExtraConfArg 2>&1 | Out-Null
        } else {
            & west build -b $Board --pristine --no-sysbuild 2>&1 | Out-Null
        }
        $ErrorActionPreference = $prevEap
        if ($LASTEXITCODE -ne 0) {
            throw "west build failed for $VariantName (exit $LASTEXITCODE)"
        }
    } finally {
        Pop-Location
    }

    $hex = Join-Path $BleGatt "build\zephyr\zephyr.hex"
    $bin = Join-Path $BleGatt "build\zephyr\zephyr.bin"

    if (-not (Test-Path $hex)) {
        throw "Missing $hex after $VariantName build"
    }

    Copy-Item -Force $hex $OutHex
    if (Test-Path $bin) {
        Copy-Item -Force $bin $OutBin
    }

    $binSize = 0
    if (Test-Path $bin) {
        $binSize = (Get-Item $bin).Length
    }

    return $binSize
}

function Write-BuildInfo {
    param(
        [string]$DestDir,
        [string]$VariantName,
        [string]$ExtraConfNote,
        [string]$GitCommit,
        [string]$GitDescribe,
        [int]$BinSize
    )

    $info = @"
Variant: $VariantName
Pouch commit: $GitCommit
git describe: $GitDescribe
Built: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

Board: $Board
Command: west build -b $Board --pristine --no-sysbuild $ExtraConfNote

Purpose:
  uart    - Scenario A: USB serial + smpmgr credential upload
  battery - Scenario B: LiPo-only boot (no UART/mcumgr)

Flash (from examples/ble_gatt):
  west flash --hex-file $DestDir\zephyr.hex

zephyr.bin: $BinSize bytes
zephyr.hex: included for programming

See docs/xiao-battery-hardware-and-charging.md
"@

    Set-Content -Path (Join-Path $DestDir "BUILD_INFO.txt") -Value $info -Encoding UTF8
}

$BleGatt = Resolve-BleGattDir -Hint $BleGattDir
$RepoRoot = Get-RepoRoot -BleGatt $BleGatt

Push-Location (Split-Path $BleGatt -Parent | Split-Path -Parent)
try {
    $gitCommit = (git -C $RepoRoot rev-parse --short HEAD 2>$null)
    $gitDescribe = (git -C $RepoRoot describe --tags --always 2>$null)
} catch {
    $gitCommit = "unknown"
    $gitDescribe = "unknown"
} finally {
    Pop-Location
}

$uartDir = Join-Path $RepoRoot "builds\xiao-ble-gatt-uart"
$batteryDir = Join-Path $RepoRoot "builds\xiao-ble-gatt-battery"
New-Item -ItemType Directory -Force -Path $uartDir, $batteryDir | Out-Null

$uartBinSize = Build-Variant -BleGatt $BleGatt -Board $Board -VariantName "xiao-ble-gatt-uart" `
    -ExtraConfArg "" -OutHex (Join-Path $uartDir "zephyr.hex") -OutBin (Join-Path $uartDir "zephyr.bin")
Write-BuildInfo -DestDir $uartDir -VariantName "xiao-ble-gatt-uart (Scenario A USB bench)" `
    -ExtraConfNote "" -GitCommit $gitCommit -GitDescribe $gitDescribe -BinSize $uartBinSize
Write-Host "  -> $uartDir" -ForegroundColor Green

$batteryConf = '-DEXTRA_CONF_FILE=prj_battery.conf'
$batteryBinSize = Build-Variant -BleGatt $BleGatt -Board $Board -VariantName "xiao-ble-gatt-battery" `
    -ExtraConfArg $batteryConf -OutHex (Join-Path $batteryDir "zephyr.hex") -OutBin (Join-Path $batteryDir "zephyr.bin")
Write-BuildInfo -DestDir $batteryDir -VariantName "xiao-ble-gatt-battery (Scenario B LiPo field)" `
    -ExtraConfNote "-- $batteryConf" -GitCommit $gitCommit -GitDescribe $gitDescribe -BinSize $batteryBinSize
Write-Host "  -> $batteryDir" -ForegroundColor Green

Write-Host ""
Write-Host "Done. Flash-ready images:" -ForegroundColor Green
Write-Host "  builds/xiao-ble-gatt-uart/zephyr.hex"
Write-Host "  builds/xiao-ble-gatt-battery/zephyr.hex"
Write-Host ""
Write-Host "Workflow: flash uart -> smpmgr upload creds -> flash battery"

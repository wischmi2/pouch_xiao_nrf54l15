# Flash Seeed XIAO BLE (nRF52840) via Adafruit UF2 bootloader (USB mass storage).
#
# 1. Connect USB.
# 2. Double-tap the reset button (left of the USB port) until a drive named "XIAO BLE" appears.
# 3. Run this script (or pass -WaitSeconds to poll while you double-tap).
#
# Usage:
#   .\scripts\flash_xiao_ble_uf2.ps1
#   .\scripts\flash_xiao_ble_uf2.ps1 -Uf2File C:\path\to\zephyr.uf2
#   .\scripts\flash_xiao_ble_uf2.ps1 -WaitSeconds 60
#   .\scripts\flash_xiao_ble_uf2.ps1 -DriveLetter E
#
# Build first:
#   cd examples\ble_gatt
#   west build -b xiao_ble --pristine --no-sysbuild

param(
    [string]$Uf2File = "",
    [string]$BleGattDir = "",
    [string]$DriveLetter = "",
    [string]$VolumeLabel = "XIAO BLE",
    [int]$WaitSeconds = 0
)

$ErrorActionPreference = "Stop"

function Resolve-BleGattDir {
    param([string]$Hint)
    if ($Hint -and (Test-Path $Hint)) {
        return (Resolve-Path $Hint).Path
    }
    $repoRoot = Split-Path $PSScriptRoot -Parent
    $candidate = Join-Path $repoRoot "examples\ble_gatt"
    if (Test-Path $candidate) {
        return (Resolve-Path $candidate).Path
    }
    throw "ble_gatt directory not found. Pass -BleGattDir."
}

function Get-XiaoBleBootDrive {
    param(
        [string]$Label,
        [string]$Letter
    )

    if ($Letter) {
        $root = "{0}:\" -f $Letter.TrimEnd(':')
        if (-not (Test-Path $root)) {
            throw "Drive not found: $root"
        }
        return (Resolve-Path $root).Path
    }

    $disks = Get-CimInstance -ClassName Win32_LogicalDisk |
        Where-Object { $_.DriveType -eq 2 -and $_.VolumeName -eq $Label }

    if ($disks.Count -eq 0) {
        return $null
    }

    if ($disks.Count -gt 1) {
        $letters = ($disks | ForEach-Object { $_.DeviceID }) -join ", "
        throw "Multiple removable drives labeled '$Label' ($letters). Pass -DriveLetter."
    }

    return "{0}\" -f $disks[0].DeviceID
}

function Wait-XiaoBleBootDrive {
    param(
        [string]$Label,
        [int]$Seconds
    )

    Write-Host "Waiting up to ${Seconds}s for '$Label' (double-tap reset now)..." -ForegroundColor Yellow
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        $drive = Get-XiaoBleBootDrive -Label $Label -Letter ""
        if ($drive) {
            return $drive
        }
        Start-Sleep -Milliseconds 500
    }
    return $null
}

$BleGatt = Resolve-BleGattDir -Hint $BleGattDir

if (-not $Uf2File) {
    $Uf2File = Join-Path $BleGatt "build\zephyr\zephyr.uf2"
}

if (-not (Test-Path $Uf2File)) {
    throw @"
UF2 not found: $Uf2File

Build first:
  cd $BleGatt
  west build -b xiao_ble --pristine --no-sysbuild
"@
}

$Uf2File = (Resolve-Path $Uf2File).Path
$uf2Size = (Get-Item $Uf2File).Length

Write-Host "== UF2 flash XIAO BLE (nRF52840) =="
Write-Host "UF2   : $Uf2File ($uf2Size bytes)"

$destRoot = $null
if ($WaitSeconds -gt 0) {
    $destRoot = Wait-XiaoBleBootDrive -Label $VolumeLabel -Seconds $WaitSeconds
} else {
    $destRoot = Get-XiaoBleBootDrive -Label $VolumeLabel -Letter $DriveLetter
}

if (-not $destRoot) {
    throw @"
Bootloader drive '$VolumeLabel' not found.

1. Connect the XIAO BLE over USB.
2. Double-tap the reset button (left of the USB port).
3. Confirm a removable drive named '$VolumeLabel' appears in File Explorer.
4. Re-run with -WaitSeconds 60 to poll while you double-tap, or -DriveLetter X if the label differs.
"@
}

$destFile = Join-Path $destRoot "zephyr.uf2"
Write-Host "Drive : $destRoot"
Write-Host "Copy  : $destFile"

Copy-Item -LiteralPath $Uf2File -Destination $destFile -Force

Write-Host ""
Write-Host "UF2 copied. The board should reset and launch the new firmware." -ForegroundColor Green
Write-Host "Open the USB CDC COM port for logs; use smpmgr to upload credentials if /lfs1 was cleared."
Write-Host "== Done =="

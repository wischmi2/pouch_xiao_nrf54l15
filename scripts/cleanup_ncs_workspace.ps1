# Remove regenerable west workspace clutter under the NCS root (parent of this repo).
#
# Safe to delete: gateway build dirs, west build trees, build logs.
# NOT touched: pouch/builds/* golden hex archives, pouch/certs, west modules.
#
# Usage (from repo root):
#   .\scripts\cleanup_ncs_workspace.ps1
#   .\scripts\cleanup_ncs_workspace.ps1 -WorkspaceRoot C:\ncs_pouch_soil
#   .\scripts\cleanup_ncs_workspace.ps1 -WhatIf

param(
    [string]$WorkspaceRoot = "",
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

if (-not $WorkspaceRoot) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $WorkspaceRoot = (Resolve-Path $WorkspaceRoot).Path
}

$PouchRoot = Join-Path $WorkspaceRoot "pouch"
if (-not (Test-Path (Join-Path $PouchRoot ".git"))) {
    throw "Expected pouch git repo at $PouchRoot"
}

$targets = @(
    (Join-Path $WorkspaceRoot "build")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612-test")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612-test2")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612-test3")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612-test4")
    (Join-Path $WorkspaceRoot "build-gateway-frdm_rw612-test5")
    (Join-Path $PouchRoot "build")
    (Join-Path $PouchRoot "examples\ble_gatt\build")
    (Join-Path $PouchRoot "examples\ble_gatt\build-*")
)

$logFiles = Get-ChildItem $WorkspaceRoot -Filter "build-gateway*.log" -File -ErrorAction SilentlyContinue

Write-Host "== NCS workspace cleanup =="
Write-Host "Workspace: $WorkspaceRoot"
Write-Host ""

$removed = 0
foreach ($pattern in $targets) {
    $parent = Split-Path $pattern -Parent
    $leaf = Split-Path $pattern -Leaf
    if ($leaf -like "*`**") {
        Get-ChildItem $parent -Directory -Filter $leaf -ErrorAction SilentlyContinue | ForEach-Object {
            if ($WhatIf) {
                Write-Host "[whatif] remove dir $($_.FullName)"
            } else {
                Remove-Item $_.FullName -Recurse -Force
                Write-Host "Removed dir $($_.Name)"
            }
            $removed++
        }
        continue
    }
    if (Test-Path $pattern) {
        if ($WhatIf) {
            Write-Host "[whatif] remove $pattern"
        } else {
            Remove-Item $pattern -Recurse -Force
            Write-Host "Removed $pattern"
        }
        $removed++
    }
}

foreach ($log in $logFiles) {
    if ($WhatIf) {
        Write-Host "[whatif] remove file $($log.FullName)"
    } else {
        Remove-Item $log.FullName -Force
        Write-Host "Removed $($log.Name)"
    }
    $removed++
}

Write-Host ""
if ($WhatIf) {
    Write-Host "WhatIf: $removed item(s) would be removed."
} else {
    Write-Host "Done. $removed item(s) removed."
    Write-Host "Kept: pouch/builds/* (golden hex), pouch/certs/, west modules (zephyr/, nrf/, ...)."
}

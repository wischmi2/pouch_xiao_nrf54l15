param(
    [string]$WorkspaceRoot = "C:/ncs_pouch_soil",
    [string]$PouchRepoPath = "C:/ncs_pouch_soil/pouch",
    [string]$Board = "xiao_nrf54l15/nrf54l15/cpuapp",
    [switch]$SkipWestUpdate,
    [switch]$UseSysbuild
)

$ErrorActionPreference = "Stop"

Write-Host "== pouch XIAO nRF54L15 build helper =="
Write-Host "WorkspaceRoot : $WorkspaceRoot"
Write-Host "PouchRepoPath : $PouchRepoPath"
Write-Host "Board         : $Board"
Write-Host "Sysbuild      : $UseSysbuild"

# Force the NCS-managed Zephyr SDK to avoid host SDK/toolchain mismatches.
$env:ZEPHYR_SDK_INSTALL_DIR = "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"

# Make sure Python-installed console scripts (west/zcbor) are available.
$pythonScripts = "C:/Users/Brian/AppData/Local/Programs/Python/Python312/Scripts"
if (-not ($env:Path -split ";" | Where-Object { $_ -eq $pythonScripts })) {
    $env:Path = "$pythonScripts;$env:Path"
}

Push-Location $WorkspaceRoot
try {
    if (-not $SkipWestUpdate) {
        Write-Host "== Running west update to align workspace revisions =="
        west update
    } else {
        Write-Host "== Skipping west update =="
    }

    Push-Location $PouchRepoPath
    try {
        Write-Host "== Ensuring required Python dependencies =="
        python -m pip install -r requirements.txt
        python -m pip install cryptography

        # Fail fast with a clear message if the board is unavailable.
        $boardName = ($Board -split "/")[0]
        $boardCheck = west boards 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to query west boards."
        }
        if (-not ($boardCheck -match "(?m)^$([regex]::Escape($boardName))$")) {
            Write-Error "Board '$Board' is not available in this workspace."
            Write-Host "Tip: run 'west boards | Select-String xiao' to see available xiao boards."
            Write-Host "This board requires an NCS/Zephyr release that includes xiao_nrf54l15, such as the v3.2.3 stack used by latest pouch."
            exit 1
        }

        Write-Host "== Building ble_gatt example =="
        Push-Location "examples/ble_gatt"
        try {
            if ($UseSysbuild) {
                Write-Host "== Using sysbuild/MCUboot =="
                Write-Host "Note: xiao_nrf54l15 currently fails in MCUboot flash_map_extended.c without board-specific flash metadata."
                west build -b $Board --pristine
            } else {
                Write-Host "== Using app-only build (--no-sysbuild) =="
                west build -b $Board --pristine --no-sysbuild
            }
        }
        finally {
            Pop-Location
        }

        $bin = Join-Path $PouchRepoPath "examples/ble_gatt/build/zephyr/zephyr.bin"
        if (-not (Test-Path $bin)) {
            throw "Build did not produce $bin"
        }
        $ascii = [System.Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($bin))
        if ($ascii -notmatch "Golioth Root X1") {
            throw "Built image missing embedded Golioth Root X1 CA"
        }
        if ($ascii -match "Zephyr OS v4\.3\.0") {
            Write-Host "== Verified: Golioth Root X1 + Zephyr v4.3.0 (unit 2 match) =="
        } elseif ($ascii -match "Zephyr OS v4\.3\.99") {
            Write-Warning "Built image has Zephyr 4.3.99 — server cert may fail (0x2700). Use scripts/flash_unit2_golden.ps1 for unit-2 behavior."
            Write-Host "== Verified: Golioth Root X1 present =="
        } else {
            throw "Built image missing expected Zephyr 4.3.x version string"
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    Pop-Location
}

Write-Host "== Build completed successfully =="

param(
    [string]$WorkspaceRoot = "C:/ncs_pouch_soil",
    [string]$PouchRepoPath = "C:/ncs_pouch_soil/pouch",
    [string]$Board = "nrf52840dk/nrf52840",
    [switch]$SkipWestUpdate
)

$ErrorActionPreference = "Stop"

Write-Host "== pouch v0.1.0 build helper =="
Write-Host "WorkspaceRoot : $WorkspaceRoot"
Write-Host "PouchRepoPath : $PouchRepoPath"
Write-Host "Board         : $Board"

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
        $boardCheck = west boards 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to query west boards."
        }
        if (-not ($boardCheck -match "(?m)^$([regex]::Escape($Board))$")) {
            Write-Error "Board '$Board' is not available in this workspace."
            Write-Host "Tip: run 'west boards | Select-String xiao' to see available xiao boards."
            Write-Host "Known NCS nRF54 board available here: nrf54l15dk/nrf54l15/cpuapp"
            exit 1
        }

        Write-Host "== Building ble_gatt example =="
        Push-Location "examples/ble_gatt"
        try {
            west build -b $Board --pristine
        }
        finally {
            Pop-Location
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

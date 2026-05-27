param(
    [string]$WorkspaceRoot = "",
    [string]$PouchRepoPath = "",
    [string]$Board = "xiao_nrf54l15/nrf54l15/cpuapp",
    [string]$BoardRoot = "",
    [switch]$SkipWestUpdate,
    [switch]$UseSysbuild,
    [switch]$NoJunction
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Resolve-ZephyrSdk {
    param([string]$NcsVersionDir = "")

    $sdkFromEnv = $env:ZEPHYR_SDK_INSTALL_DIR
    if ($sdkFromEnv -and (Test-Path $sdkFromEnv)) {
        return (Resolve-Path $sdkFromEnv).Path
    }

    $candidates = @()

    $toolchainsJson = "C:/ncs/toolchains/toolchains.json"
    if ($NcsVersionDir -and (Test-Path $toolchainsJson)) {
        $meta = Get-Content $toolchainsJson -Raw | ConvertFrom-Json
        $versionName = Split-Path $NcsVersionDir -Leaf
        foreach ($entry in $meta.toolchains) {
            if ($entry.ncs_versions -contains $versionName) {
                $bundle = $entry.identifier.bundle_id
                $candidates += "C:/ncs/toolchains/$bundle/opt/zephyr-sdk"
            }
        }
    }

    $candidates += @(
        "C:/ncs/toolchains/fd21892d0f/opt/zephyr-sdk"
        "C:/ncs/toolchains/0b393f9e1b/opt/zephyr-sdk"
        "C:/ncs/toolchains/66cdf9b75e/opt/zephyr-sdk"
    )
    if (Test-Path "C:/ncs/toolchains") {
        $candidates += Get-ChildItem "C:/ncs/toolchains/*/opt/zephyr-sdk" -Directory -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
    }

    return Resolve-ExistingPath $candidates
}

function Test-NcsToolchainInstall {
    param([string]$Root)
    return $Root -match '^[A-Za-z]:[/\\]ncs[/\\]v\d+\.\d+\.\d+$' -and (Test-Path (Join-Path $Root ".west"))
}

function Test-WestWorkspace {
    param([string]$Root)
    return (Test-Path (Join-Path $Root ".west")) -or (Test-Path (Join-Path $Root "../.west"))
}

function Ensure-PouchModule {
    param(
        [string]$WorkspacePouch,
        [string]$DevPouch
    )

    $workspacePouch = (Resolve-Path $WorkspacePouch).Path
    $devPouch = (Resolve-Path $DevPouch).Path

    if ($workspacePouch -eq $devPouch) {
        Write-Host "== Workspace pouch matches dev repo =="
        return
    }

    if ($NoJunction) {
        Write-Error @"
Workspace pouch ($workspacePouch) is not the dev repo ($devPouch).
Re-run without -NoJunction, or point west manifest pouch at your dev tree.
See docs/v0.1.0-build-steps.md and docs/battery_issues.md.
"@
    }

    $backup = "${workspacePouch}.bak"
    if (Test-Path $workspacePouch) {
        $item = Get-Item $workspacePouch
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            Write-Host "== Workspace pouch is already a junction =="
            return
        }

        if (-not (Test-Path $backup)) {
            Write-Host "== Backing up workspace pouch to $backup =="
            Rename-Item $workspacePouch $backup
        }
        else {
            Write-Host "== Removing existing workspace pouch directory =="
            Remove-Item $workspacePouch -Recurse -Force
        }
    }

    Write-Host "== Linking workspace pouch -> $devPouch =="
    New-Item -ItemType Junction -Path $workspacePouch -Target $devPouch | Out-Null
}

if (-not $WorkspaceRoot) {
    $WorkspaceRoot = Resolve-ExistingPath @(
        $env:POUCH_NCS_ROOT
        "C:/ncs/v3.2.3"
        "C:/ncs/v3.2.0"
        "C:/ncs_pouch_soil"
        "C:/Users/Brian/ncs"
    )
}

if (-not $PouchRepoPath) {
    $PouchRepoPath = Resolve-ExistingPath @(
        $env:POUCH_REPO_PATH
        "C:/Users/Brian/pouch_xiao_nrf54l15"
        (Join-Path $WorkspaceRoot "pouch")
    )
}

if (-not $WorkspaceRoot) {
    Write-Error "No NCS west workspace found. Set POUCH_NCS_ROOT or install NCS under C:/ncs/v3.2.3 (Toolchain Manager). See docs/v0.1.0-build-steps.md."
}

if (-not $PouchRepoPath) {
    Write-Error "No pouch repo found. Set POUCH_REPO_PATH or clone this repo to C:/Users/Brian/pouch_xiao_nrf54l15."
}

$WorkspaceRoot = (Resolve-Path $WorkspaceRoot).Path
$PouchRepoPath = (Resolve-Path $PouchRepoPath).Path
$workspacePouch = Join-Path $WorkspaceRoot "pouch"
$appDir = Join-Path $PouchRepoPath "examples/ble_gatt"
$buildDir = Join-Path $appDir "build"

$useNcsInstall = Test-NcsToolchainInstall $WorkspaceRoot

$sdk = Resolve-ZephyrSdk -NcsVersionDir $(if ($useNcsInstall) { $WorkspaceRoot } else { "" })
if (-not $sdk) {
    Write-Error "Zephyr SDK not found. Install NCS toolchains or set ZEPHYR_SDK_INSTALL_DIR."
}
$env:ZEPHYR_SDK_INSTALL_DIR = $sdk

$pythonScripts = "C:/Users/Brian/AppData/Local/Programs/Python/Python312/Scripts"
if (-not ($env:Path -split ";" | Where-Object { $_ -eq $pythonScripts })) {
    $env:Path = "$pythonScripts;$env:Path"
}

if ($useNcsInstall) {
    $BoardRoot = ""
} elseif (-not $BoardRoot) {
    $BoardRoot = Resolve-ExistingPath @(
        "C:/Users/Brian/zephyr"
    )
}

Write-Host "== pouch XIAO nRF54L15 build helper =="
Write-Host "WorkspaceRoot     : $WorkspaceRoot"
Write-Host "PouchRepoPath     : $PouchRepoPath"
Write-Host "Workspace pouch   : $workspacePouch"
Write-Host "App directory     : $appDir"
Write-Host "Build directory   : $buildDir"
Write-Host "Zephyr SDK        : $sdk"
Write-Host "Board             : $Board"
Write-Host "BoardRoot         : $(if ($BoardRoot) { $BoardRoot } else { '(default)' })"
Write-Host "NCS install layout: $useNcsInstall"
Write-Host "Sysbuild          : $UseSysbuild"

if (-not $useNcsInstall) {
    Ensure-PouchModule -WorkspacePouch $workspacePouch -DevPouch $PouchRepoPath
}
else {
    Write-Host "== Using Toolchain Manager NCS ($WorkspaceRoot); pouch via ZEPHYR_EXTRA_MODULES =="
}

Push-Location $WorkspaceRoot
try {
    if (-not $SkipWestUpdate) {
        Write-Host "== Running west update to align workspace revisions =="
        west update
    }
    else {
        Write-Host "== Skipping west update =="
    }

    Write-Host "== Ensuring required Python dependencies =="
    python -m pip install -r (Join-Path $PouchRepoPath "requirements.txt")
    python -m pip install cryptography

    $boardName = ($Board -split "/")[0]
    if ($BoardRoot) {
        Write-Host "== Using BOARD_ROOT=$BoardRoot (skipping west boards query) =="
    }
    else {
        $boardCheck = west boards 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to query west boards."
        }
        if ($boardCheck -notmatch "(?m)^$([regex]::Escape($boardName))$") {
            Write-Error "Board '$Board' is not available. Run 'west update' for NCS v3.2.3+ or pass -BoardRoot with a Zephyr tree that includes xiao_nrf54l15."
        }
    }

    $cmakeArgs = @()
    if ($useNcsInstall) {
        $cmakeArgs += "-DZEPHYR_EXTRA_MODULES=$($PouchRepoPath -replace '\\', '/')"
    }
    if ($BoardRoot) {
        $cmakeArgs += "-DBOARD_ROOT=$($BoardRoot -replace '\\', '/')"
    }

    Write-Host "== Building ble_gatt example =="
    if ($UseSysbuild) {
        Write-Host "== Using sysbuild/MCUboot (may fail on xiao; see docs/v0.1.0-build-steps.md) =="
        if ($cmakeArgs.Count -gt 0) {
            west build -b $Board -d $buildDir -p always $appDir -- $cmakeArgs
        }
        else {
            west build -b $Board -d $buildDir -p always $appDir
        }
    }
    else {
        Write-Host "== Using app-only build (--no-sysbuild) =="
        if ($cmakeArgs.Count -gt 0) {
            west build -b $Board -d $buildDir -p always --no-sysbuild $appDir -- $cmakeArgs
        }
        else {
            west build -b $Board -d $buildDir -p always --no-sysbuild $appDir
        }
    }
}
finally {
    Pop-Location
}

Write-Host "== Build completed successfully =="
Write-Host "Firmware: $buildDir/zephyr/zephyr.elf"
Write-Host "Hex file: $buildDir/zephyr/zephyr.hex"

# Apply Kconfig fix required for Zephyr v4.3.0 tag + nRF Connect SDK v3.3.0 nrf_security.
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$ZephyrKconfig = Join-Path $Root "zephyr\modules\mbedtls\Kconfig"
$Patch = Join-Path $Root "pouch\patches\zephyr-v4.3.0-disable-mbedtls-builtin.patch"
if (-not (Test-Path $ZephyrKconfig)) {
    throw "Zephyr not found at $ZephyrKconfig (run west update from workspace root)."
}
Push-Location (Join-Path $Root "zephyr")
try {
    git apply --check $Patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        git apply $Patch
        Write-Host "Applied $Patch"
    } else {
        $content = Get-Content $ZephyrKconfig -Raw
        if ($content -notmatch "DISABLE_MBEDTLS_BUILTIN") {
            throw "Zephyr mbedtls Kconfig missing DISABLE_MBEDTLS_BUILTIN; patch failed and tree is unpatched."
        }
        Write-Host "Zephyr mbedtls Kconfig already patched."
    }
} finally {
    Pop-Location
}

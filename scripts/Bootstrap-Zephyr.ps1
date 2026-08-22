[CmdletBinding()]
param(
    [string]$Python312 = ''
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolVenv = Join-Path $projectRoot '.tools\zephyr-venv'
$toolPython = Join-Path $toolVenv 'Scripts\python.exe'
$west = Join-Path $toolVenv 'Scripts\west.exe'

if (-not (Test-Path -LiteralPath $toolPython)) {
    if ([string]::IsNullOrWhiteSpace($Python312)) {
        $launcher = Get-Command py -ErrorAction SilentlyContinue
        if ($null -ne $launcher) {
            try {
                $Python312 = (& $launcher.Source -3.12 -c 'import sys; print(sys.executable)').Trim()
            } catch {
                $Python312 = ''
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($Python312)) {
        throw 'Python 3.12 or newer is required. Install it, or pass -Python312 <python.exe>.'
    }

    $version = & $Python312 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")'
    if ([version]$version -lt [version]'3.12') {
        throw "Zephyr 4.4 requires Python 3.12 or newer; found $version"
    }
    & $Python312 -m venv $toolVenv
}

$env:Path = "$(Join-Path $toolVenv 'Scripts');$env:Path"
& $toolPython -m pip install --disable-pip-version-check 'west==1.5.0'

Push-Location $projectRoot
try {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot '.west'))) {
        & $west init -l firmware
    }
    & $west config manifest.path firmware
    & $west config manifest.file west.yml
    & $west update
    & $west config zephyr.base .deps/zephyr
    & $west zephyr-export

    $zephyrRoot = Join-Path $projectRoot '.deps\zephyr'
    & $toolPython -m pip install `
        --disable-pip-version-check `
        --requirement (Join-Path $zephyrRoot 'scripts\requirements-base.txt') `
        --requirement (Join-Path $zephyrRoot 'scripts\requirements-build-test.txt')
} finally {
    Pop-Location
}

Write-Output 'Zephyr sources and Python dependencies are ready.'
Write-Output 'Next: run Install-ZephyrSdk.ps1, then Build-Zephyr.ps1.'

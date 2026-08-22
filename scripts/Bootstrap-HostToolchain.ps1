[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolVenv = Join-Path $projectRoot '.tools\host-venv'
$toolPython = Join-Path $toolVenv 'Scripts\python.exe'

if (-not (Test-Path -LiteralPath $toolPython)) {
    python -m venv $toolVenv
}

& $toolPython -m pip install --disable-pip-version-check --requirement (Join-Path $projectRoot 'tools\host-requirements.txt')
& $toolPython -c "import cmake, ninja, ziglang; print('Host toolchain ready')"

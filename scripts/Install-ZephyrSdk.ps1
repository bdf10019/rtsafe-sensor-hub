[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolVenv = Join-Path $projectRoot '.tools\zephyr-venv'
$west = Join-Path $toolVenv 'Scripts\west.exe'
$sdkRoot = Join-Path $projectRoot '.tools\zephyr-sdk-1.0.1'
$binRoot = Join-Path $projectRoot '.tools\bin'
$sevenZip = Join-Path $binRoot '7z.exe'

if (-not (Test-Path -LiteralPath $west)) {
    throw 'Run Bootstrap-Zephyr.ps1 before installing the SDK.'
}
if (Test-Path -LiteralPath (Join-Path $sdkRoot 'sdk_version')) {
    Write-Output "Zephyr SDK 1.0.1 is already installed at $sdkRoot"
    exit 0
}

New-Item -ItemType Directory -Path $binRoot -Force | Out-Null
if (-not (Test-Path -LiteralPath $sevenZip)) {
    Invoke-WebRequest -Uri 'https://www.7-zip.org/a/7zr.exe' -OutFile $sevenZip
}

$env:Path = "$binRoot;$(Join-Path $toolVenv 'Scripts');$env:Path"
Push-Location $projectRoot
try {
    & $west sdk install `
        --version 1.0.1 `
        --install-dir $sdkRoot `
        --gnu-toolchains arm-zephyr-eabi
} finally {
    Pop-Location
}

Write-Output "Zephyr SDK 1.0.1 installed at $sdkRoot"

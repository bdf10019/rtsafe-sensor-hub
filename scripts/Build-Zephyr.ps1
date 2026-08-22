[CmdletBinding()]
param(
    [string]$Board = 'qemu_cortex_m3',
    [string]$StageRoot = '',
    [switch]$RunQemu
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolVenv = Join-Path $projectRoot '.tools\zephyr-venv'
$hostVenv = Join-Path $projectRoot '.tools\host-venv'
$west = Join-Path $toolVenv 'Scripts\west.exe'
$python = Join-Path $toolVenv 'Scripts\python.exe'
$sdkRoot = Join-Path $projectRoot '.tools\zephyr-sdk-1.0.1'

foreach ($requiredPath in @($west, $python, $sdkRoot)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Missing Zephyr dependency: $requiredPath. Run the bootstrap scripts first."
    }
}

function Test-AsciiPath([string]$Path) {
    foreach ($character in $Path.ToCharArray()) {
        if ([int]$character -gt 127) { return $false }
    }
    return $true
}

function Copy-BuildTree(
    [string]$Source,
    [string]$Destination,
    [string[]]$ExcludedDirectories = @()
) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $arguments = @($Source, $Destination, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if ($ExcludedDirectories.Count -gt 0) {
        $arguments += '/XD'
        $arguments += $ExcludedDirectories
    }
    $previousPreference = $PSNativeCommandUseErrorActionPreference
    $PSNativeCommandUseErrorActionPreference = $false
    try {
        & robocopy @arguments
        $copyCode = $LASTEXITCODE
    } finally {
        $PSNativeCommandUseErrorActionPreference = $previousPreference
    }
    if ($copyCode -ge 8) {
        throw "robocopy failed with exit code ${copyCode}: $Source"
    }
}

$buildRoot = $projectRoot
$sdkForBuild = $sdkRoot
$pythonForBuild = $python

if ($IsWindows -and -not (Test-AsciiPath $projectRoot)) {
    if ([string]::IsNullOrWhiteSpace($StageRoot)) {
        $StageRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'rtsafe-zephyr-stage'
    }
    $StageRoot = [System.IO.Path]::GetFullPath($StageRoot)
    if (-not (Test-AsciiPath $StageRoot)) {
        throw 'Zephyr on Windows needs an ASCII staging path. Pass -StageRoot with an ASCII-only path.'
    }

    Write-Output "Staging Zephyr build inputs at $StageRoot"
    Copy-BuildTree (Join-Path $projectRoot '.deps\zephyr') (Join-Path $StageRoot '.deps\zephyr') @('.git', '.cache')
    Copy-BuildTree (Join-Path $projectRoot '.deps\modules') (Join-Path $StageRoot '.deps\modules') @('.git')
    Copy-BuildTree (Join-Path $projectRoot 'firmware') (Join-Path $StageRoot 'firmware') @('.git')
    Copy-BuildTree (Join-Path $projectRoot 'include') (Join-Path $StageRoot 'include')
    Copy-BuildTree (Join-Path $projectRoot 'src\core') (Join-Path $StageRoot 'src\core')
    Copy-BuildTree (Join-Path $projectRoot 'tools\zephyr-stage-manifest') (Join-Path $StageRoot 'manifest')
    Copy-BuildTree $sdkRoot (Join-Path $StageRoot 'sdk')

    $buildRoot = $StageRoot
    $sdkForBuild = Join-Path $StageRoot 'sdk'
    Push-Location $buildRoot
    try {
        if (-not (Test-Path -LiteralPath (Join-Path $buildRoot '.west'))) {
            & $west init -l manifest
        }
        & $west config manifest.path manifest
        & $west config manifest.file west.yml
        & $west config zephyr.base .deps/zephyr
    } finally {
        Pop-Location
    }
}

$env:ZEPHYR_SDK_INSTALL_DIR = $sdkForBuild
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
$env:Path = "$(Join-Path $toolVenv 'Scripts');$(Join-Path $hostVenv 'Scripts');$env:Path"
$pythonArgument = '-DPython3_EXECUTABLE=' + ($pythonForBuild -replace '\\', '/')

Push-Location $buildRoot
try {
    & $west build --pristine=always --board $Board firmware\zephyr `
        --build-dir build\zephyr-app -- $pythonArgument '-DBUILD_VERSION=v1.0.0'
    & $west build --pristine=always --board $Board firmware\zephyr\tests\core `
        --build-dir build\zephyr-core-tests -- $pythonArgument '-DBUILD_VERSION=v1.0.0'
    if ($RunQemu) {
        & $west build --build-dir build\zephyr-app --target run
        & $west build --build-dir build\zephyr-core-tests --target run
    }
} finally {
    Pop-Location
}

Write-Output "Zephyr application and ztest images built at $buildRoot\build"
if (-not $RunQemu) {
    Write-Output 'QEMU execution was skipped. Pass -RunQemu when a working QEMU runtime is installed.'
}

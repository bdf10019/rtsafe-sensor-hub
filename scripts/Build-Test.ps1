[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolVenv = Join-Path $projectRoot '.tools\host-venv'
$toolPython = Join-Path $toolVenv 'Scripts\python.exe'

if (-not (Test-Path -LiteralPath $toolPython)) {
    & (Join-Path $PSScriptRoot 'Bootstrap-HostToolchain.ps1')
}

$cmake = Join-Path $toolVenv 'Scripts\cmake.exe'
$ctest = Join-Path $toolVenv 'Scripts\ctest.exe'
$ninja = Join-Path $toolVenv 'Scripts\ninja.exe'
$zig = Join-Path $toolVenv 'Lib\site-packages\ziglang\zig.exe'
$buildDirectory = Join-Path $projectRoot 'build\host'

foreach ($requiredTool in @($cmake, $ctest, $ninja, $zig)) {
    if (-not (Test-Path -LiteralPath $requiredTool)) {
        throw "Required project-local tool is missing: $requiredTool"
    }
}

& $cmake `
    --fresh `
    -S $projectRoot `
    -B $buildDirectory `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_C_COMPILER=$zig" `
    '-DCMAKE_C_COMPILER_ARG1=cc' `
    "-DCMAKE_CXX_COMPILER=$zig" `
    '-DCMAKE_CXX_COMPILER_ARG1=c++' `
    "-DPython3_EXECUTABLE=$toolPython"

& $cmake --build $buildDirectory --parallel
& $ctest --test-dir $buildDirectory --output-on-failure
& $toolPython (Join-Path $projectRoot 'tools\check_public_safety.py') $projectRoot

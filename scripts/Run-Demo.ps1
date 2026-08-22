[CmdletBinding()]
param(
    [ValidateSet(
        'nominal',
        'imu-dropout',
        'force-spike',
        'heartbeat-loss',
        'encoder-jump',
        'deadline-overrun',
        'crc-corruption',
        'mixed'
    )]
    [string]$Scenario = 'force-spike',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'Build-Test.ps1') -Configuration Release
}

$simulator = Join-Path $projectRoot 'build\host\rtsafe-simulator.exe'
$supervisor = Join-Path $projectRoot 'build\host\rtsafe-supervisor.exe'
$output = Join-Path $projectRoot "outputs\demo-$Scenario"
foreach ($executable in @($simulator, $supervisor)) {
    if (-not (Test-Path -LiteralPath $executable)) {
        throw "Missing executable: $executable"
    }
}

& $simulator --scenario $Scenario --cycles 2000 --seed 424242 --output-dir $output
$arguments = @('--input', (Join-Path $output 'telemetry.bin'), '--json', (Join-Path $output 'supervisor.json'))
if ($Scenario -eq 'crc-corruption') { $arguments += '--allow-invalid' }
& $supervisor @arguments

Write-Output "Demo evidence: $output"

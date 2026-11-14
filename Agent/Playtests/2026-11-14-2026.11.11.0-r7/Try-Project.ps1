param(
    [ValidateSet('ai','calm-city','curious-city','tired-bomber','ai-trash')]
    [string]$Role = 'ai',
    [ValidateSet('editor','game')][string]$Mode = 'editor',
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$runtimeRoot = Join-Path $repoRoot 'Game/build/x64/Release/Runtime'
$projects = @{
    'ai' = 'Game/build/x64/Release/Tutorials/AiTutorial/Project.json'
    'calm-city' = 'Engine/build/playtests/20261114-r7/calm-city/CalmCityUpgraded/Project.json'
    'curious-city' = 'Engine/build/playtests/20261114-r7/curious-city/upgraded-city/Project.json'
    'tired-bomber' = 'Engine/build/playtests/20261114-r7/tired-bomber/BomberUpgradedR7/Project.json'
    'ai-trash' = 'Engine/build/playtests/20261114-r7/ai-trash/TrashGameUpgraded/Project.json'
}
$projectPath = Join-Path $repoRoot $projects[$Role]
if (!(Test-Path -LiteralPath $projectPath)) { throw "Project is missing: $projectPath. Rebuild it using the role's REBUILD.md." }
$projectRoot = Split-Path -Parent $projectPath
$manifest = Get-Content -LiteralPath $projectPath -Raw -Encoding UTF8 | ConvertFrom-Json
$modulePath = Join-Path $projectRoot $manifest.module
if (!(Test-Path -LiteralPath $modulePath)) { throw "Build the project first: $modulePath" }
$runtimeExe = Join-Path $runtimeRoot 'EngineRuntime.exe'
if (!(Test-Path -LiteralPath $runtimeExe)) { throw "Runtime is missing: $runtimeExe" }
if ($CheckOnly) {
    [pscustomobject]@{Role=$Role; Project=$projectPath; Module=$modulePath; Runtime=$runtimeExe; Mode=$Mode}
    return
}
$launch = New-Object System.Diagnostics.ProcessStartInfo
$launch.FileName = $runtimeExe
$launch.Arguments = '--project "' + $projectPath + '" --mode ' + $Mode
if ($Mode -eq 'editor') { $launch.Arguments += ' --scene-api' }
$launch.WorkingDirectory = $projectRoot
$launch.UseShellExecute = $false
$launch.CreateNoWindow = $true
[Diagnostics.Process]::Start($launch) | Out-Null

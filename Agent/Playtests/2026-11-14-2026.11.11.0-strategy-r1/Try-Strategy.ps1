param(
    [ValidateSet('game','editor')][string]$Mode = 'game',
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$project = Join-Path $repo 'Engine/build/playtests/strategy-r1/RepairCorridor/Project.json'
$runtime = Join-Path $repo 'Game/build/x64/Release/Runtime/EngineRuntime.exe'
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
$manifest = Get-Content -LiteralPath $project -Raw -Encoding UTF8 | ConvertFrom-Json
$module = Join-Path (Split-Path $project) $manifest.module
if (!(Test-Path -LiteralPath $module) -or !(Test-Path -LiteralPath $runtime)) {
    throw 'Build the project and Runtime first; see strategy/REBUILD.md.'
}
if ($CheckOnly) {
    [pscustomobject]@{Project=$project; Module=$module; Runtime=$runtime; Mode=$Mode}
    return
}
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $runtime
$start.Arguments = '--project "' + $project + '" --mode ' + $Mode
if ($Mode -eq 'editor') { $start.Arguments += ' --scene-api' }
$start.WorkingDirectory = Split-Path $project
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
[Diagnostics.Process]::Start($start) | Out-Null

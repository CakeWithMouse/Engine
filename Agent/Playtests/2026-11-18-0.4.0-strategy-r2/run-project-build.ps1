param(
    [Parameter(Mandatory=$true)][ValidateSet('cmake','msbuild')][string]$BuildSystem,
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$LogName,
    [string]$Project = 'E:\Engine\Engine\build\playtests\strategy-040-r2\RepairCorridor\Project.json'
)
$ErrorActionPreference = 'Stop'
$projectTools = Join-Path $Runtime 'SDK\ProjectTools.ps1'
if (!(Test-Path -LiteralPath $projectTools)) { throw "Missing SDK ProjectTools.ps1: $projectTools" }
$store = 'E:\Engine\Engine\build\playtests\strategy-040-r2\EngineStore'
$log = Join-Path 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2' $LogName
New-Item -ItemType Directory -Force -Path $store | Out-Null
$env:PAINENGINE_ENGINE_STORE = $store
if ($BuildSystem -eq 'cmake') {
    $env:PATH = 'E:\Engine\Engine\build\tools\python\cmake\data\bin;E:\Engine\Engine\build\tools\python\bin;' + $env:PATH
}
try {
    & $projectTools -Action Build -Project $project -Runtime $Runtime -Rebuild *> $log
    if (!$?) { throw "ProjectTools returned failure; inspect $log" }
    [IO.File]::WriteAllText("$log.status", "ProjectTools completed without exception`n")
} catch {
    $_ | Out-String | Add-Content -LiteralPath $log -Encoding UTF8
    [IO.File]::WriteAllText("$log.status", "failed: $_`n")
    exit 1
}

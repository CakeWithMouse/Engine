param([Parameter(Mandatory=$true)][string]$Package,
      [Parameter(Mandatory=$true)][string]$ProjectRoot)
$ErrorActionPreference='Stop'
$Runtime=Join-Path ([IO.Path]::GetFullPath($Package)) 'Runtime'
$Archive=Join-Path ([IO.Path]::GetFullPath($Package)) 'Sources/RepairCorridor-0.4.0-r2-source.zip'
if(Test-Path -LiteralPath $ProjectRoot){throw 'Use a new project directory'}
Expand-Archive -LiteralPath $Archive -DestinationPath $ProjectRoot
& (Join-Path $ProjectRoot 'Build.ps1') -Runtime $Runtime
$state=Get-Content -LiteralPath (Join-Path $ProjectRoot 'Project.code.json') -Raw | ConvertFrom-Json
if($state.state -ne 'ready' -or !(Test-Path -LiteralPath (Join-Path $ProjectRoot $state.module))){throw 'Quickstart module not activated'}
& (Join-Path $Runtime 'EngineRuntime.exe') --project (Join-Path $ProjectRoot 'Project.json') --mode game --frames 60
if($LASTEXITCODE -ne 0){throw 'Quickstart game failed'}
Write-Output 'Quickstart source unpack, CMake build and Runtime60 passed'

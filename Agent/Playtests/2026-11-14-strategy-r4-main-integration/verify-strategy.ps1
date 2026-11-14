param([Parameter(Mandatory=$true)][string]$Output,[Parameter(Mandatory=$true)][string]$Scratch)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
$source=Join-Path $repo 'Agent/Playtests/2026-11-14-2026.11.11.0-strategy-r4/strategy'
$destination=Join-Path $Scratch 'RepairCorridor'
if (Test-Path -LiteralPath $destination) { throw 'Use a new Scratch directory; the game destination already exists.' }
New-Item -ItemType Directory -Path $Output,$Scratch -Force | Out-Null
Expand-Archive -LiteralPath (Join-Path $source 'RepairCorridor-r4-source.zip') -DestinationPath $destination
$project=Join-Path $destination 'Project.json'
$env:PAINENGINE_ENGINE_STORE=Join-Path $Scratch 'EngineStore'
try {
    & "$runtime/SDK/ProjectTools.ps1" -Action Build -Rebuild -Project $project -Runtime $runtime *> (Join-Path $Output 'strategy-build.log')
    if (!$?) { throw 'Project build failed' }
    'Build validated and activated' | Set-Content (Join-Path $Output 'strategy-build.status')
    $request=Get-Content (Join-Path $source 'r4-clean-extract-play-request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $request.project=$project
    $requestPath=Join-Path $Output 'strategy-request.json'
    $request | ConvertTo-Json -Depth 20 | Set-Content $requestPath -Encoding UTF8
    $env:PAINENGINE_STRATEGY_TELEMETRY=Join-Path $Output 'strategy-telemetry.log'
    $env:PAINENGINE_D3D_SCREENSHOT=Join-Path $Output 'strategy-win.bmp'
    $env:PAINENGINE_D3D_SCREENSHOT_FRAME='650'
    & "$runtime/EngineCommandlets.exe" project.input $requestPath (Join-Path $Output 'strategy-result.json') *> (Join-Path $Output 'strategy-play.log')
    $playExit=$LASTEXITCODE
    $playExit | Set-Content (Join-Path $Output 'strategy-play.exit')
    if ($playExit -ne 0) { throw "Play failed: $playExit" }
    Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY,Env:PAINENGINE_D3D_SCREENSHOT,Env:PAINENGINE_D3D_SCREENSHOT_FRAME
    $runtimeArgs=@('--project',('"'+$project+'"'),'--mode','game','--frames','60')
    $process=Start-Process -FilePath "$runtime/EngineRuntime.exe" -ArgumentList $runtimeArgs -WorkingDirectory $destination -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $Output 'strategy-runtime.log') -RedirectStandardError (Join-Path $Output 'strategy-runtime.stderr.log')
    $process.ExitCode | Set-Content (Join-Path $Output 'strategy-runtime.exit')
    if ($process.ExitCode -ne 0) { throw "Runtime failed: $($process.ExitCode)" }
    Write-Output 'Strategy build, Play and Runtime completed; inspect telemetry and screenshot.'
} finally {
    Remove-Item Env:PAINENGINE_ENGINE_STORE,Env:PAINENGINE_STRATEGY_TELEMETRY,Env:PAINENGINE_D3D_SCREENSHOT,Env:PAINENGINE_D3D_SCREENSHOT_FRAME -ErrorAction SilentlyContinue
}

param([Parameter(Mandatory=$true)][string]$Output,
      [Parameter(Mandatory=$true)][string]$Scratch)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location $repo
if(Test-Path -LiteralPath $Scratch){throw 'Use a new scratch directory'}
New-Item -ItemType Directory -Path $Output,$Scratch -Force | Out-Null
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
$env:PATH=(Join-Path $repo 'Engine/ThirdParty/Qt6.7/bin')+';'+(Join-Path $repo 'Engine/vcpkg_installed/x64-windows/bin')+';'+$env:PATH
function Stage([string]$Name,[scriptblock]$Action) {
    $global:LASTEXITCODE=0
    try {
        & $Action *> (Join-Path $Output ($Name+'.log'))
        if($LASTEXITCODE -ne 0){throw "$Name exit $LASTEXITCODE"}
        '0' | Set-Content (Join-Path $Output ($Name+'.exit'))
        Write-Output "$Name passed"
    } catch {
        '1' | Set-Content (Join-Path $Output ($Name+'.exit'))
        Get-Content (Join-Path $Output ($Name+'.log')) -Tail 25
        throw
    }
}
function Build([string]$Project) {
    & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project $Project -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk
}
Stage 'core-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/tests/CoreTests.vcxproj -Configuration Debug -PlatformToolset v143 }
Stage 'core-tests' { & ./Game/build/x64/Debug/Runtime/CoreTests.exe }
Stage 'ai-build' { Build 'Engine/tests/ai/AiTests.vcxproj' }
Stage 'ai-tests' { & ./Engine/build/x64/Release/AiTests.exe }
Stage 'runtime-editor-build' { Build 'Engine/Runtime/EngineRuntime.vcxproj' }
Stage 'fixture-build' { Build 'Engine/tests/ai/diagnostics/DiagnosticsModule.vcxproj' }
Stage 'ui-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EditorUiTest }
foreach($mode in @('ai-diagnostics','camera-guides')) {
    $fixture=Join-Path $Scratch $mode
    New-Item -ItemType Directory -Path $fixture | Out-Null
    Copy-Item Engine/tests/ai/diagnostics/Fixture/* $fixture -Recurse
    Copy-Item Engine/build/x64/Release/AiDiagnosticsFixture/*.dll $fixture
    Stage $mode { & "$runtime/EditorUiTests.exe" $fixture (Join-Path $repo 'Engine/Content') "--$mode" }
}
Stage 'commandlets-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EngineCommandlet }
Stage 'diagnostics-api' {
    & ./Editor/tests/AiDiagnosticsCommandletTests.ps1 -Project (Join-Path $Scratch 'ai-diagnostics/Project.json') -OutputDirectory (Join-Path $Output 'api')
    # The script asserts an expected native rejection last, leaving exit code 2.
    # A failed assertion throws; successful completion is the script's result.
    $global:LASTEXITCODE=0
}
$r5=Join-Path $repo 'Agent/Playtests/2026-11-14-2026.11.11.0-strategy-r5/strategy'
$game=Join-Path $Scratch 'RepairCorridor'
Expand-Archive -LiteralPath "$r5/RepairCorridor-r5-source.zip" -DestinationPath $game
$env:PAINENGINE_ENGINE_STORE=Join-Path $Scratch 'EngineStore'
Stage 'game-build' { & "$runtime/SDK/ProjectTools.ps1" -Action Build -Rebuild -Project "$game/Project.json" -Runtime $runtime }
$request=Get-Content "$r5/r5-final-clean-play-request.json" -Raw | ConvertFrom-Json
$request.project=Join-Path $game 'Project.json'
$request | ConvertTo-Json -Depth 30 | Set-Content "$Output/game-request.json" -Encoding utf8
$env:PAINENGINE_STRATEGY_TELEMETRY=Join-Path $Output 'game-telemetry.log'
Stage 'game-play' { & "$runtime/EngineCommandlets.exe" project.input "$Output/game-request.json" "$Output/game-result.json" }
Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY
$result=Get-Content "$Output/game-result.json" -Raw|ConvertFrom-Json
$diagnostics=@($result.steps | Where-Object kind -eq diagnostics)[-1].diagnostics
if($diagnostics.details -notmatch 'Arrived' -or [Math]::Abs($diagnostics.radius-.4)-gt .00001 -or $diagnostics.path-ne 'found'){throw 'Strategy diagnostics regression'}
Stage 'game-runtime60' { & "$runtime/EngineRuntime.exe" --project "$game/Project.json" --mode game --frames 60 }
if((Get-Content "$Output/game-runtime60.log" -Raw)-notmatch 'GameUpdates=60'){throw 'Runtime did not produce 60 updates'}
Remove-Item Env:PAINENGINE_ENGINE_STORE
Get-FileHash "$runtime/SDK/Lib/Engine.lib" | ConvertTo-Json | Set-Content "$Output/sdk-hash.json"

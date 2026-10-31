param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$build="$repo/Engine/BuildSupport/Invoke-MSBuild.ps1"
if(!$SkipBuild){
    & $build -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration $Configuration -EnableAssimp
    & $build -Project "$repo/Engine/tests/EngineTests.vcxproj" -Configuration $Configuration -EnableAssimp
    & $build -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EditorUiTest
}
& "$repo/Engine/build/x64/$Configuration/EngineTests.exe"
if($LASTEXITCODE){throw 'Core authoring tests failed'}
$runtime="$repo/Game/build/x64/$Configuration/Runtime"
$launcher="$repo/Game/build/x64/$Configuration/EngineLauncher.exe"
$work="$repo/Engine/build/authoring-workflow/$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $work -Force | Out-Null
$env:PAINENGINE_ENGINE_STORE="$work/Engines"
foreach($name in @('Assets','Properties')){
    $project="$work/$name"
    $p=Start-Process -FilePath $launcher -ArgumentList @('--create-project',('"'+$project+'"'),$name) -WindowStyle Hidden -PassThru -Wait
    if($p.ExitCode){throw 'Cannot create authoring fixture'}
    $mode='--authoring'
    if($name -eq 'Properties'){
        & "$runtime/SDK/ProjectTools.ps1" -Action Enable -Project "$project/Project.json" -Runtime $runtime
        $mode='--properties'
    }
    & "$runtime/EditorUiTests.exe" $project "$runtime/EngineContent" $mode
    if($LASTEXITCODE){throw "$name UI checks failed"}
}
Write-Output "Authoring workflow passed. Fixtures: $work"

param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$root=Join-Path $repo ('Engine/build/workflow-delivery/'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
if(!$SkipBuild){
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration Release -EnableAssimp
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration Release -EnableAssimp -EditorUiTest
}
$env:TEMP=$root; $env:TMP=$root; $env:PAINENGINE_ENGINE_STORE=Join-Path $root 'EmptyStore'
$project=Join-Path $root 'Проект с пробелами'
$launcher=Start-Process -FilePath "$repo/Game/build/x64/Release/EngineLauncher.exe" -ArgumentList @('--create-project',('"'+$project+'"'),'Workflow') -WindowStyle Hidden -PassThru -Wait
if($launcher.ExitCode){throw 'Project creation failed'}
& "$runtime/EditorUiTests.exe" $project "$runtime/EngineContent" --workflow *> "$root/groups-and-crash.log"
if($LASTEXITCODE){throw "Group UI failed: $root/groups-and-crash.log"}
& "$runtime/EditorUiTests.exe" $project "$runtime/EngineContent" --recovery-check *> "$root/recovery.log"
if($LASTEXITCODE){throw "Recovery failed: $root/recovery.log"}
$manifest=Join-Path $project 'Project.json'; $before=(Get-FileHash -LiteralPath $manifest).Hash
$package=Join-Path $root 'Игра для друга'
& "$runtime/EngineRuntime.exe" --export-project $manifest --destination $package *> "$root/export.log"
if($LASTEXITCODE){throw "Export failed: $root/export.log"}
if((Get-FileHash -LiteralPath $manifest).Hash -ne $before){throw 'Export changed source project'}
if(!(Test-Path -LiteralPath "$package/Start.cmd") -or !(Test-Path -LiteralPath "$package/ExportReport.json")){throw 'Package entrypoint or report missing'}
foreach($dependency in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if(!(Test-Path -LiteralPath "$package/Runtime/$dependency")){throw "Packaged VC++ dependency missing: $dependency"}
}
$moved=Join-Path $root 'Source unavailable'
$prefix=[IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar
foreach($target in @($project,$moved)){if(![IO.Path]::GetFullPath($target).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Move outside test workspace'}}
Move-Item -LiteralPath $project -Destination $moved
try {
    & "$package/Runtime/EngineRuntime.exe" --project "$package/Game/Project.json" --mode game --frames 3 *> "$root/portable.log"
    if($LASTEXITCODE){throw 'Packaged game depends on source project'}
} finally { Move-Item -LiteralPath $moved -Destination $project }
& "$runtime/EngineRuntime.exe" --export-project $manifest --destination $package *> "$root/reject-overwrite.log"
if(!$LASTEXITCODE){throw 'Export overwrote existing package'}
& "$runtime/EngineRuntime.exe" --export-project $manifest --destination "$project/Export" *> "$root/reject-nested.log"
if(!$LASTEXITCODE -or (Test-Path -LiteralPath "$project/Export")){throw 'Nested export accepted'}
New-Item -ItemType Directory -Path "$project/Content/Materials" -Force | Out-Null
[IO.File]::WriteAllText("$project/Content/Materials/Broken.mat",'{"version":1,"shader":"missing.hlsl"}')
& "$runtime/EngineRuntime.exe" --export-project $manifest --destination "$root/Rejected" *> "$root/reject-asset.log"
if(!$LASTEXITCODE -or (Test-Path -LiteralPath "$root/Rejected")){throw 'Invalid assets were published'}
[IO.File]::WriteAllText("$root/Outside.hlsl",'// Test dependency outside project content')
[IO.File]::WriteAllText("$project/Content/Materials/Broken.mat",'{"version":1,"shader":"/Game/../../Outside.hlsl"}')
& "$runtime/EngineRuntime.exe" --export-project $manifest --destination "$root/RejectedOutside" *> "$root/reject-outside.log"
if(!$LASTEXITCODE -or (Test-Path -LiteralPath "$root/RejectedOutside")){throw 'Outside Content dependency was published'}
Write-Output "Group UI, crash recovery, startup isolation, portable export and rejection tests passed. Fixture: $root"

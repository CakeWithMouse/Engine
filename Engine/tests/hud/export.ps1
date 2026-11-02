param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if(!$SkipBuild){ & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration Release -EnableAssimp }
$root=Join-Path $repo ('Engine/build/hud-export/'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root)|Out-Null
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
$project=Join-Path $root 'Проект HUD'
$env:PAINENGINE_ENGINE_STORE=Join-Path $root 'EngineStore'
$created=Start-Process -FilePath "$repo/Game/build/x64/Release/EngineLauncher.exe" -ArgumentList @('--create-project',('"'+$project+'"'),'HudExport') -WindowStyle Hidden -PassThru -Wait
if($created.ExitCode){throw 'Create project failed'}
if(Test-Path -LiteralPath "$project/Content/ProjectHud.txt"){throw 'New project unexpectedly selects a HUD'}
[IO.Directory]::CreateDirectory("$project/Content/UI")|Out-Null
[IO.File]::WriteAllText("$project/Content/UI/Hud.ui",'<UI><Panel rect="16 16 280 100" color="#205080ff"><Label rect="12 12 240 30" text="HUD из экспорта"/><Button name="play" rect="12 48 200 36" text="Кнопка"/></Panel></UI>')
[IO.File]::WriteAllText("$project/Content/ProjectHud.txt",'UI/Hud.ui')
$package=Join-Path $root 'Готовая игра'
& "$runtime/EngineRuntime.exe" --export-project "$project/Project.json" --destination $package *> "$root/export.log"
if($LASTEXITCODE){throw "Export failed: $root/export.log"}
foreach($file in @('Game/Content/ProjectHud.txt','Game/Content/UI/Hud.ui','Runtime/EngineContent/Shaders/UI.hlsl')){if(!(Test-Path -LiteralPath (Join-Path $package $file))){throw "Missing exported HUD resource: $file"}}
$smoke=Get-Content "$package/GameSmoke.log" -Raw
if(!$smoke.Contains('FontAtlas:') -or !$smoke.Contains('Shaders/UI.hlsl')){throw 'Export smoke did not draw selected HUD'}
$hidden=Join-Path $root 'Source unavailable'
$prefix=[IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar
foreach($path in @($project,$hidden)){if(![IO.Path]::GetFullPath($path).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Move outside test fixture'}}
Move-Item -LiteralPath $project -Destination $hidden
try {
    & "$package/Runtime/EngineRuntime.exe" --project "$package/Game/Project.json" --mode game --frames 5 *> "$root/portable.log"
    if($LASTEXITCODE){throw 'Export requires original project'}
    if(!(Get-Content "$root/portable.log" -Raw).Contains('FontAtlas:')){throw 'Portable game did not create HUD'}
} finally { Move-Item -LiteralPath $hidden -Destination $project }
[IO.File]::WriteAllText("$project/Content/ProjectHud.txt",'../outside.ui')
& "$runtime/EngineRuntime.exe" --export-project "$project/Project.json" --destination "$root/Rejected" *> "$root/rejected.log"
if(!$LASTEXITCODE -or (Test-Path -LiteralPath "$root/Rejected")){throw 'Export accepted HUD outside Content'}
[IO.File]::WriteAllText("$project/Content/ProjectHud.txt",'UI/missing.ui')
& "$runtime/EngineRuntime.exe" --export-project "$project/Project.json" --destination "$root/Missing" *> "$root/missing.log"
if(!$LASTEXITCODE -or (Test-Path -LiteralPath "$root/Missing")){throw 'Export accepted missing HUD'}
Write-Output "HUD export and portable launch passed: $root"

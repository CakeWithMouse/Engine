param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if(!$SkipBuild){
 & "$repo/Engine/build.ps1" -Target EngineLauncher -Configuration $Configuration -EnableAssimp $true
 & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/CodeProjectTests.vcxproj" -Configuration $Configuration -EnableAssimp
}
& "$repo/Engine/build/x64/$Configuration/CodeProjectTests.exe"
if($LASTEXITCODE){throw 'Core contract tests failed'}
$runtime="$repo/Game/build/x64/$Configuration/Runtime"
$launcher="$repo/Game/build/x64/$Configuration/EngineLauncher.exe"
$work=Join-Path $repo ('Engine/build/code-projects/'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$env:PAINENGINE_ENGINE_STORE=Join-Path $work 'Engines'
$project=Join-Path $work 'Проект с пробелами'
$manifest=Join-Path $project 'Project.json'
$script="$runtime/SDK/ProjectTools.ps1"
$utf8=[Text.UTF8Encoding]::new($false)
$script:checks=0
function Check([bool]$value,[string]$message){if(!$value){throw $message};$script:checks++}
function Launch([string[]]$Arguments){
 $p=Start-Process -FilePath $launcher -ArgumentList $Arguments -WindowStyle Hidden -PassThru
 if(!$p.WaitForExit(60000)){$p.Kill();throw 'Launcher timeout'}
 Check ($p.ExitCode -eq 0) ('Launcher: '+($Arguments -join ' '))
}
Launch @('--create-project',('"'+$project+'"'),'CodeTest')
$scene=Join-Path $project 'Content/Scenes/Main.scene.json'
$before=(Get-FileHash -LiteralPath $scene).Hash
& $script -Action Enable -Project $manifest -Runtime $runtime
$projectInfo=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
Check ($projectInfo.module -ne 'Game.dll') 'Conversion selected a compiled DLL'
Check ((Get-FileHash -LiteralPath $scene).Hash -eq $before) 'Conversion changed scene bytes'
Check (Test-Path -LiteralPath "$project/Source/ProjectGame.cpp") 'Game scaffold missing'
Check ((Get-Content -LiteralPath "$project/Source/ProjectGame.cpp" -Raw).Contains('Game::TickComponents(deltaTime)')) 'Parent call missing'
& "$PSScriptRoot/run-starter.ps1" -Project $project -Runtime $runtime
$state=Get-Content -LiteralPath "$project/Project.code.json" -Raw | ConvertFrom-Json
& $script -Action Build -Project $manifest -Runtime $runtime
Check ((Get-Content -LiteralPath "$project/Project.code.json" -Raw | ConvertFrom-Json).buildId -eq $state.buildId) 'No-op build changed module identity'
$held=[IO.File]::Open((Join-Path $project '.project-build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$failed=$false
try { try { & $script -Action AddClass -Project $manifest -Runtime $runtime -Name ConcurrentClass -Kind Object } catch { $failed=$true } }
finally { $held.Dispose() }
Check ($failed -and !(Test-Path -LiteralPath "$project/Source/ConcurrentClass.h")) 'Concurrent code operation was not rejected'
$componentHeader=Get-Content -LiteralPath "$project/Source/ProjectComponent.h" -Raw
$codec=@'
    int savedValue = 7;
    std::string SaveProjectData() const override { return nlohmann::json{{"version",1},{"value",savedValue}}.dump(); }
    void LoadProjectData(const std::string& text) override {
        if(text.empty()) return;
        auto data=nlohmann::json::parse(text);
        if(data.at("version")!=1) throw std::runtime_error("Unsupported schema");
        savedValue=data.at("value").get<int>();
    }
    void Tick(float deltaTime) override;
'@
[IO.File]::WriteAllText("$project/Source/ProjectComponent.h",("#include <nlohmann/json.hpp>`n"+$componentHeader.Replace('    void Tick(float deltaTime) override;',$codec)),$utf8)
& $script -Action AddClass -Project $manifest -Runtime $runtime -Name RulesObject -Kind Object
& $script -Action AddClass -Project $manifest -Runtime $runtime -Name SecondGame -Kind Game
& $script -Action AddClass -Project $manifest -Runtime $runtime -Name ChildComponent -Kind Component -Base ProjectComponent -BaseHeader ProjectComponent.h
& $script -Action Build -Project $manifest -Runtime $runtime
$types=Get-Content -LiteralPath "$project/ProjectTypes.tsv" -Raw
Check ($types.Contains("Object`tRulesObject") -and $types.Contains("Game`tSecondGame") -and $types.Contains("Component`tChildComponent")) 'Catalog missing a type family'
Check ((Get-Content -LiteralPath "$project/Source/ChildComponent.cpp" -Raw).Contains('ProjectComponent::Tick')) 'Wrong immediate parent call'
$sceneData=Get-Content -LiteralPath $scene -Raw | ConvertFrom-Json
$object=$sceneData.objects[1] | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$object.id=20; $object.name='Rules';$object.type='RulesObject';$sceneData.objects += $object
[IO.File]::WriteAllText($scene,($sceneData|ConvertTo-Json -Depth 20),$utf8)
[IO.File]::WriteAllText("$project/Content/ProjectGame.txt","SecondGame`n",$utf8)
Launch @('--smoke-project',('"'+$manifest+'"'))
Launch @('--smoke-project-editor',('"'+$manifest+'"'))
$lastModule=(Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).module
[IO.File]::WriteAllText("$project/Source/Broken.cpp",'THIS IS NOT VALID C++',$utf8)
$failed=$false;try{& $script -Action Build -Project $manifest -Runtime $runtime}catch{$failed=$true}
Check $failed 'Broken C++ accepted'
Check ((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).module -eq $lastModule) 'Failed compile replaced active DLL'
Remove-Item -LiteralPath "$project/Source/Broken.cpp"
[IO.File]::WriteAllText("$project/Content/ProjectGame.txt","MissingGame`n",$utf8)
Add-Content -LiteralPath "$project/Source/ProjectComponent.cpp" -Value '// Force candidate validation'
$failed=$false;try{& $script -Action Build -Project $manifest -Runtime $runtime}catch{$failed=$true}
Check $failed 'Missing selected Game accepted'
Check ((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).module -eq $lastModule) 'Failed validation replaced active DLL'
[IO.File]::WriteAllText("$project/Content/ProjectGame.txt","SecondGame`n",$utf8)
$gameSource = Get-Content -LiteralPath "$project/Source/SecondGame.cpp" -Raw
[IO.File]::WriteAllText("$project/Source/SecondGame.cpp",("#include <stdexcept>`n"+$gameSource.Replace('Game::AfterInitialize();','Game::AfterInitialize(); throw std::runtime_error("Startup rejected for test");')),$utf8)
$failed=$false;try{& $script -Action Build -Project $manifest -Runtime $runtime}catch{$failed=$true}
Check $failed 'Failing Game initialization accepted'
Check ((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).module -eq $lastModule) 'Failed initialization replaced active DLL'
[IO.File]::WriteAllText("$project/Source/SecondGame.cpp",$gameSource,$utf8)
$originalSource=Get-Content -LiteralPath "$project/Source/RulesObject.h" -Raw
$failed=$false;try{& $script -Action AddClass -Project $manifest -Runtime $runtime -Name RulesObject -Kind Object}catch{$failed=$true}
Check ($failed -and (Get-Content -LiteralPath "$project/Source/RulesObject.h" -Raw) -eq $originalSource) 'Duplicate wizard overwrote source'
& $script -Action Build -Project $manifest -Runtime $runtime
Write-Output "Code project workflow: $script:checks checks passed. Fixture: $project"

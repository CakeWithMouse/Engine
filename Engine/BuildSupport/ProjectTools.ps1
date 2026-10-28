param(
    [ValidateSet('Enable','Build','AddClass')][string]$Action='Build',
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Runtime='', [string]$Name='',
    [ValidateSet('Component','Character','Game','Object')][string]$Kind='Component',
    [string]$Base='', [string]$BaseHeader=''
)
$ErrorActionPreference='Stop'
if (-not ('ProjectFileCommit' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ProjectFileCommit {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern bool MoveFileEx(string source, string destination, uint flags);
}
'@
}
$encoding = [Text.UTF8Encoding]::new($false)
$projectFile = [IO.Path]::GetFullPath($Project)
$projectRoot = Split-Path $projectFile
$pin = Get-Content -LiteralPath (Join-Path $projectRoot 'Engine.lock.json') -Raw | ConvertFrom-Json
if (!$Runtime) {
    $store = if ($env:PAINENGINE_ENGINE_STORE) { $env:PAINENGINE_ENGINE_STORE } else { Join-Path $env:LOCALAPPDATA 'PainEngine/Engines' }
    $Runtime = Join-Path $store ($pin.engineVersion+'/'+$pin.engineConfiguration)
}
$Runtime = [IO.Path]::GetFullPath($Runtime)
$sdk = Join-Path $Runtime 'SDK'
$sdkInfo = Get-Content -LiteralPath (Join-Path $sdk 'Sdk.json') -Raw | ConvertFrom-Json
if ($sdkInfo.version -ne $pin.engineVersion -or $sdkInfo.configuration -ne $pin.engineConfiguration -or $sdkInfo.platform -ne 'x64') { throw 'SDK does not match Engine.lock.json. Upgrade a project copy explicitly.' }
$buildLock = $null
try { $buildLock = [IO.File]::Open((Join-Path $projectRoot '.project-build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
catch { throw 'Another project code operation is running, or the project folder is not writable.' }
try {
function WriteNew([string]$relative, [string]$text) {
    $text = $text.Replace('__ENGINE_VERSION__',$pin.engineVersion).Replace('__ENGINE_CONFIGURATION__',$pin.engineConfiguration).Replace('__ENGINE_TOOLSET__',$sdkInfo.toolset)
    $file = Join-Path $projectRoot $relative
    if (Test-Path -LiteralPath $file) { throw "File already exists; preserved: $file" }
    [IO.Directory]::CreateDirectory((Split-Path $file)) | Out-Null
    [IO.File]::WriteAllText($file,$text,$encoding)
}
function WriteJson([string]$file, $data) {
    $temp = $file+'.tmp.'+[guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($temp,($data | ConvertTo-Json -Depth 20),$encoding)
    if (![ProjectFileCommit]::MoveFileEx($temp,$file,9)) { throw "Cannot commit project metadata (Windows error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())). Previous file preserved." }
}
function AddClass([string]$className,[string]$family,[string]$parent,[string]$include) {
    $className=$className.Trim(); $parent=$parent.Trim(); $include=$include.Trim()
    if ($family -eq 'Character') { if (!$parent) { $parent='Character' }; if (!$include) { $include='Engine/Components/Character.h' }; $family='Component' }
    if ($className -cnotmatch '^[A-Z_a-z][A-Z_a-z0-9]*$') { throw "Invalid class name '$className'. Use Latin letters, digits and underscore; do not start with a digit. Example: MyCharacter (without .h/.cpp, spaces or Cyrillic)." }
    $reserved = 'alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq Game GameComponent Character SceneCamera PlaySettings Module'.Split(' ')
    if ($className -cin $reserved -or $className -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$') { throw "Class name '$className' is reserved. Choose a new name, for example MyCharacter." }
    if (!$parent) { $parent = if($family -eq 'Game'){'Game'}elseif($family -eq 'Component'){'GameComponent'}else{''} }
    if ($className -ceq $parent) { throw "Class '$className' cannot inherit from itself. Choose a different class name." }
    if ($parent -and $parent -cnotmatch '^[A-Z_a-z][A-Z_a-z0-9]*(::[A-Z_a-z][A-Z_a-z0-9]*)*$') { throw 'Invalid base C++ type' }
    if (!$include) { $include = if($family -eq 'Game'){'Engine/MainGame/BaseGameClass/Game.h'}elseif($family -eq 'Component'){'Engine/Components/GameComponents.h'}else{'Engine/Components/ProjectObject.h'} }
    if ($include -match '["<>\r\n]' -or $include.Contains('..')) { throw 'Invalid base header' }
    foreach($ext in @('h','cpp')) { if(Test-Path -LiteralPath (Join-Path $projectRoot "Source/$className.$ext")){throw "Class source already exists; preserved: Source/$className.$ext. Choose a new class name."} }
    $inherit = if($parent){' : public '+$parent}else{''}
    $declarations = ''
    $body = '#include "'+$className+'.h"'+"`n"
    if($family -eq 'Component') { $declarations = "    void BeginPlay() override;`n    void Tick(float deltaTime) override;"; $body += "void $className"+'::BeginPlay() { '+$parent+"::BeginPlay(); }`n"; $body += "void $className"+'::Tick(float deltaTime) { '+$parent+"::Tick(deltaTime); }`n" }
    if($family -eq 'Game') {
        $declarations = "    void AfterInitialize() override;`n    void PreUpdate(float deltaTime) override;`n    void TickComponents(float deltaTime) override;"
        $body += "void $className"+'::AfterInitialize() { '+$parent+"::AfterInitialize(); }`n"
        $body += "void $className"+'::PreUpdate(float deltaTime) { '+$parent+"::PreUpdate(deltaTime); }`n"
        $body += "void $className"+'::TickComponents(float deltaTime) { '+$parent+"::TickComponents(deltaTime); }`n"
    }
    $macro = 'ENGINE_'+$family.ToUpperInvariant()
    $header = '#pragma once'+"`n"+'#include <Engine/Components/ComponentAnnotations.h>'+"`n"+'#include "'+$include+'"'+"`n"+$macro+'("'+$className+'")'+"`nclass $className$inherit`n{`npublic:`n    $className() = default;`n$declarations`n};`n"
    WriteNew "Source/$className.h" $header
    WriteNew "Source/$className.cpp" $body
}
$codeFile = Join-Path $projectRoot 'Project.code.json'
if ($Action -eq 'Enable' -and !(Test-Path -LiteralPath $codeFile)) {
    $manifest = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
    $original = [IO.Path]::GetFullPath((Join-Path $projectRoot $manifest.module))
    $hash = (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash
    $known = @((Get-FileHash -LiteralPath (Join-Path $Runtime 'Templates/SceneProject/Game.dll') -Algorithm SHA256).Hash)
    $compat = Join-Path $Runtime 'TemplateCompatibility.json'
    if (Test-Path -LiteralPath $compat) { $known += (Get-Content -LiteralPath $compat -Raw | ConvertFrom-Json).SceneProject }
    if ($hash -notin $known) { throw 'Custom DLL detected. Attach its existing sources; conversion cannot recover C++ from a DLL.' }
    foreach($path in @('Source','Game.vcxproj','Game.sln','Build.ps1')) { if(Test-Path -LiteralPath (Join-Path $projectRoot $path)){throw "Conversion would overwrite $path"} }
    WriteNew 'Source/RotatingCube.h' @'
#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>

ENGINE_COMPONENT("RotatingCube")
class RotatingCube : public CubeComponent
{
public:
    RotatingCube();
    void Tick(float deltaTime) override;
};
'@
    WriteNew 'Source/RotatingCube.cpp' @'
#include "RotatingCube.h"
#include <cmath>

RotatingCube::RotatingCube()
{
    InitCube();
    SetColor({1.0f, 0.65f, 0.2f, 1.0f});
}

void RotatingCube::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);
    auto rotation = GetRotation();
    // SetRotation uses degrees. deltaTime is scaled simulation time.
    rotation.y = std::fmod(rotation.y + 90.0f * deltaTime, 360.0f);
    SetRotation(rotation);
}
'@
    WriteNew 'Source/ProjectGame.h' @'
#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/MainGame/BaseGameClass/Game.h>

ENGINE_GAME("ProjectGame")
class ProjectGame : public Game
{
public:
    void AfterInitialize() override;
    void PreUpdate(float deltaTime) override;
    void TickComponents(float deltaTime) override;
};
'@
    WriteNew 'Source/ProjectGame.cpp' @'
#include "ProjectGame.h"
#include "RotatingCube.h"
#include <stdexcept>

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    // One full turn in four real seconds, independent of the frame rate.
    SetSimulationTimeScale(1.0f);
    auto& scene = GetScene();
    // A saved RotatingCube already supplies the example after loading/Play.
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<RotatingCube*>(entry.second.get())) return;
    std::unique_ptr<GameComponent> cube = std::make_unique<RotatingCube>();
    cube->SetPosition({2.0f, 1.0f, 0.0f});
    std::string name = "RotatingCube";
    for (unsigned suffix = 2; scene.FindComponentByName(name); ++suffix)
        name = "RotatingCube" + std::to_string(suffix);
    const auto added = scene.AddComponent(name, cube, "RotatingCube");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add the starter RotatingCube");
}

void ProjectGame::PreUpdate(float deltaTime) { Game::PreUpdate(deltaTime); }
void ProjectGame::TickComponents(float deltaTime) { Game::TickComponents(deltaTime); }
'@
    AddClass 'ProjectComponent' 'Component' '' ''
    WriteNew 'Source/Module.cpp' @'
#include "GeneratedComponents.h"
#include <Engine/Project/ProjectModule.h>
static ComponentTypeCatalog ProjectComponentTypes() {
    static const auto types = CombineProjectTypes(EngineGenerated::Project::GetComponentTypes());
    return {types.data(), types.size()};
}
static std::unique_ptr<Game> ProjectGameFactory() { return SelectProjectGame(EngineGenerated::Project::GetGameTypes()); }
#define SCENE_MODULE_CREATE_GAME ProjectGameFactory
#include <Engine/Project/SceneModule.inl>
'@
    WriteNew 'Game.vcxproj' @'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations">
<ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
<ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
</ItemGroup>
<PropertyGroup Label="Globals"><ProjectGuid>{EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}</ProjectGuid><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<PropertyGroup>
<EnginePinnedVersion>$([System.Text.RegularExpressions.Regex]::Replace($([System.IO.File]::ReadAllText('$(MSBuildProjectDirectory)\Engine.lock.json')), '(?s).*"engineVersion"\s*:\s*"([^"]+)".*', '$1'))</EnginePinnedVersion>
<EnginePinnedConfiguration>$([System.Text.RegularExpressions.Regex]::Replace($([System.IO.File]::ReadAllText('$(MSBuildProjectDirectory)\Engine.lock.json')), '(?s).*"engineConfiguration"\s*:\s*"([^"]+)".*', '$1'))</EnginePinnedConfiguration>
<EngineSdk Condition="'$(EngineSdk)'=='' and '$(PAINENGINE_ENGINE_STORE)'!=''">$(PAINENGINE_ENGINE_STORE)\$(EnginePinnedVersion)\$(EnginePinnedConfiguration)\SDK</EngineSdk>
<EngineSdk Condition="'$(EngineSdk)'==''">$(LocalAppData)\PainEngine\Engines\$(EnginePinnedVersion)\$(EnginePinnedConfiguration)\SDK</EngineSdk>
<EngineSdkToolset Condition="Exists('$(EngineSdk)\Sdk.json')">$([System.Text.RegularExpressions.Regex]::Replace($([System.IO.File]::ReadAllText('$(EngineSdk)\Sdk.json')), '(?s).*"toolset"\s*:\s*"([^"]+)".*', '$1'))</EngineSdkToolset>
<ProjectBuildOutput Condition="'$(ProjectBuildOutput)'==''">$(MSBuildProjectDirectory)\Binaries\IDE\$(Configuration)</ProjectBuildOutput>
</PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><PlatformToolset>$(EngineSdkToolset)</PlatformToolset><CharacterSet>Unicode</CharacterSet></PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
<Import Project="$(EngineSdk)\Build\ProjectSdk.props" />
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
<Import Project="$(EngineSdk)\Build\ProjectSdk.targets" />
</Project>
'@
    WriteNew 'Game.sln' @'
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
Project("{BC8A1FFA-BEE3-4634-8014-F334798102B3}") = "Game", "Game.vcxproj", "{EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}"
EndProject
Global
 GlobalSection(SolutionConfigurationPlatforms) = preSolution
  Debug|x64 = Debug|x64
  Release|x64 = Release|x64
 EndGlobalSection
 GlobalSection(ProjectConfigurationPlatforms) = postSolution
  {EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}.Debug|x64.ActiveCfg = Debug|x64
  {EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}.Debug|x64.Build.0 = Debug|x64
  {EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}.Release|x64.ActiveCfg = Release|x64
  {EF3BF4DF-1C0E-441B-9DD2-EB85268D11BD}.Release|x64.Build.0 = Release|x64
 EndGlobalSection
EndGlobal
'@
    WriteNew 'Build.ps1' @'
param([string]$Runtime='')
$ErrorActionPreference='Stop'
$pin=Get-Content -LiteralPath "$PSScriptRoot/Engine.lock.json" -Raw | ConvertFrom-Json
if(!$Runtime){
 $store=if($env:PAINENGINE_ENGINE_STORE){$env:PAINENGINE_ENGINE_STORE}else{Join-Path $env:LOCALAPPDATA 'PainEngine/Engines'}
 $Runtime=Join-Path $store ($pin.engineVersion+'/'+$pin.engineConfiguration)
}
& "$Runtime/SDK/ProjectTools.ps1" -Action Build -Project "$PSScriptRoot/Project.json" -Runtime $Runtime
'@
    WriteJson $codeFile @{format=1;state='prepared';originalModule=$manifest.module;sdk=$pin.engineVersion}
}
if (!(Test-Path -LiteralPath $codeFile)) { throw 'Enable C++ first' }
if ($Action -eq 'AddClass') { AddClass $Name $Kind $Base $BaseHeader; return }

$fingerprintParts = @($sdkInfo.version,$sdkInfo.configuration,(Get-FileHash -LiteralPath "$sdk/Lib/Engine.lib").Hash)
foreach($source in (Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -File -Recurse | Sort-Object FullName)) {
    $fingerprintParts += $source.FullName.Substring($projectRoot.Length),(Get-FileHash -LiteralPath $source.FullName).Hash
}
$fingerprintParts += (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Game.vcxproj')).Hash
$fingerprint = [Convert]::ToBase64String([Security.Cryptography.SHA256]::Create().ComputeHash($encoding.GetBytes(($fingerprintParts -join "`n"))))
$codeState = Get-Content -LiteralPath $codeFile -Raw | ConvertFrom-Json
$currentManifest = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
if ($codeState.state -eq 'ready' -and $codeState.module -eq $currentManifest.module -and $codeState.fingerprint -eq $fingerprint -and (Test-Path -LiteralPath (Join-Path $projectRoot $currentManifest.module))) { Write-Output 'Project is already up to date'; return }

$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ tools and Windows SDK to build this project' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild is unavailable' }
$buildId = [guid]::NewGuid().ToString('N')
$output = Join-Path $projectRoot ('Binaries/'+$pin.engineConfiguration+'/'+$buildId)
[IO.Directory]::CreateDirectory($output) | Out-Null
# Explicit arguments, no command-string evaluation; inherited duplicate PATH keys
# are normalized before MSBuild starts its compiler tasks.
foreach($group in ([Environment]::GetEnvironmentVariables().Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1)) {
 $value=[Environment]::GetEnvironmentVariable($group.Group[0]); foreach($key in $group.Group){[Environment]::SetEnvironmentVariable($key,$null,'Process')}; [Environment]::SetEnvironmentVariable($group.Name,$value,'Process')
}
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName=$msbuild; $start.UseShellExecute=$false
$start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
$start.EnvironmentVariables.Clear()
foreach($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()]=$entry.Value }
$start.Arguments='"'+(Join-Path $projectRoot 'Game.vcxproj')+'" /nologo /m:1 /nr:false /verbosity:minimal /p:Platform=x64 /p:Configuration='+$pin.engineConfiguration+' /p:PlatformToolset='+$sdkInfo.toolset+' "/p:EngineSdk='+$sdk+'" "/p:ProjectBuildOutput='+$output+'"'
$buildProcess=[Diagnostics.Process]::Start($start)
$buildOutput=$buildProcess.StandardOutput.ReadToEndAsync(); $buildErrors=$buildProcess.StandardError.ReadToEndAsync()
$buildProcess.WaitForExit()
Write-Output $buildOutput.GetAwaiter().GetResult(); Write-Output $buildErrors.GetAwaiter().GetResult()
if ($buildProcess.ExitCode) { throw 'Project build failed; previous module preserved' }
Copy-Item -Path "$Runtime/Templates/SceneProject/*.dll" -Destination $output -Exclude Game.dll -Force
$manifest = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$previous = $manifest.module
$relative = 'Binaries/'+$pin.engineConfiguration+'/'+$buildId+'/Game.dll'
$candidate = Join-Path $projectRoot ('Project.candidate.'+$buildId+'.json')
$manifest.module = $relative
WriteJson $candidate $manifest
try {
    $check = Start-Process -FilePath (Join-Path $Runtime 'EngineRuntime.exe') -ArgumentList @('--validate-project',('"'+$candidate+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output 'validate.log') -RedirectStandardError (Join-Path $output 'validate-error.log')
    if (!$check.WaitForExit(60000)) { $check.Kill(); throw 'Candidate validation timed out' }
    if ($check.ExitCode) { throw "Candidate rejected; previous module preserved. See $output/validate-error.log" }
    $smoke = Start-Process -FilePath (Join-Path $Runtime 'EngineRuntime.exe') -ArgumentList @('--project',('"'+$candidate+'"'),'--mode','game','--frames','3') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output 'startup.log') -RedirectStandardError (Join-Path $output 'startup-error.log')
    if (!$smoke.WaitForExit(60000)) { $smoke.Kill(); throw 'Candidate startup timed out; previous module preserved' }
    if ($smoke.ExitCode) { throw "Candidate startup failed; previous module preserved. See $output/startup-error.log" }
    Copy-Item -LiteralPath $projectFile -Destination ($projectFile+'.before-build') -Force
    WriteJson $codeFile @{format=1;state='ready';sdk=$pin.engineVersion;configuration=$pin.engineConfiguration;buildId=$buildId;module=$relative;previousModule=$previous;fingerprint=$fingerprint}
    Copy-Item -LiteralPath (Join-Path $output 'ProjectTypes.tsv') -Destination (Join-Path $projectRoot 'ProjectTypes.tsv') -Force
    WriteJson $projectFile $manifest
    Write-Output 'Build validated and activated. Use Hot Reload in the editor, or reopen the project.'
} finally { if(Test-Path -LiteralPath $candidate){Remove-Item -LiteralPath $candidate} }
} finally { $buildLock.Dispose() }

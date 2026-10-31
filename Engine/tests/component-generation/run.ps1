param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$buildScript = Join-Path $repo 'Engine/BuildSupport/Invoke-MSBuild.ps1'
& $buildScript -Project (Join-Path $PSScriptRoot 'ComponentGenerationTests.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset
$bin = Join-Path $repo "Engine/build/x64/$Configuration"
& (Join-Path $bin 'ComponentGenerationTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Integration test failed' }

# A fresh sandbox per run, retained for inspecting generated C++ and diagnostics.
$scratch = Join-Path $repo ('Engine/build/component-generator-tests/' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $scratch 'Source with spaces'
$output = Join-Path $scratch 'Generated'
[IO.Directory]::CreateDirectory($source) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Put([string]$Path, [string]$Text) { [IO.File]::WriteAllText($Path, $Text, $utf8) }
$manifest = Join-Path $scratch 'roots.txt'
Put $manifest $source
$header = Join-Path $source 'Example.h'
$generated = Join-Path $output 'GeneratedComponents.cpp'
$script:passed = 0
function Generate([bool]$Success, [string]$Expected = '') {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $bin 'ComponentGenerator.exe'
    $start.Arguments = '"' + $manifest + '" "' + $output + '" ScannerTests'
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $message = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    if (($process.ExitCode -eq 0) -ne $Success) { throw "Unexpected generator result: $message" }
    if ($Expected -and !$message.Contains($Expected)) { throw "Missing diagnostic '$Expected': $message" }
    if (!$Success -and !(Get-Content -LiteralPath $generated -Raw).Equals($script:lastGood)) {
        throw 'Failed generation changed previous C++ output'
    }
    $script:passed++
}
Put $header @'
#ifndef EXAMPLE_H
#define EXAMPLE_H
// ENGINE_COMPONENT() class Fake : public GameComponent {};
const char* text = u8R"delim(ENGINE_COMPONENT() class Fake {})delim";
namespace Outer::Inner {
ENGINE_COMPONENT("Player") class Player final : public GameComponent {};
ENGINE_COMPONENT() class Other : public GameComponent {};
}
#endif
'@
Generate $true '2 component(s)'
$script:lastGood = Get-Content -LiteralPath $generated -Raw
if (!$lastGood.Contains('Create<::Outer::Inner::Player>') -or !$lastGood.Contains('Outer::Inner::Other')) { throw 'Wrong qualified type name' }
$timestamp = (Get-Item -LiteralPath $generated).LastWriteTimeUtc
Generate $true
if ((Get-Item -LiteralPath $generated).LastWriteTimeUtc -ne $timestamp) { throw 'No-op generation changed timestamp' }
$cases = @(
    @('ENGINE_COMPONENT() class Forward;', "Expected ':'"),
    @('ENGINE_COMPONENT(123) class Bad : public GameComponent {};', "Expected ')'"),
    @('template<class T> ENGINE_COMPONENT() class Bad : public GameComponent {};', 'non-template'),
    @('namespace { ENGINE_COMPONENT() class Bad : public GameComponent {}; }', 'named namespace'),
    @('class Outer { ENGINE_COMPONENT() class Bad : public GameComponent {}; };', 'named namespace'),
    @("#if 0`nENGINE_COMPONENT() class Bad : public GameComponent {};`n#endif", 'Conditional component'),
    @('ENGINE_COMPONENT() class A : public GameComponent {}; ENGINE_COMPONENT() class A : public GameComponent {};', 'Duplicate component'),
    @('ENGINE_PROPERTY() int field;', 'not supported'),
    @('ENGINE_COMPONENT() struct Bad : public GameComponent {};', "Expected 'class'"),
    @('/* unterminated', 'Unterminated comment')
)
foreach ($case in $cases) { Put $header $case[0]; Generate $false $case[1] }
Put $header ''
Put (Join-Path $source 'Wrong.cpp') 'ENGINE_COMPONENT() class Bad : public GameComponent {};'
Generate $false 'must be in a header'
Put (Join-Path $source 'Wrong.cpp') ''
Generate $true '0 component(s)'
if ((Get-Content -LiteralPath $generated -Raw).Contains('Player')) { throw 'Removed class remained in output' }
Put (Join-Path $source 'New.hpp') 'ENGINE_COMPONENT() class NewClass : public GameComponent {};'
Generate $true '1 component(s)'
if (!(Get-Content -LiteralPath $generated -Raw).Contains('Create<::NewClass>')) { throw 'New header not discovered' }
Put (Join-Path $source 'New.hpp') ''
Put $manifest "$source`r`n$source"
Generate $true '0 component(s)'
$script:lastGood = Get-Content -LiteralPath $generated -Raw
Put $manifest (Join-Path $scratch 'Missing')
Generate $false
Put $manifest $source
Put $header 'ENGINE_OBJECT("Rules", "Game.Rules") class Rules : public RulesBase {}; ENGINE_GAME("Game", "Game.Main") class MyGame : public Game {}; ENGINE_COMPONENT("Child", "Game.Child") class Child : public Parent {};'
Generate $true
$script:lastGood = Get-Content -LiteralPath $generated -Raw
$typeManifest = Get-Content -LiteralPath (Join-Path $output 'ProjectTypes.tsv') -Raw
if (!$typeManifest.Contains("Object`tGame.Rules") -or !$typeManifest.Contains("Game`tGame.Main") -or !$lastGood.Contains('CreateObject<::Rules>')) { throw 'Missing registered type family or stable ID' }
Put $header 'ENGINE_OBJECT("A", "Same.ID") class A {}; ENGINE_GAME("B", "Same.ID") class B : public Game {};'
Generate $false 'Duplicate'
Put $header 'ENGINE_OBJECT("A", "bad/id") class A {};'
Generate $false
Write-Output "Scanner tests: $passed passed"

# Compile-time contract: run the same build target as a consumer; no editor/link
# to Engine is needed for this static library. Each case overwrites only our fixture.
$compileRoot = Join-Path $scratch 'Source'
[IO.Directory]::CreateDirectory($compileRoot) | Out-Null
$compileHeader = Join-Path $compileRoot 'Contract.h'
$project = Join-Path $scratch 'GeneratorContractTests.vcxproj'
$enginePath = [Security.SecurityElement]::Escape((Join-Path $repo 'Engine'))
Put $project @"
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="$Configuration|x64"><Configuration>$Configuration</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <Import Project="$enginePath\BuildSupport\Cpp.props" />
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <Import Project="$enginePath\BuildSupport\ComponentGeneration.targets" />
</Project>
"@
$contracts = @(
    @('class Bad : public GameComponent { public: virtual void Required() = 0; };', 'cannot create an abstract class'),
    @('class Bad : private GameComponent {};', 'public, unambiguous'),
    @('class Bad : public GameComponent { public: explicit Bad(int) {} };', 'public default constructor'),
    @('class Bad : public std::string {};', 'public, unambiguous')
)
foreach ($contract in $contracts) {
    Put $compileHeader ("#pragma once`n#include <Engine/Components/GameComponents.h>`nENGINE_COMPONENT()`n" + $contract[0])
    $failed = $false
    $log = [Collections.Generic.List[string]]::new()
    try { & $buildScript -Project $project -Configuration $Configuration -PlatformToolset $PlatformToolset | ForEach-Object { $log.Add([string]$_) } }
    catch { $failed = $true }
    Put (Join-Path $scratch "negative-$($contracts.IndexOf($contract)).log") ($log -join "`n")
    if (!$failed -or !(($log -join "`n").Contains($contract[1]))) { throw "Expected compiler rejection: $($contract[1])`n$($log -join "`n")" }
}
Put $compileHeader "#pragma once`n"
& $buildScript -Project $project -Configuration $Configuration -PlatformToolset $PlatformToolset
Write-Output "Compiler contract: 4 expected rejections + empty catalog passed. Artifacts: $scratch"

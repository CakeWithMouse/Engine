param([ValidateSet('Debug','Release')][string]$Configuration='Release', [string]$WorkRoot='O:/Engine/SubsystemSdkChecks')
$ErrorActionPreference='Stop'
$engine=[IO.Path]::GetFullPath("$PSScriptRoot/../..")
$work=Join-Path $WorkRoot ([guid]::NewGuid().ToString('N'))
$runtime=Join-Path $work 'InstalledRuntime'
New-Item -ItemType Directory -Force $runtime | Out-Null
Set-Content "$runtime/EngineVersion.json" '{"version":"subsystem-test"}'
& "$engine/BuildSupport/New-ProjectSdk.ps1" -Runtime $runtime -Configuration $Configuration
foreach($name in @('Module','Host')) {
    $dir=Join-Path $work $name
    New-Item -ItemType Directory -Force "$dir/Source" | Out-Null
    $type=if($name -eq 'Module'){'DynamicLibrary'}else{'Application'}
    $project=@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion><EngineSdk>SDKPATH</EngineSdk><ProjectBuildOutput>OUTPATH</ProjectBuildOutput></PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>TYPE</ConfigurationType><PlatformToolset>v145</PlatformToolset></PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
<Import Project="$(EngineSdk)\Build\ProjectSdk.props" />
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
<Import Project="$(EngineSdk)\Build\ProjectSdk.targets" />
</Project>
'@
    $project=$project.Replace('SDKPATH',"$runtime/SDK").Replace('OUTPATH',"$dir/Binaries").Replace('TYPE',$type)
    Set-Content "$dir/Game.vcxproj" $project
    if($name -eq 'Module') { Copy-Item "$PSScriptRoot/SdkModule.h","$PSScriptRoot/SdkModule.cpp" "$dir/Source" }
    else {Copy-Item "$PSScriptRoot/SdkHost.cpp" "$dir/Source"}
    & "$engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$dir/Game.vcxproj" -Configuration $Configuration
    if($name -eq 'Module') {
        $extra="$dir/Source/AddedSubsystem.h"
        Set-Content $extra "#pragma once`n#include <Engine/MainGame/GameSubsystem.h>`nENGINE_SUBSYSTEM() class AddedSubsystem : public GameSubsystem {};"
        & "$engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$dir/Game.vcxproj" -Configuration $Configuration
        $generated="$dir/Intermediate/$Configuration/Generated/GeneratedComponents.cpp"
        if(!(Get-Content $generated -Raw).Contains('CreateGameSubsystem<::AddedSubsystem>')) {throw 'Normal SDK build did not discover added header'}
        # Only remove the known, generated fixture in this fresh test directory.
        Remove-Item -LiteralPath $extra
        & "$engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$dir/Game.vcxproj" -Configuration $Configuration
        if((Get-Content $generated -Raw).Contains('AddedSubsystem')) {throw 'Normal SDK build retained deleted class'}
        Set-Content $extra "#pragma once`n#include <Engine/MainGame/GameSubsystem.h>`nENGINE_SUBSYSTEM() class BadSubsystem : public std::string {};"
        $failed=$false
        try { & "$engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$dir/Game.vcxproj" -Configuration $Configuration } catch {$failed=$true}
        if(!$failed) {throw 'Invalid subsystem declaration compiled'}
        Remove-Item -LiteralPath $extra
        & "$engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$dir/Game.vcxproj" -Configuration $Configuration
    }
}
$bin="$engine/build/x64/$Configuration"
Copy-Item "$bin/*.dll" "$work/Module/Binaries" -ErrorAction SilentlyContinue
& "$work/Host/Binaries/Game.exe" "$work/Module/Binaries/Game.dll" "$bin/EngineContent"
if($LASTEXITCODE) {throw 'External SDK test failed'}
Write-Output "External project and installed SDK: $work"

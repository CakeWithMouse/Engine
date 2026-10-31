param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SceneModuleTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/Fixtures.proj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/build/x64/$Configuration/SceneModuleTests.exe" "$repo/Game/build/x64/$Configuration/Runtime/Projects/RuntimeExample/Game.dll" "$repo/Engine/build/x64/$Configuration/RuntimeFixtures"
if ($LASTEXITCODE -ne 0) { throw 'Scene module tests failed' }

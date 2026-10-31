param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/EditSessionTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/../scene-module/Fixtures.proj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/build/x64/$Configuration/EditSessionTests.exe" "$repo/Game/build/x64/$Configuration/Runtime/Projects/RuntimeExample/Game.dll" "$repo/Engine/Content" "$repo/Engine/build/x64/$Configuration/EditConsumer.dll"
if ($LASTEXITCODE -ne 0) { throw 'Edit session tests failed' }

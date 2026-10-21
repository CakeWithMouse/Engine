param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SkeletalTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/build/x64/$Configuration/SkeletalTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Skeletal tests failed' }

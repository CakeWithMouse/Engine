param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto', [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
if (!$env:VCPKG_ROOT -and (Test-Path 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg')) {
    $env:VCPKG_ROOT = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
}
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SkeletalTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp
& "$repo/Engine/build/x64/$Configuration/SkeletalTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Skeletal tests failed' }

param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'v145', [switch]$NoBuild, [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$useAssimp = $EnableAssimp.IsPresent -or (Test-Path (Join-Path $PSScriptRoot '../vcpkg_installed/x64-windows/include/assimp/scene.h'))
if (!$NoBuild) {
    & (Join-Path $PSScriptRoot '../BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot 'EngineTests.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$useAssimp
}
& (Join-Path $PSScriptRoot "../build/x64/$Configuration/EngineTests.exe")
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }

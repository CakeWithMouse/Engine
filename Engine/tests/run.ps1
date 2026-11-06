param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'v145', [switch]$NoBuild, [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$useAssimp = $EnableAssimp.IsPresent -or (Test-Path (Join-Path $PSScriptRoot '../vcpkg_installed/x64-windows/include/assimp/scene.h'))
if (!$NoBuild) {
    & (Join-Path $PSScriptRoot '../BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot 'EngineTests.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$useAssimp
    # VFS lifecycle tests load these DLLs. Rebuild against the current Module API;
    # an old fixture otherwise looks like an engine lifecycle regression.
    & (Join-Path $PSScriptRoot '../BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot 'runtime/Fixtures.proj') -Configuration $Configuration -PlatformToolset $PlatformToolset
}
& (Join-Path $PSScriptRoot "../build/x64/$Configuration/EngineTests.exe")
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }

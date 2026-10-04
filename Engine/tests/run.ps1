param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'v145', [switch]$NoBuild)
$ErrorActionPreference = 'Stop'
if (!$NoBuild) {
    & (Join-Path $PSScriptRoot '../BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot 'EngineTests.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset
}
& (Join-Path $PSScriptRoot "../build/x64/$Configuration/EngineTests.exe")
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }

param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto', [switch]$NoBuild)
$ErrorActionPreference = 'Stop'
if (!$NoBuild) {
    & "$PSScriptRoot/../BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/ViewportTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
}
$capture = [IO.Path]::GetFullPath("$PSScriptRoot/../build/viewport-$Configuration.bmp")
& "$PSScriptRoot/../build/x64/$Configuration/ViewportTests.exe" $capture
if ($LASTEXITCODE -ne 0) { throw "Viewport tests failed: $LASTEXITCODE" }
Write-Host "Rendered frame: $capture"

param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$Filter='', [switch]$NoBuild,
    [switch]$RefreshGuide)
$ErrorActionPreference='Stop'
# Compile the checked-in examples on a clean checkout. The original authoring
# guide is not shipped in this repository; regeneration is an explicit step for
# maintainers who have it, and still fails if that source is absent.
if ($RefreshGuide) { & "$PSScriptRoot/Extract-Guide.ps1" }
if (!$NoBuild) { & "$PSScriptRoot/../../BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/AiTests.vcxproj" -Configuration $Configuration }
& "$PSScriptRoot/../../build/x64/$Configuration/AiTests.exe" $Filter
if ($LASTEXITCODE) { throw 'AI tests failed' }

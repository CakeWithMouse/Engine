param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$Filter='', [switch]$NoBuild)
$ErrorActionPreference='Stop'
if (!$NoBuild) { & "$PSScriptRoot/Extract-Guide.ps1"; & "$PSScriptRoot/../../BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/AiTests.vcxproj" -Configuration $Configuration }
& "$PSScriptRoot/../../build/x64/$Configuration/AiTests.exe" $Filter
if ($LASTEXITCODE) { throw 'AI tests failed' }

param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
& "$PSScriptRoot/../../BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SubsystemTests.vcxproj" -Configuration $Configuration -EnableAssimp
& "$PSScriptRoot/../../build/x64/$Configuration/SubsystemTests.exe"
if ($LASTEXITCODE) { throw 'Subsystem tests failed' }

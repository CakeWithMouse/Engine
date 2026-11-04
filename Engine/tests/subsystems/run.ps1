param([ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$EnableAssimp)
$ErrorActionPreference='Stop'
$assimpArgs=@{};if($PSBoundParameters.ContainsKey('EnableAssimp')){$assimpArgs.EnableAssimp=$EnableAssimp}
& "$PSScriptRoot/../../BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SubsystemTests.vcxproj" -Configuration $Configuration @assimpArgs
& "$PSScriptRoot/../../build/x64/$Configuration/SubsystemTests.exe"
if ($LASTEXITCODE) { throw 'Subsystem tests failed' }

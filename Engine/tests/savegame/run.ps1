param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/SaveGameTests.vcxproj" -Configuration $Configuration -EnableAssimp
$fixture=Join-Path $repo ('Engine/build/savegame-' + [guid]::NewGuid().ToString('N'))
& "$repo/Engine/build/x64/$Configuration/SaveGameTests.exe" $fixture "$repo/Game/build/x64/$Configuration/Runtime/EngineContent"
if($LASTEXITCODE -ne 0){throw 'SaveGame tests failed'}
& "$repo/Engine/build/x64/$Configuration/SaveGameTests.exe" $fixture --read
if($LASTEXITCODE -ne 0){throw 'SaveGame restart test failed'}

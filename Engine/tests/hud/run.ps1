param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if(!$SkipBuild){ & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/HudTests.vcxproj" -Configuration $Configuration -EnableAssimp }
$fixture=Join-Path $repo ('Engine/build/hud-tests/'+[guid]::NewGuid().ToString('N'))
& "$repo/Engine/build/x64/$Configuration/HudTests.exe" $fixture "$repo/Engine/Content"
if($LASTEXITCODE){throw 'HUD tests failed'}

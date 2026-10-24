param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto', [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/AssetTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp
$work = Join-Path $repo ('Engine/build/asset-tests/Проект ' + [guid]::NewGuid().ToString('N'))
& "$repo/Engine/build/x64/$Configuration/AssetTests.exe" "$repo/Game/build/x64/$Configuration/Runtime/Templates/SceneProject/Game.dll" "$repo/Engine/Content" $work "$repo/Game/RuntimeExample/Content/Models/CesiumMan.glb"
if ($LASTEXITCODE -ne 0) { throw 'Asset workflow tests failed' }

param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Engine','EngineLauncher','BasicExamples','Pong','SunGame','Katamari')][string]$Target = 'All',
    [string]$PlatformToolset = 'v145', [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$project = switch ($Target) {
    'All' { 'Engine.sln' }
    'Engine' { 'Engine.vcxproj' }
    'EngineLauncher' { '../Game/Launcher/EngineLauncher.vcxproj' }
    default { "../Game/$Target/$Target.vcxproj" }
}
& (Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot $project) -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp
if ($Target -eq 'All' -and $EnableAssimp) {
    & (Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot '../Game/Katamari/Katamari.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp
}

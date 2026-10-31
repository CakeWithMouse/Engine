param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Engine','EngineRuntime','EngineLauncher','BasicExamples','Pong','SunGame','Battle','Katamari')][string]$Target = 'All',
    [string]$PlatformToolset = 'auto', [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'

$invoke = Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1'

# The editor is a Qt application and Qt is not in the repository. Without it the editor cannot be
# built - but nothing else needs it, so the games, the runtime and the launcher still have to come
# out. Same rule as Assimp: a missing optional dependency skips its target, it does not stop the
# build. Engine/docs/RUNTIME-QUICKSTART.md says how to point the build at a Qt SDK.
function Test-QtAvailable {
    foreach ($root in @($env:QTDIR, (Join-Path $PSScriptRoot '../Editor/ThirdParty/Qt6.7'))) {
        if ($root -and (Test-Path -LiteralPath (Join-Path $root 'bin/moc.exe'))) { return $true }
    }
    return $false
}

function Build-Project([string]$relativePath, [bool]$alone) {
    & $invoke -Project (Join-Path $PSScriptRoot $relativePath) -Configuration $Configuration `
        -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp -SkipProjectReferences:$alone
}

$project = switch ($Target) {
    'All' { 'Engine.sln' }
    'Engine' { 'Engine.vcxproj' }
    'EngineRuntime' { 'Runtime/EngineRuntime.vcxproj' }
    'EngineLauncher' { '../Game/Launcher/EngineLauncher.vcxproj' }
    default { "../Game/$Target/$Target.vcxproj" }
}

# EngineRuntime references the editor, and the launcher references the runtime, so building either
# of them the usual way drags the editor in. Without Qt they are built alone, after what they need.
$reachesEditor = @('All', 'EngineRuntime', 'EngineLauncher') -contains $Target
if ($reachesEditor -and -not (Test-QtAvailable)) {
    Write-Output 'Qt 6 was not found: building without the editor (set QTDIR or unpack the SDK into Editor/ThirdParty/Qt6.7).'

    # In dependency order; the runtime loads the example game as a DLL at run time.
    Build-Project 'Engine.vcxproj' $false
    Build-Project '../Game/RuntimeExample/RuntimeExample.vcxproj' $false
    Build-Project 'Runtime/EngineRuntime.vcxproj' $true
    if ($Target -ne 'EngineRuntime') {
        Build-Project '../Game/Launcher/EngineLauncher.vcxproj' $true
    }
    if ($Target -eq 'All') {
        Build-Project 'tests/EngineTests.vcxproj' $false
        foreach ($game in @('BasicExamples', 'Pong', 'SunGame', 'Battle')) {
            Build-Project "../Game/$game/$game.vcxproj" $false
        }
    }
}
else {
    Build-Project $project $false
}

if ($Target -eq 'All' -and $EnableAssimp) {
    Build-Project '../Game/Katamari/Katamari.vcxproj' $false
}

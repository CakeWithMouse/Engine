param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Engine','EngineRuntime','EngineLauncher','BasicExamples','Pong','SunGame','Battle','Katamari')][string]$Target = 'All',
    [string]$PlatformToolset = 'auto',
    [Nullable[bool]]$EnableAssimp = $null,
    [switch]$DisableAssimp
)
$ErrorActionPreference = 'Stop'

$invoke = Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1'

# Auto-detect whether Assimp should be enabled if not explicitly requested or disabled
$useAssimp = $false
if ($DisableAssimp) {
    $useAssimp = $false
} elseif ($null -ne $EnableAssimp) {
    $useAssimp = [bool]$EnableAssimp
} else {
    # Check vcpkg availability
    $vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vcpkgRoot = $env:VCPKG_ROOT
    if (!$vcpkgRoot -and (Test-Path -LiteralPath $vswhere)) {
        $installations = & $vswhere -products '*' -property installationPath
        foreach ($inst in $installations) {
            $candidate = Join-Path $inst 'VC\vcpkg'
            if (Test-Path -LiteralPath (Join-Path $candidate 'scripts\buildsystems\msbuild\vcpkg.props')) {
                $vcpkgRoot = $candidate
                break
            }
        }
    }
    if (!$vcpkgRoot) {
        $candidates = @('C:\vcpkg', 'D:\vcpkg', (Join-Path $env:USERPROFILE 'vcpkg'))
        foreach ($c in $candidates) {
            if ($c -and (Test-Path -LiteralPath (Join-Path $c 'scripts\buildsystems\msbuild\vcpkg.props'))) {
                $vcpkgRoot = $c
                break
            }
        }
    }
    if ($vcpkgRoot -and (Test-Path -LiteralPath (Join-Path $vcpkgRoot 'scripts\buildsystems\msbuild\vcpkg.props'))) {
        $env:VCPKG_ROOT = $vcpkgRoot
        # Only an Assimp that is already installed turns the optional target on. vcpkg.json is
        # committed, so treating it as a signal enabled Assimp on every machine with vcpkg and
        # made a failed port install a failed build of the whole engine - and installing it from
        # the manifest is not always possible (under a non-ASCII path the port's own link step
        # gets a mangled library name). Ask for it with -EnableAssimp to install it on purpose.
        $installedHeader = Join-Path $PSScriptRoot 'vcpkg_installed\x64-windows\include\assimp\scene.h'
        $vcpkgInstalledHeader = Join-Path $vcpkgRoot 'installed\x64-windows\include\assimp\scene.h'
        if ((Test-Path -LiteralPath $installedHeader) -or (Test-Path -LiteralPath $vcpkgInstalledHeader)) {
            $useAssimp = $true
        }
    }
}

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
        -PlatformToolset $PlatformToolset -EnableAssimp:$useAssimp -SkipProjectReferences:$alone
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

    # In dependency order; the runtime loads the example game and the scene project as DLLs at
    # run time, so both have to exist before the runtime is built on its own.
    Build-Project 'Engine.vcxproj' $false
    Build-Project '../Game/RuntimeExample/RuntimeExample.vcxproj' $false
    Build-Project '../Game/SceneProject/SceneProject.vcxproj' $false
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

if ($Target -eq 'All' -and $useAssimp) {
    Build-Project '../Game/Katamari/Katamari.vcxproj' $false
}

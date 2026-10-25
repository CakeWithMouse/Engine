param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Engine','EngineRuntime','EngineLauncher','BasicExamples','Pong','SunGame','Battle','Katamari')][string]$Target = 'All',
    [string]$PlatformToolset = 'auto',
    [Nullable[bool]]$EnableAssimp = $null,
    [switch]$DisableAssimp
)
$ErrorActionPreference = 'Stop'

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
        $installedHeader = Join-Path $PSScriptRoot 'vcpkg_installed\x64-windows\include\assimp\scene.h'
        $vcpkgInstalledHeader = Join-Path $vcpkgRoot 'installed\x64-windows\include\assimp\scene.h'
        if ((Test-Path -LiteralPath $installedHeader) -or (Test-Path -LiteralPath $vcpkgInstalledHeader) -or (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'vcpkg.json'))) {
            $useAssimp = $true
        }
    }
}

$project = switch ($Target) {
    'All' { 'Engine.sln' }
    'Engine' { 'Engine.vcxproj' }
    'EngineRuntime' { 'Runtime/EngineRuntime.vcxproj' }
    'EngineLauncher' { '../Game/Launcher/EngineLauncher.vcxproj' }
    default { "../Game/$Target/$Target.vcxproj" }
}
& (Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot $project) -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$useAssimp
if ($Target -eq 'All' -and $useAssimp) {
    & (Join-Path $PSScriptRoot 'BuildSupport/Invoke-MSBuild.ps1') -Project (Join-Path $PSScriptRoot '../Game/Katamari/Katamari.vcxproj') -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp
}

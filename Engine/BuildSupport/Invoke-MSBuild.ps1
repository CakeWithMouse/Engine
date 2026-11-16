param([Parameter(Mandatory=$true)][string]$Project,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto',
    [switch]$EnableAssimp,
    # Builds this project alone. Used when a referenced project cannot be built here, which is
    # the editor without Qt: everything else still has to come out.
    [switch]$SkipProjectReferences,
    [switch]$LocalSdk,
    [switch]$EditorUiTest,
    [switch]$EngineCommandlet)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio with C++ tools and Windows SDK is required (vswhere.exe not found).' }
. "$PSScriptRoot/Toolchain.ps1"
$family = if ($PlatformToolset -eq 'auto') { '' } else { $PlatformToolset }
$toolchain = Resolve-EngineToolchain -Toolset $family
$PlatformToolset = $toolchain.toolset
$msbuild = $toolchain.msbuild
Write-Output "Using $PlatformToolset / MSVC $($toolchain.version) / Windows SDK $($toolchain.windowsSdk.version)"
# Windows PowerShell 5.1 cannot even read ProcessStartInfo.EnvironmentVariables
# when the parent supplied both Path and PATH. Normalize duplicate names first.
$environmentSnapshot = [Environment]::GetEnvironmentVariables()
$duplicateNames = $environmentSnapshot.Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1
foreach ($group in $duplicateNames) {
    $value = [Environment]::GetEnvironmentVariable($group.Group[0])
    foreach ($name in $group.Group) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    [Environment]::SetEnvironmentVariable($group.Name, $value, 'Process')
}

# Auto-detect VCPKG_ROOT if not already set in environment
if (!$env:VCPKG_ROOT) {
    $installations = & $vswhere -products '*' -property installationPath
    foreach ($inst in $installations) {
        $candidate = Join-Path $inst 'VC\vcpkg'
        if (Test-Path -LiteralPath (Join-Path $candidate 'scripts\buildsystems\msbuild\vcpkg.props')) {
            $env:VCPKG_ROOT = $candidate
            break
        }
    }
    if (!$env:VCPKG_ROOT) {
        $candidates = @('C:\vcpkg', 'D:\vcpkg', (Join-Path $env:USERPROFILE 'vcpkg'))
        foreach ($c in $candidates) {
            if ($c -and (Test-Path -LiteralPath (Join-Path $c 'scripts\buildsystems\msbuild\vcpkg.props'))) {
                $env:VCPKG_ROOT = $c
                break
            }
        }
    }
}
if ($env:VCPKG_ROOT) {
    [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $env:VCPKG_ROOT, 'Process')
}

$useAssimp = if ($PSBoundParameters.ContainsKey('EnableAssimp')) {
    $EnableAssimp.IsPresent
} else {
    # Keep installed capabilities when building a single target (including tests
    # and commandlets). An omitted switch must not rebuild Engine.lib as a stub.
    Test-Path -LiteralPath (Join-Path $PSScriptRoot '../vcpkg_installed/x64-windows/include/assimp/scene.h')
}
$assimp = $useAssimp.ToString().ToLowerInvariant()
Write-Output "Assimp enabled: $assimp"
$arguments = @($Project, '/nologo', '/verbosity:minimal', '/m:1', '/nr:false', '/p:UseStructuredOutput=false',
    (ConvertTo-MSBuildProperty 'Configuration' $Configuration), (ConvertTo-MSBuildProperty 'EngineEnableAssimp' $assimp))
$arguments += @(Get-EngineToolchainArguments $toolchain)
if ($env:VCPKG_ROOT) { $arguments += ConvertTo-MSBuildProperty 'VcpkgRoot' ($env:VCPKG_ROOT.TrimEnd('\','/')+'\') }
if ($useAssimp) { $arguments += ConvertTo-MSBuildProperty 'IncludePath' ((Get-EngineToolchainProperties $toolchain).IncludePath + ';' + (Join-Path $PSScriptRoot '../vcpkg_installed/x64-windows/include')) }
if ($SkipProjectReferences) { $arguments += '/p:BuildProjectReferences=false' }
if ($EditorUiTest) { $arguments += '/p:EditorUiTest=true' }
if ($EngineCommandlet) { $arguments += '/p:EngineCommandlet=true' }
if ($LocalSdk) { $arguments += '/p:ProjectSdkOptions=-UseBuildToolset' }
$result = Invoke-ToolchainProcess $msbuild $arguments -Timeout 3600000
Write-Output $result.stdout
if ($result.stderr) { Write-Output $result.stderr }
if ($result.exitCode) { throw "MSBuild failed: $($result.exitCode)" }

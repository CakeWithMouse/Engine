param([Parameter(Mandatory=$true)][string]$Project,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto',
    [switch]$EnableAssimp,
    [switch]$EditorUiTest)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio with C++ tools and Windows SDK is required (vswhere.exe not found).' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild with C++ tools and Windows SDK is required.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
if ($PlatformToolset -eq 'auto') {
    $PlatformToolset = @('v145','v143') | Where-Object {
        @(Get-ChildItem -Path (Join-Path $installation "MSBuild/Microsoft/VC/*/Platforms/x64/PlatformToolsets/$_/Toolset.props") -ErrorAction SilentlyContinue).Count -gt 0
    } | Select-Object -First 1
    if (!$PlatformToolset) { throw 'No supported C++ toolset found (v145 or v143). Install C++ tools or pass -PlatformToolset explicitly.' }
    Write-Output "Using installed platform toolset: $PlatformToolset"
}
# Windows PowerShell 5.1 cannot even read ProcessStartInfo.EnvironmentVariables
# when the parent supplied both Path and PATH. Normalize duplicate names first.
$environmentSnapshot = [Environment]::GetEnvironmentVariables()
$duplicateNames = $environmentSnapshot.Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1
foreach ($group in $duplicateNames) {
    $value = [Environment]::GetEnvironmentVariable($group.Group[0])
    foreach ($name in $group.Group) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    [Environment]::SetEnvironmentVariable($group.Name, $value, 'Process')
}

# Auto-detect VCPKG_ROOT if not already set in environment (reuses $installation, no extra vswhere call)
if (!$env:VCPKG_ROOT) {
    if ($installation) {
        $candidate = Join-Path $installation 'VC\vcpkg'
        if (Test-Path -LiteralPath (Join-Path $candidate 'scripts\buildsystems\msbuild\vcpkg.props')) {
            $env:VCPKG_ROOT = $candidate
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

$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $msbuild
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $false
$start.RedirectStandardError = $false
# The desktop host may supply both PATH and Path. Normalize keys for MSBuild's CL task.
$start.EnvironmentVariables.Clear()
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
    $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()] = $entry.Value
}
$assimp = $EnableAssimp.IsPresent.ToString().ToLowerInvariant()
$vcpkgClean = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT.TrimEnd('\') } else { $null }
$vcpkgParam = if ($vcpkgClean) { ' "/p:VcpkgRoot=' + $vcpkgClean + '\\"' } else { '' }

$vcpkgInstalledInclude = Join-Path $PSScriptRoot '..\vcpkg_installed\x64-windows\include'
$vcpkgManifestParam = if (Test-Path -LiteralPath $vcpkgInstalledInclude) { ' /p:VcpkgManifestInstall=false' } else { '' }

$start.Arguments = '"' + $Project + '" /nologo /verbosity:minimal /m /nr:false /p:ForceSynchronousPDBWrites=true /p:UseStructuredOutput=false /p:PreferredToolArchitecture=x64 /p:Platform=x64 /p:Configuration=' + $Configuration + ' /p:PlatformToolset=' + $PlatformToolset + ' /p:EngineEnableAssimp=' + $assimp + $vcpkgParam + $vcpkgManifestParam
if ($EditorUiTest) { $start.Arguments += ' /p:EditorUiTest=true' }
$process = [Diagnostics.Process]::Start($start)

$process.WaitForExit()

if ($process.ExitCode -ne 0) { throw "MSBuild failed: $($process.ExitCode)" }

# Automatically clean release cache after successful build so Editor.dll hash mismatch never happens
Remove-Item -Path "$env:LOCALAPPDATA\PainEngine\Engines\2026.10.28.2" -Recurse -Force -ErrorAction SilentlyContinue

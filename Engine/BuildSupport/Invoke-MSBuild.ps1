param([Parameter(Mandatory=$true)][string]$Project,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto',
    [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio with C++ tools and Windows SDK is required (vswhere.exe not found).' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild with C++ tools and Windows SDK is required.' }
if ($PlatformToolset -eq 'auto') {
    $installation = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
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
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $msbuild
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
# The desktop host may supply both PATH and Path. Normalize keys for MSBuild's CL task.
$start.EnvironmentVariables.Clear()
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
    $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()] = $entry.Value
}
$assimp = $EnableAssimp.IsPresent.ToString().ToLowerInvariant()
$start.Arguments = '"' + $Project + '" /nologo /verbosity:minimal /m:1 /nr:false /p:UseStructuredOutput=false /p:Platform=x64 /p:Configuration=' + $Configuration + ' /p:PlatformToolset=' + $PlatformToolset + ' /p:EngineEnableAssimp=' + $assimp
$process = [Diagnostics.Process]::Start($start)
$output = $process.StandardOutput.ReadToEndAsync()
$errors = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
Write-Output $output.GetAwaiter().GetResult()
if ($errors.GetAwaiter().GetResult()) { Write-Output $errors.GetAwaiter().GetResult() }
if ($process.ExitCode -ne 0) { throw "MSBuild failed: $($process.ExitCode)" }

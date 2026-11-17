param(
    [ValidateSet('ninja','portable','clang','editor')][string]$Preset = 'ninja',
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'Debug',
    [string]$Target = '',
    [string]$QtRoot = '',
    [switch]$EnableAssimp,
    [switch]$EnableJolt,
    [string]$Toolchain = '',
    [int]$Jobs = 8,
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot
# Keep each configuration in its own tree; never overwrite the MSBuild outputs.
$binary = Join-Path $repository "build/cmake/$Preset-$Configuration"
if ($env:OS -eq 'Windows_NT') {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Get-Command cl -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath $vswhere)) {
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) {
            Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
            Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
        }
    }
    # Ninja's include dependency scanner must use the same language during
    # compiler detection and subsequent builds, including non-English VS installs.
    $env:VSLANG = '1033'
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    & cmd /c chcp 65001 | Out-Null
}
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake 3.24+ and Ninja must be on PATH.' }
$configure = @('--preset', $Preset, '-S', $repository, '-B', $binary, "-DCMAKE_BUILD_TYPE=$Configuration")
if ($QtRoot) { $configure += "-DCMAKE_PREFIX_PATH=$QtRoot" }
if ($EnableAssimp) { $configure += '-DPAIN_ENABLE_ASSIMP=ON' }
if ($EnableJolt) { $configure += '-DPAIN_ENABLE_JOLT=ON' }
if ($Toolchain) { $configure += "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" }
Push-Location $repository
try {
    & cmake @configure
    if ($LASTEXITCODE) { throw "CMake configure failed ($LASTEXITCODE)" }
    $build = @('--build', $binary, '--parallel', $Jobs, '--config', $Configuration)
    if ($Target) { $build += @('--target', $Target) }
    & cmake @build
    if ($LASTEXITCODE) { throw "CMake build failed ($LASTEXITCODE)" }
    if ($Test) {
        & ctest --test-dir $binary -C $Configuration --output-on-failure
        if ($LASTEXITCODE) { throw "CTest failed ($LASTEXITCODE)" }
    }
} finally { Pop-Location }

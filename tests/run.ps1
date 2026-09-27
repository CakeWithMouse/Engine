param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'v143'
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio with MSVC and Windows SDK is required.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild not found.' }
& $msbuild (Join-Path $PSScriptRoot 'EngineTests.vcxproj') /nologo /verbosity:minimal "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$PlatformToolset"
if ($LASTEXITCODE -ne 0) { throw "Test build failed: $LASTEXITCODE" }
Push-Location (Join-Path $PSScriptRoot '..\1_Lab')
try {
    & (Join-Path $PSScriptRoot "$Configuration\EngineTests.exe")
    if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }
} finally { Pop-Location }

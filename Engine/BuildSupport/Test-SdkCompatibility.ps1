# Build/link every object in the SDK, not just the starter project's used subset.
param([Parameter(Mandatory=$true)][string]$Sdk, [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string[]]$Toolsets = @('v142','v143','v145'))
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/Toolchain.ps1"
$info = Get-Content -LiteralPath "$Sdk/Sdk.json" -Raw | ConvertFrom-Json
$available = @(Get-EngineToolchains)
$results = @()
foreach ($family in $Toolsets) {
    if (!($available | Where-Object toolset -eq $family)) {
        if ($family -eq 'v142') { throw 'Release validation requires the VS 2019 (14.29) baseline tools.' }
        Write-Output "SDK compatibility: $family is not installed; skipped."
        continue
    }
    $toolchain = Resolve-EngineToolchain $info -Toolset $family
    $directory = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $family
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $source = Join-Path $directory 'Probe.cpp'
    [IO.File]::WriteAllText($source, @'
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/Character.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/SaveGame/SaveGame.h>
int main() { return 0; }
'@)
    $properties = Get-EngineToolchainProperties $toolchain
    $args = @('/nologo','/std:c++17','/EHsc','/permissive-','/utf-8', $(if($info.crt -eq 'MDd'){'/MDd'}else{'/MD'}),
        ('/I'+(Join-Path $Sdk 'Include')), ('/Fo'+(Join-Path $directory 'Probe.obj')), ('/Fe'+(Join-Path $directory 'Probe.exe')), $source)
    foreach ($include in ($properties.IncludePath -split ';')) { $args += '/I'+$include }
    $args += '/link', '/NOLOGO', '/MACHINE:X64', ('/WHOLEARCHIVE:'+(Join-Path $Sdk 'Lib/Engine.lib'))
    foreach ($libraryPath in ($properties.LibraryPath -split ';')) { $args += '/LIBPATH:'+$libraryPath }
    $args += @(Get-ChildItem -LiteralPath "$Sdk/Lib" -Filter '*.lib' | ForEach-Object FullName)
    $args += 'd3d11.lib','d3dcompiler.lib','dxgi.lib','dxguid.lib','user32.lib','gdi32.lib','ole32.lib'
    $build = Invoke-ToolchainProcess (Join-Path $toolchain.bin 'cl.exe') $args -Timeout 180000
    $log = Join-Path $directory 'build.log'
    [IO.File]::WriteAllText($log, $build.stdout + $build.stderr, [Text.UTF8Encoding]::new($false))
    if ($build.exitCode) { throw "SDK compatibility check failed for $family / $($toolchain.version). See $log" }
    $results += [ordered]@{toolset=$family;version=$toolchain.version;compilerVersion=$toolchain.compilerVersion;wholeArchiveLink='passed'}
    Write-Output "SDK compatibility: $family / $($toolchain.version) headers and whole-archive link passed."
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'results.json'), (ConvertTo-Json -InputObject @($results) -Depth 5), [Text.UTF8Encoding]::new($false))

param([Parameter(Mandatory=$true)][string]$Runtime, [Parameter(Mandatory=$true)][string]$OutputDirectory, [switch]$RequireAllToolsets)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../Toolchain.ps1"
$Runtime = [IO.Path]::GetFullPath($Runtime)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$info = Get-Content -LiteralPath "$Runtime/SDK/Sdk.json" -Raw | ConvertFrom-Json
$installed = @(Get-EngineToolchains)
if ($RequireAllToolsets) {
    foreach ($family in @('v142','v143','v145')) {
        if (!($installed | Where-Object toolset -eq $family)) { throw "Release validation requires $family. Install that C++ toolset on the release-builder." }
    }
}
foreach ($family in @('v142','v143','v145')) {
    if (!($installed | Where-Object toolset -eq $family)) { Write-Output "SKIP $family (not installed)"; continue }
    $root = Join-Path $OutputDirectory "$family project with spaces"
    if (Test-Path -LiteralPath $root) { throw "Use an empty test directory: $root" }
    [IO.Directory]::CreateDirectory("$root/Binaries") | Out-Null
    [IO.Directory]::CreateDirectory("$root/Content") | Out-Null
    Copy-Item -Path "$Runtime/Templates/SceneProject/*.dll" -Destination "$root/Binaries"
    $utf8 = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText("$root/Project.json", (@{name='Toolchain test';module='Binaries/Game.dll';content='Content'} | ConvertTo-Json), $utf8)
    [IO.File]::WriteAllText("$root/Engine.lock.json", (@{format=1;templateId='SceneProject';engineVersion=$info.version;engineConfiguration=$info.configuration} | ConvertTo-Json), $utf8)
    $args = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"$Runtime/SDK/ProjectTools.ps1",'-Action','Enable','-Project',"$root/Project.json",'-Runtime',$Runtime,'-Toolset',$family)
    $result = Invoke-ToolchainProcess 'powershell.exe' $args -Timeout 600000
    [IO.File]::WriteAllText((Join-Path $OutputDirectory "$family.log"), $result.stdout+$result.stderr, $utf8)
    if ($result.exitCode) { throw "$family build/load/run failed. See $OutputDirectory/$family.log" }
    Write-Output "$family project compiled, validated and ran in EngineRuntime."
    if ($family -eq 'v145') {
        # Copying an older source over a newer one must still invalidate its .obj.
        $source = Join-Path $root 'Source/RotatingCube.cpp'
        $original = [IO.File]::ReadAllBytes($source)
        $timestamp = [IO.File]::GetLastWriteTimeUtc($source)
        $module = (Get-Content "$root/Project.json" -Raw | ConvertFrom-Json).module
        try {
            [IO.File]::AppendAllText($source, "`n#error STALE_SOURCE_REGRESSION`n", $utf8)
            [IO.File]::SetLastWriteTimeUtc($source, [datetime]'2000-01-01')
            $buildArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"$Runtime/SDK/ProjectTools.ps1",'-Action','Build','-Project',"$root/Project.json",'-Runtime',$Runtime,'-Toolset',$family)
            $failed = Invoke-ToolchainProcess 'powershell.exe' $buildArgs -Timeout 600000
            $text = $failed.stdout + $failed.stderr
            [IO.File]::WriteAllText((Join-Path $OutputDirectory 'stale-source.log'), $text, $utf8)
            if (!$failed.exitCode -or !$text.Contains('STALE_SOURCE_REGRESSION')) { throw 'Changed source with an old timestamp was not recompiled.' }
            if ((Get-Content "$root/Project.json" -Raw | ConvertFrom-Json).module -ne $module) { throw 'Failed compilation replaced the active module.' }
        } finally {
            [IO.File]::WriteAllBytes($source, $original)
            [IO.File]::SetLastWriteTimeUtc($source, $timestamp)
        }
        Write-Output 'Changed source with an old timestamp was compiled; failed build preserved the active module.'
    }
}

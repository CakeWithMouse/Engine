param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/ProjectWorkflowTests.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset
$testParent = Join-Path $repo "Engine/build/project-workflow/$Configuration"
New-Item -ItemType Directory -Path $testParent -Force | Out-Null
$projectDir = Join-Path $testParent ("Проект с пробелами " + [Guid]::NewGuid().ToString('N'))
$runtime = Join-Path $repo "Game/build/x64/$Configuration/Runtime"
& "$repo/Engine/build/x64/$Configuration/ProjectWorkflowTests.exe" "$runtime/Templates/SceneProject/Game.dll" "$runtime/EngineContent" $projectDir
if ($LASTEXITCODE -ne 0) { throw 'Project workflow tests failed' }
$launcher = Join-Path $repo "Game/build/x64/$Configuration/EngineLauncher.exe"
foreach ($mode in @('--check-project', '--smoke-project', '--smoke-project-editor')) {
    $process = Start-Process -FilePath $launcher -ArgumentList @($mode, ('"' + (Join-Path $projectDir 'Project.json') + '"')) -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(90000)) { $process.Kill(); throw "$mode timed out" }
    if ($process.ExitCode -ne 0) { throw "$mode failed: $($process.ExitCode)" }
}
Write-Output "Project creation / primitive editing / save / reopen / game / editor passed. Fixture: $projectDir"

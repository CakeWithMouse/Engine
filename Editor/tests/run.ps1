param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [string]$PlatformToolset = 'auto', [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -PlatformToolset $PlatformToolset -EditorUiTest
$testParent = Join-Path $repo 'Engine/build/project-workflow'
New-Item -ItemType Directory -Path $testParent -Force | Out-Null
$fixture = Join-Path $testParent ('UI-' + [guid]::NewGuid().ToString('N'))
$launcher = Join-Path $repo "Game/build/x64/$Configuration/EngineLauncher.exe"
$process = Start-Process -FilePath $launcher -ArgumentList @('--create-project', ('"' + $fixture + '"'), 'UITest') -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw 'UI fixture creation failed' }
$runtime = Join-Path $repo "Game/build/x64/$Configuration/Runtime"
$modelArguments = @()
if ($EnableAssimp) { $modelArguments = @("$repo/Game/RuntimeExample/Content/Models/CesiumMan.glb") }
& "$runtime/EditorUiTests.exe" $fixture "$runtime/EngineContent" @modelArguments
if ($LASTEXITCODE -ne 0) { throw 'Editor UI tests failed' }
Write-Output "Qt UI workflow passed. Fixture: $fixture"

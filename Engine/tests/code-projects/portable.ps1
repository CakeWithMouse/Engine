param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$work=Join-Path (Split-Path $repo) ('CodeSdkChecks/'+[guid]::NewGuid().ToString('N'))
$runtime=Join-Path $work 'Выпуск SDK/Runtime'
$sourceRuntime=Join-Path $repo "Game/build/x64/$Configuration/Runtime"
[IO.Directory]::CreateDirectory($runtime) | Out-Null
Get-ChildItem -LiteralPath $sourceRuntime -File | Where-Object Extension -in '.exe','.dll','.json','.txt' | Copy-Item -Destination $runtime
foreach($name in @('SDK','EngineContent','Templates')) { Copy-Item -LiteralPath (Join-Path $sourceRuntime $name) -Destination $runtime -Recurse }
$project=Join-Path $work 'Игра вне репозитория'
$launcher=Join-Path $repo "Game/build/x64/$Configuration/EngineLauncher.exe"
$env:PAINENGINE_ENGINE_STORE=Join-Path $work 'Engines'
$create=Start-Process -FilePath $launcher -ArgumentList @('--create-project',('"'+$project+'"'),'Portable') -WindowStyle Hidden -PassThru
if(!$create.WaitForExit(60000)){$create.Kill();throw 'Create project timed out'}
if($create.ExitCode){throw 'Create project failed'}
$engineHash=(Get-FileHash -LiteralPath "$runtime/SDK/Lib/Engine.lib").Hash
& "$runtime/SDK/ProjectTools.ps1" -Action Enable -Project "$project/Project.json" -Runtime $runtime
if((Get-Content -LiteralPath "$project/Game.vcxproj" -Raw).Contains($repo)){throw 'Generated project contains a repository dependency'}
if((Get-FileHash -LiteralPath "$runtime/SDK/Lib/Engine.lib").Hash -ne $engineHash){throw 'Building game changed engine SDK'}
$state=Get-Content -LiteralPath "$project/Project.code.json" -Raw | ConvertFrom-Json
if($state.state -ne 'ready'){throw 'Portable SDK build was not activated'}
$install=Start-Process -FilePath $launcher -ArgumentList @('--install-engine',('"'+$runtime+'"')) -WindowStyle Hidden -PassThru
if(!$install.WaitForExit(60000)){$install.Kill();throw 'SDK installation timed out'}
if($install.ExitCode){throw 'SDK installation failed'}
& "$project/Build.ps1"
$vswhere=Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
$properties=& $msbuild "$project/Game.vcxproj" /nologo "/p:Configuration=$Configuration" /p:Platform=x64 '-getProperty:EngineSdk,PlatformToolset'
if($LASTEXITCODE -or !(($properties -join "`n").Contains('Engines'))){throw 'IDE could not resolve pinned SDK without explicit build properties'}
Write-Output "Portable SDK build and startup passed without Engine sources. Fixture: $work"

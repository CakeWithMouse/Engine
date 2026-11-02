param([Parameter(Mandatory=$true)][string]$Destination,[string]$Runtime='')
$ErrorActionPreference='Stop'
if(!$Runtime){$Runtime=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/x64/Release/Runtime'))}
$Destination=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $Destination){throw 'Choose a new destination; existing projects are preserved.'}
New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
$launcher=Join-Path (Split-Path $Runtime) 'EngineLauncher.exe'
$p=Start-Process -FilePath $launcher -ArgumentList @('--create-project',('"'+$Destination+'"'),'PropertiesTutorial') -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode){throw 'Cannot create tutorial project'}
& "$Runtime/SDK/ProjectTools.ps1" -Action Enable -Project "$Destination/Project.json" -Runtime $Runtime
Copy-Item -Path "$PSScriptRoot/Source/*" -Destination "$Destination/Source" -Force
Copy-Item -LiteralPath "$PSScriptRoot/Content/Scenes/Main.scene.json" -Destination "$Destination/Content/Scenes/Main.scene.json" -Force
foreach($file in @('README.md','OpenEditor.ps1','Открыть редактор.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $Destination}
$project=Get-Content -LiteralPath "$Destination/Project.json" -Raw | ConvertFrom-Json
$project.name='Туториал — пользовательские свойства'
[IO.File]::WriteAllText("$Destination/Project.json",($project|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
& "$Runtime/SDK/ProjectTools.ps1" -Action Build -Project "$Destination/Project.json" -Runtime $Runtime
Write-Output "Tutorial ready: $Destination"

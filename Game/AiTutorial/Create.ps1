param([Parameter(Mandatory=$true)][string]$Destination,[string]$Runtime='')
# Creates the AI tutorial project: a project without C++ (the standard game module runs .fsm machines),
# with the courtyard scene and the guard/visitor machines.
$ErrorActionPreference='Stop'
if(!$Runtime){$Runtime=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/x64/Release/Runtime'))}
$Destination=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $Destination){throw 'Choose a new destination; existing projects are preserved.'}
New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
$launcher=Join-Path (Split-Path $Runtime) 'EngineLauncher.exe'
$p=Start-Process -FilePath $launcher -ArgumentList @('--create-project',('"'+$Destination+'"'),'AiTutorial') -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode){throw 'Cannot create the tutorial project'}
New-Item -ItemType Directory -Path "$Destination/Content/AI" -Force | Out-Null
Copy-Item -Path "$PSScriptRoot/Content/AI/*.fsm" -Destination "$Destination/Content/AI" -Force
New-Item -ItemType Directory -Path "$Destination/Content/Scripts" -Force | Out-Null
Copy-Item -Path "$PSScriptRoot/Content/Scripts/*.vscript" -Destination "$Destination/Content/Scripts" -Force
Copy-Item -LiteralPath "$PSScriptRoot/Content/Scenes/Main.scene.json" -Destination "$Destination/Content/Scenes/Main.scene.json" -Force
foreach($file in @('README.md','OpenEditor.ps1','Открыть редактор.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $Destination}
$project=Get-Content -LiteralPath "$Destination/Project.json" -Raw | ConvertFrom-Json
$project.name='Туториал — автоматы ИИ'
[IO.File]::WriteAllText("$Destination/Project.json",($project|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
Write-Output "Tutorial ready: $Destination"

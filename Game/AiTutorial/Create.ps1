param([Parameter(Mandatory=$true)][string]$Destination,[string]$Runtime='',[switch]$Bundled)
# Creates the AI tutorial project: a project without C++ (the standard game module runs .fsm machines),
# with the courtyard scene and the guard/visitor machines.
# -Bundled: the launcher's own copy in <launcher>/Tutorials/AiTutorial, made on every launcher build. The
# previous copy is replaced, and the project runs on the launcher's runtime like the bundled examples.
$ErrorActionPreference='Stop'
if(!$Runtime){$Runtime=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/x64/Release/Runtime'))}
$Runtime=[IO.Path]::GetFullPath($Runtime)
$Destination=[IO.Path]::GetFullPath($Destination)
$launcher=Join-Path (Split-Path $Runtime) 'EngineLauncher.exe'
$final=$Destination
if($Bundled){
    if($Destination -ne [IO.Path]::GetFullPath((Join-Path (Split-Path $Runtime) 'Tutorials/AiTutorial'))){throw 'The bundled tutorial lives in <launcher>/Tutorials/AiTutorial.'}
    $Destination="$final.new"
    if(Test-Path -LiteralPath $Destination){Remove-Item -LiteralPath $Destination -Recurse -Force}
}
elseif(Test-Path -LiteralPath $Destination){throw 'Choose a new destination; existing projects are preserved.'}
New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
$p=Start-Process -FilePath $launcher -ArgumentList @('--create-project',('"'+$Destination+'"'),'AiTutorial') -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode){throw 'Cannot create the tutorial project'}
New-Item -ItemType Directory -Path "$Destination/Content/AI" -Force | Out-Null
Copy-Item -Path "$PSScriptRoot/Content/AI/*.fsm" -Destination "$Destination/Content/AI" -Force
New-Item -ItemType Directory -Path "$Destination/Content/Scripts" -Force | Out-Null
Copy-Item -Path "$PSScriptRoot/Content/Scripts/*.vscript" -Destination "$Destination/Content/Scripts" -Force
Copy-Item -LiteralPath "$PSScriptRoot/Content/Scenes/Main.scene.json" -Destination "$Destination/Content/Scenes/Main.scene.json" -Force
foreach($file in @('README.md','OpenEditor.ps1','Открыть редактор.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $Destination}
$project=Get-Content -LiteralPath "$Destination/Project.json" -Raw | ConvertFrom-Json
# The launcher lists bundled tutorials as «Туториал: <name>».
$project.name=if($Bundled){'Автоматы ИИ'}else{'Туториал — автоматы ИИ'}
[IO.File]::WriteAllText("$Destination/Project.json",($project|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
if($Bundled){
    # A bundled project is pinned to the launcher's runtime itself, not to an installed copy of it.
    $lock=Get-Content -LiteralPath "$Destination/Engine.lock.json" -Raw | ConvertFrom-Json
    $lock.engineVersion=(Get-Content -LiteralPath "$Runtime/EngineVersion.json" -Raw | ConvertFrom-Json).version
    [IO.File]::WriteAllText("$Destination/Engine.lock.json",($lock|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    # The old copy may be open in the editor: then it stays, and the next build replaces it.
    if(Test-Path -LiteralPath $final){
        try{Remove-Item -LiteralPath $final -Recurse -Force}
        catch{Remove-Item -LiteralPath $Destination -Recurse -Force -ErrorAction SilentlyContinue; Write-Warning "AI tutorial not updated, close it in the editor: $final"; exit 0}
    }
    Move-Item -LiteralPath $Destination -Destination $final
}
Write-Output "Tutorial ready: $final"

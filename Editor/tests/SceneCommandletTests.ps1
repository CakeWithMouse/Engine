param([Parameter(Mandatory=$true)][string]$Project)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scratch=Join-Path $repo ('Engine/build/scene-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$request=@{project=[IO.Path]::GetFullPath($Project);steps=@(
    @{action='new'},@{action='create';type='Primitive.Cube';name='ProbeA'},@{action='create';type='Primitive.Cube';name='ProbeB'},
    @{action='select';targets=@('ProbeA','ProbeB')},@{action='move';target='ProbeA';delta=@(2,0,0)},
    @{action='undo'},@{action='redo'},@{action='select';target='ProbeA'},
    @{action='parent';target='ProbeB';parent='ProbeA'},@{action='save';path="$scratch/Saved.scene.json"},
    @{action='duplicate';target='ProbeA'},@{action='undo'},@{action='new'},@{action='load';path="$scratch/Saved.scene.json"},
    @{action='delete';target='ProbeB'},@{action='undo'}
)}
$request|ConvertTo-Json -Depth 10|Set-Content "$scratch/request.json" -Encoding utf8
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{$env:TEMP=$scratch;$env:TMP=$scratch
    & "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" project.scene "$scratch/request.json" "$scratch/result.json" *> "$scratch/run.log"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
if($LASTEXITCODE -ne 0){throw "Scene command failed: $scratch"}
$result=Get-Content "$scratch/result.json" -Raw|ConvertFrom-Json
if(@($result.steps|Where-Object status -ne 0).Count){throw "Scene operation rejected: $scratch"}
function ObjectAt($index,$name){@($result.steps[$index].objects|Where-Object name -eq $name)[0]}
foreach($name in 'ProbeA','ProbeB'){
    if((ObjectAt 4 $name).position[0] -ne 2 -or (ObjectAt 5 $name).position[0] -ne 0 -or (ObjectAt 6 $name).position[0] -ne 2){throw 'Group move / undo / redo mismatch'}
}
if((ObjectAt 8 'ProbeB').parent -ne (ObjectAt 8 'ProbeA').id){throw 'Parent relationship not set'}
if($result.steps[10].objects.Count -ne $result.steps[9].objects.Count+1 -or $result.steps[11].objects.Count -ne $result.steps[9].objects.Count){throw 'Duplicate / undo mismatch'}
if((ObjectAt 13 'ProbeB').parent -ne (ObjectAt 13 'ProbeA').id){throw 'Save As / New / Load lost hierarchy'}
if((ObjectAt 14 'ProbeB') -or !(ObjectAt 15 'ProbeB')){throw 'Delete / undo mismatch'}
Write-Output "Scene commandlet: 16 real editor operations passed. Fixture: $scratch"

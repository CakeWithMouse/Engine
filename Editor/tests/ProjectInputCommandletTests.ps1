param([Parameter(Mandatory=$true)][string]$Project,[Parameter(Mandatory=$true)][string]$ObjectName,[int]$Key=87)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scratch=Join-Path $repo ('Engine/build/input-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$request=@{project=[IO.Path]::GetFullPath($Project);steps=@(
    @{kind='control';action='play'},@{kind='frames';count=3},@{kind='focus';enabled=$true},
    @{kind='key';key=$Key;down=$true},@{kind='frames';count=30},
    @{kind='focus';enabled=$false},@{kind='frames';count=10},
    @{kind='focus';enabled=$true},@{kind='frames';count=10},
    @{kind='key';key=$Key;down=$false},@{kind='key';key=$Key;down=$true},@{kind='frames';count=10},
    @{kind='key';key=$Key;down=$false},@{kind='frames';count=10}
)}
$request|ConvertTo-Json -Depth 10|Set-Content "$scratch/request.json" -Encoding utf8
& "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" project.input "$scratch/request.json" "$scratch/result.json" *> "$scratch/run.log"
if($LASTEXITCODE -ne 0){throw "Project input failed: $scratch"}
$result=Get-Content "$scratch/result.json" -Raw|ConvertFrom-Json
function Position($index){$objects=@($result.steps[$index].objects|Where-Object name -eq $ObjectName);if($objects.Count -ne 1){throw "Expected exactly one $ObjectName"};return $objects[0].position}
function Distance($a,$b){[Math]::Sqrt([Math]::Pow($a[0]-$b[0],2)+[Math]::Pow($a[1]-$b[1],2)+[Math]::Pow($a[2]-$b[2],2))}
if((Distance (Position 1) (Position 4)) -lt .01){throw 'Held key did not move Character'}
if((Distance (Position 4) (Position 6)) -gt .001){throw 'Lost focus did not cancel held movement'}
if((Distance (Position 6) (Position 8)) -gt .001){throw 'Regained focus resurrected held movement'}
if((Distance (Position 8) (Position 11)) -lt .01){throw 'New press after focus regain did not move'}
if((Distance (Position 11) (Position 13)) -gt .001){throw 'Released key kept moving Character'}
Write-Output "Project input commandlet: held, focus loss/regain, new press and release passed. Fixture: $scratch"

param([Parameter(Mandatory=$true)][string]$Project,[Parameter(Mandatory=$true)][string]$SpawnType,
      [Parameter(Mandatory=$true)][int]$X,[Parameter(Mandatory=$true)][int]$Y)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scratch=Join-Path $repo ('Engine/build/hud-input-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$request=@{project=[IO.Path]::GetFullPath($Project);steps=@(
    @{kind='control';action='play'},@{kind='frames';count=3},@{kind='focus';enabled=$true},
    @{kind='cursor';x=$X;y=$Y},@{kind='key';key=500;down=$true},@{kind='frames';count=1},
    @{kind='key';key=500;down=$false},@{kind='frames';count=3}
)}
$request|ConvertTo-Json -Depth 10|Set-Content "$scratch/request.json" -Encoding utf8
& "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" project.input "$scratch/request.json" "$scratch/result.json" *> "$scratch/run.log"
if($LASTEXITCODE -ne 0){throw "HUD input failed: $scratch"}
$result=Get-Content "$scratch/result.json" -Raw|ConvertFrom-Json
$before=@($result.steps[1].objects|Where-Object type -eq $SpawnType).Count
$after=@($result.steps[7].objects|Where-Object type -eq $SpawnType).Count
if($after -ne $before+1){throw "Expected one $SpawnType from real HUD click; before=$before, after=$after; $scratch"}
Write-Output "HUD input commandlet: Engine Keys::LeftButton=500 spawned exactly one $SpawnType. Fixture: $scratch"

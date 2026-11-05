param([Parameter(Mandatory=$true)][string]$Project,[string]$Material='/Game/Materials/WarmStone.mat')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$Project=[IO.Path]::GetFullPath($Project)
$source=Split-Path $Project
$manifest=Get-Content $Project -Raw | ConvertFrom-Json
$scratch=Join-Path $repo ('Engine/build/graph-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
Copy-Item -LiteralPath (Join-Path $source $manifest.content) -Destination "$scratch/Content" -Recurse
$manifest.module=[IO.Path]::GetFullPath((Join-Path $source $manifest.module))
$sceneRelative=[IO.Path]::GetRelativePath((Join-Path $source $manifest.content),(Join-Path $source $manifest.startupScene))
$manifest.content='Content';$manifest.startupScene="Content/$sceneRelative"
$manifest | ConvertTo-Json -Depth 10 | Set-Content "$scratch/Project.json" -Encoding utf8
$materialFile=Join-Path "$scratch/Content" $Material.Substring(6)
$graphFile="$materialFile.graph.json"
$request=@{project="$scratch/Project.json";material=$Material;image="$scratch/graph.png";steps=@(
    @{action='enable'},@{action='save'},@{action='preset';index=1},@{action='apply'},
    @{action='revert';lockedFile=$graphFile},@{action='revert'},
    @{action='ai';text='This is not a graph'},@{action='preset';index=2},@{action='undo'},@{action='redo'},@{action='revert'}
)}
$request | ConvertTo-Json -Depth 20 | Set-Content "$scratch/request.json" -Encoding utf8
& "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" material.graph "$scratch/request.json" "$scratch/result.json" *> "$scratch/run.log"
if($LASTEXITCODE -ne 0){throw "Graph command failed: $scratch"}
$result=Get-Content "$scratch/result.json" -Raw | ConvertFrom-Json
if(!$result.nativePreviewWindow){throw 'Material preview lacks a real HWND'}
function GraphJson($step){$step.graph | ConvertTo-Json -Depth 30 -Compress}
if(!$result.steps[1].accepted -or $result.steps[1].dirty){throw 'Initial graph save failed'}
if(!$result.steps[4].dirty -or (GraphJson $result.steps[4]) -ne (GraphJson $result.steps[3])){throw 'Failed rollback discarded editable graph'}
if($result.steps[5].dirty -or (GraphJson $result.steps[5]) -ne (GraphJson $result.steps[1]) -or $result.steps[5].hlsl -ne $result.steps[1].hlsl){throw 'Retry did not restore saved graph and HLSL'}
if($result.steps[6].accepted -or (GraphJson $result.steps[6]) -ne (GraphJson $result.steps[5])){throw 'Invalid AI text mutated graph'}
if((GraphJson $result.steps[8]) -ne (GraphJson $result.steps[5]) -or (GraphJson $result.steps[9]) -ne (GraphJson $result.steps[7])){throw 'Undo/redo graph mismatch'}
if($result.dialogs.Count -lt 3){throw 'Missing failed-write/confirmation diagnostics'}
if(!(Test-Path "$scratch/graph.png")){throw 'No actual Qt image'}
Write-Output "Material graph commandlet: save, failed rollback, retry, AI rejection, undo/redo passed. Fixture: $scratch"

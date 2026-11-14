param([Parameter(Mandatory=$true)][string]$Project,
      [string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory=Join-Path $repo ('Engine/build/ai-diagnostics-commandlet-'+[guid]::NewGuid().ToString('N')) }
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe=Join-Path $repo 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'
$request=@{project=[IO.Path]::GetFullPath($Project);steps=@(
    @{kind='diagnostics';request=@{layers=1}},
    @{kind='control';action='play'},@{kind='frames';count=100},
    @{kind='diagnostics';request=@{layers=255;selectedOnly=$false}},
    @{kind='control';action='pause'},
    @{kind='diagnostics';request=@{layers=255;selectedOnly=$false}},
    @{kind='diagnostics';request=@{layers=0}},
    @{kind='control';action='resume'},
    @{kind='diagnostics';request=@{layers=1}},
    @{kind='control';action='stop'},
    @{kind='diagnostics';request=@{layers=1}}
)}
$request | ConvertTo-Json -Depth 12 | Set-Content "$out/request.json" -Encoding utf8
& $exe project.input "$out/request.json" "$out/result.json" *> "$out/run.log"
if ($LASTEXITCODE -ne 0) { throw "AI diagnostics commandlet failed: $out" }
$result=Get-Content "$out/result.json" -Raw | ConvertFrom-Json
foreach ($check in @(@(0,'preview'),@(3,'live'),@(5,'paused'),@(6,'off'),@(8,'live'),@(10,'preview'))) {
    if ($result.steps[$check[0]].diagnostics.source -ne $check[1]) { throw "Wrong source at step $($check[0])" }
}
$live=$result.steps[3].diagnostics
if (!$live.configured -or @($live.units).Count -lt 2 -or $live.lines -le 0) { throw 'Expected actual navigation and controllers from the diagnostics fixture' }
if (@($result.steps[10].diagnostics.units).Count -ne 0) { throw 'Stop retained runtime controllers' }
$request.steps=@(@{kind='diagnostics';request='invalid'})
$request | ConvertTo-Json -Depth 12 | Set-Content "$out/invalid-request.json" -Encoding utf8
& $exe project.input "$out/invalid-request.json" "$out/invalid-result.json" *> "$out/invalid.log"
if ($LASTEXITCODE -eq 0) { throw 'Malformed diagnostics request accepted' }
if ((Get-Content "$out/invalid-result.json" -Raw) -notmatch 'diagnostics request object required') { throw 'Unexpected malformed-request error' }
Write-Output "AI diagnostics commandlet: preview/live/pause/clear/resume/stop and invalid request passed. Evidence: $out"

$ErrorActionPreference='Stop'
$rows=Import-Csv "$PSScriptRoot/feature-matrix.csv"
foreach($pair in @(@(1,'calm-city'),@(2,'curious-city'),@(3,'tired-bomber'),@(4,'ai-trash'))){
    $path="$PSScriptRoot/$($pair[1])/evidence.json"
    if(!(Test-Path $path)){continue}
    $document=Get-Content $path -Raw|ConvertFrom-Json
    $entries=if($document.features){$document.features}else{$document.evidence}
    foreach($entry in $entries){
        if($entry.status -eq 'pass'){$entry.status='passed'}
        $row=$rows|Where-Object feature_id -eq $entry.feature_id
        if(!$row){throw "Unknown feature ID $($entry.feature_id) in $path"}
        if($entry.status -notin @('passed','partial','failed','blocked','not-tested')){throw "Unknown status $($entry.status) in $path"}
        $row.('developer_'+$pair[0])=$entry.status
        $row.('evidence_'+$pair[0])="$($pair[1])/evidence.json#$($entry.feature_id)"
    }
}
foreach($row in $rows){
    $statuses=1..4|ForEach-Object {$row.('developer_'+$_)}
    $row.attempted_developers=@($statuses|Where-Object{$_ -in @('passed','partial','failed','blocked')}).Count
    $row.successful_developers=@($statuses|Where-Object{$_ -eq 'passed'}).Count
    $row.coverage=if([int]$row.successful_developers -ge 2){'covered'}elseif([int]$row.attempted_developers -ge 2){'partial'}else{'pending'}
}
$rows|Export-Csv "$PSScriptRoot/feature-matrix.csv" -NoTypeInformation -Encoding utf8
$rows|Group-Object coverage|Select-Object Name,Count

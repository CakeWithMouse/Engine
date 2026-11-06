param([Parameter(Mandatory=$true)][string]$Project,
    [string]$Commandlet="$PSScriptRoot/../../Game/build/x64/Release/Runtime/EngineCommandlets.exe",
    [Parameter(Mandatory=$true)][string]$Model,
    [string]$Output="$PSScriptRoot/../../Engine/build/material-slots-$([guid]::NewGuid().ToString('N'))")
$ErrorActionPreference='Stop'
$Project=[IO.Path]::GetFullPath($Project);$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
function Invoke-Commandlet($name,$request,$label){
    $request|ConvertTo-Json -Depth 30|Set-Content "$Output/$label.request.json" -Encoding utf8
    & $Commandlet $name "$Output/$label.request.json" "$Output/$label.result.json" *> "$Output/$label.log"
    if($LASTEXITCODE -ne 0){throw "Commandlet failed: $label"}
    return Get-Content "$Output/$label.result.json" -Raw|ConvertFrom-Json
}
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
try {
    $env:TEMP=$Output;$env:TMP=$Output
    $import=Invoke-Commandlet 'asset.import' @{source=[IO.Path]::GetFullPath($Model);content=(Join-Path (Split-Path $Project) 'Content')} 'import'
    $slots=@($import.materialSlots.PSObject.Properties|Sort-Object Name)
    if($slots.Count -lt 2 -or $import.material){throw 'Expected at least two imported slots without a global override'}
    $slot=$slots[1].Name;$original=$slots[1].Value;$replacement=$slots[0].Value
    $request=@{project=$Project;steps=@(
        @{action='new'},@{action='model';path=$import.asset;name='Multi'},
        @{action='property';target='Multi';path="/materialSlots/$slot";value=$replacement},
        @{action='undo'},@{action='redo'},
        @{action='property';target='Multi';path="/materialSlots/$slot";value='/Game/../outside.mat'},
        @{action='save';path="$Output/Slots.scene.json"},@{action='duplicate';target='Multi'},@{action='undo'},
        @{action='new'},@{action='load';path="$Output/Slots.scene.json"},
        @{action='material';target='Multi';path=$replacement},@{action='material';target='Multi';path=''}
    )}
    $result=Invoke-Commandlet 'project.scene' $request 'scene'
    for($i=0;$i -lt $result.steps.Count;$i++){
        if($i -eq 5){if($result.steps[$i].status -eq 0){throw 'Escaping slot path accepted'}}
        elseif($result.steps[$i].status -ne 0){throw "Scene step $i failed: $($result.steps[$i].error)"}
    }
    function ObjectAt($index){@($result.steps[$index].objects|Where-Object name -eq 'Multi')[0]}
    foreach($index in @(2,4,5,10)){
        if((ObjectAt $index).projectData.materialSlots.$slot -ne $replacement){throw "Slot mismatch at $index"}
    }
    if((ObjectAt 3).projectData.materialSlots.$slot -ne $original){throw 'Undo did not restore original slot'}
    if($result.steps[7].objects.Count -ne 2 -or $result.steps[8].objects.Count -ne 1){throw 'Duplicate/undo mismatch'}
    foreach($object in $result.steps[7].objects){if($object.projectData.materialSlots.$slot -ne $replacement){throw 'Duplicate lost slots'}}
    if((ObjectAt 11).material -ne $replacement -or (ObjectAt 12).material){throw 'Whole-object override/clear mismatch'}
    if((ObjectAt 12).projectData.materialSlots.$slot -ne $replacement){throw 'Global override destroyed slot assignments'}
    Write-Output "Material slots: import + 13 editor operations passed. Evidence: $Output"
} finally {$env:TEMP=$previousTemp;$env:TMP=$previousTmp}

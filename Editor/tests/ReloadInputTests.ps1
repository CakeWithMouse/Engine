param([Parameter(Mandatory=$true)][string]$Project,
      [string]$Output="$PSScriptRoot/../../Engine/build/reload-input/$([guid]::NewGuid().ToString('N'))")
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/../..");$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path $Output){throw 'Use a new output directory'}
[IO.Directory]::CreateDirectory($Output)|Out-Null
$Project=[IO.Path]::GetFullPath($Project);$manifest=Get-Content $Project -Raw|ConvertFrom-Json
Copy-Item -LiteralPath (Join-Path (Split-Path $Project) $manifest.module) -Destination "$Output/Candidate.dll"
@{project=$Project;candidate="$Output/Candidate.dll";key=84;clickX=95;clickY=131}|ConvertTo-Json|Set-Content "$Output/request.json" -Encoding utf8
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
[IO.Directory]::CreateDirectory("$Output/Temp")|Out-Null
try {
    # Qt history and Engine atomic scene writes must share a writable directory,
    # including restricted Windows hosts whose default AppContainer temp denies IO.
    $env:TEMP="$Output/Temp";$env:TMP=$env:TEMP
    & "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" project.reload-input "$Output/request.json" "$Output/result.json" *> "$Output/console.log"
} finally {$env:TEMP=$previousTemp;$env:TMP=$previousTmp}
if($LASTEXITCODE){throw 'Reload command failed'}
$value=Get-Content "$Output/result.json" -Raw|ConvertFrom-Json
$checks=@()
foreach($phase in 'original','reloaded'){
    foreach($step in @(@('before',0),@('afterKey',1),@('afterClick',2))){
        $count=@($value.$phase.($step[0])|Where-Object type -eq 'Trash.Projectile').Count
        $checks+=@{phase=$phase;step=$step[0];count=$count;passed=$count -eq $step[1]}
    }
}
$checks|ConvertTo-Json|Set-Content "$Output/results.json" -Encoding utf8
if(@($checks|Where-Object {!$_.passed}).Count){throw 'BUG007: T/HUD failed before or after real DLL replacement'}
Write-Output 'BUG007: six gameplay assertions passed, including T and HUD after DLL replacement.'

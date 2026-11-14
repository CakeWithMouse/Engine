param([string]$Repository='E:/Engine')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath($Repository)
$work="$repo/Engine/build/playtests/20261107-visual"
$env:PAINENGINE_ENGINE_STORE="$work/store-verified"
$env:TEMP=$work
$env:TMP=$work
$launcher="$repo/Game/build/x64/Release/EngineLauncher.exe"
function Invoke-Launcher([string[]]$Arguments, [string]$Log) {
    $start=[Diagnostics.ProcessStartInfo]::new($launcher)
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::Start($start)
    $stdout=$process.StandardOutput.ReadToEndAsync()
    $stderr=$process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    ($stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()) | Set-Content $Log
    if($process.ExitCode -ne 0){throw "Launcher failed ($($process.ExitCode)): $Arguments; log=$Log"}
}
$projects=@(
    @('calm','calm-city/CalmCityFinal'),
    @('curious','curious-city/final-city'),
    @('bomber','tired-bomber/BomberFinalR5'),
    @('trash','ai-trash/TrashGame')
)
$results=@()
foreach($item in $projects){
    $name=$item[0]
    $source="$repo/Engine/build/playtests/20261107-r5/$($item[1])/Project.json"
    $copy="$work/verified-$name"
    Invoke-Launcher @('--upgrade-project',$source,$copy) "$work/upgrade-$name-final.log"
    Invoke-Launcher @('--build-project',"$copy/Project.json") "$work/build-$name-final.log"
    Invoke-Launcher @('--smoke-project',"$copy/Project.json") "$work/game-$name-final.log"
    Invoke-Launcher @('--smoke-project-editor',"$copy/Project.json") "$work/editor-$name-final.log"
    $results += @{project=$name;path=$copy;upgrade=0;build=0;game=0;editor=0}
    Write-Output "$name upgrade/build/game/editor PASS"
}
$results | ConvertTo-Json -Depth 5 | Set-Content "$work/projects-final.json"

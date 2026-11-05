param([string]$Output="$PSScriptRoot/../../../Engine/build/launcher-cli/$([guid]::NewGuid().ToString('N'))",
    [switch]$Observe, [switch]$SkipCpp)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/../../..")
$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory'}
[IO.Directory]::CreateDirectory($Output)|Out-Null
$results=[Collections.Generic.List[object]]::new()
$previousStore=$env:PAINENGINE_ENGINE_STORE
$env:PAINENGINE_ENGINE_STORE="$Output/EmptyStore"
function InvokeLauncher($name,$arguments){
    $start=[Diagnostics.ProcessStartInfo]::new("$repo/Game/build/x64/Release/EngineLauncher.exe")
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $start.StandardOutputEncoding=[Text.UTF8Encoding]::new($false,$true)
    $start.StandardErrorEncoding=[Text.UTF8Encoding]::new($false,$true)
    foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
    $p=[Diagnostics.Process]::Start($start)
    $stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
    if(!$p.WaitForExit(180000)){$p.Kill();throw "Launcher timeout: $name"}
    $value=@{exit=$p.ExitCode;stdout=$stdout.GetAwaiter().GetResult();stderr=$stderr.GetAwaiter().GetResult()}
    $p.Dispose()
    $value|ConvertTo-Json|Set-Content "$Output/$name.json" -Encoding utf8
    return $value
}
function Check($id,$passed){$results.Add(@{id=$id;passed=[bool]$passed});Write-Output "${id}: $passed"}
try {
    $r=InvokeLauncher Help @('--help')
    Check Help ($r.exit -eq 0 -and $r.stderr -eq '' -and $r.stdout.Contains('--create-project') -and $r.stdout.Contains('--enable-code') -and $r.stdout.Contains('CodeBuild.log'))
    $r=InvokeLauncher HelpAlias @('-h');Check HelpAlias ($r.exit -eq 0 -and $r.stdout.Contains('--build-project'))
    $r=InvokeLauncher HelpArguments @('--help','extra');Check HelpArguments ($r.exit -eq 2 -and $r.stderr.Contains('arguments'))
    $r=InvokeLauncher Unknown @('--неизвестно');Check Unknown ($r.exit -eq 2 -and $r.stderr.Contains('--неизвестно') -and $r.stderr.Contains('--help') -and $r.stdout -eq '')
    $r=InvokeLauncher UnsupportedCpp @('--create-code-project',"$Output/Unused",'City');Check UnsupportedCpp ($r.exit -eq 2 -and $r.stderr.Contains('--create-project') -and $r.stderr.Contains('--enable-code') -and !(Test-Path "$Output/Unused"))
    foreach($command in @('--create-project','--enable-code','--build-project')){
        $r=InvokeLauncher ($command.Substring(2)+'-arguments') @($command)
        Check ($command+' arguments') ($r.exit -eq 2 -and $r.stderr.Contains('arguments') -and $r.stderr.Contains($command))
    }
    $missing="$Output/Нет проекта/Project.json"
    $r=InvokeLauncher Missing @('--check-project',$missing);Check Missing ($r.exit -eq 1 -and $r.stderr.Length -gt 0 -and $r.stdout -eq '')
    $existing="$Output/Существующая папка"
    [IO.Directory]::CreateDirectory($existing)|Out-Null
    [IO.File]::WriteAllText("$existing/marker.txt",'Не изменять')
    $hash=(Get-FileHash "$existing/marker.txt").Hash
    $r=InvokeLauncher Existing @('--create-project',$existing,'Город')
    Check Existing ($r.exit -eq 1 -and $r.stderr.Contains([IO.Path]::GetFullPath($existing)) -and $r.stderr.Contains('NEW') -and (Get-FileHash "$existing/marker.txt").Hash -eq $hash -and @(Get-ChildItem $existing).Count -eq 1)
    $empty="$Output/Пустая папка";[IO.Directory]::CreateDirectory($empty)|Out-Null
    $r=InvokeLauncher Empty @('--create-project',$empty,'Город');Check Empty ($r.exit -eq 1 -and $r.stderr.Contains('NEW') -and @(Get-ChildItem $empty).Count -eq 0)
    $project="$Output/Новый город"
    $r=InvokeLauncher Create @('--create-project',$project,'Новый город');Check Create ($r.exit -eq 0 -and (Test-Path "$project/Project.json"))
    if(!$SkipCpp){
        $r=InvokeLauncher Enable @('--enable-code',"$project/Project.json");Check Enable ($r.exit -eq 0 -and (Test-Path "$project/Source/ProjectComponent.cpp"))
        $source="$project/Source/ProjectComponent.cpp";$original=[IO.File]::ReadAllText($source)
        $module=(Get-Content "$project/Project.json" -Raw|ConvertFrom-Json).module
        try {
            [IO.File]::WriteAllText($source,$original+"`n#error CLI_failure_probe`n")
            $r=InvokeLauncher FailedBuild @('--build-project',"$project/Project.json")
            Check FailedBuild ($r.exit -eq 1 -and $r.stderr.Contains([IO.Path]::GetFullPath("$project/CodeBuild.log")) -and
                (Get-Content "$project/Project.json" -Raw|ConvertFrom-Json).module -eq $module -and
                [IO.File]::ReadAllText("$project/CodeBuild.log").Contains('C1189'))
        } finally { [IO.File]::WriteAllText($source,$original) }
        $r=InvokeLauncher Build @('--build-project',"$project/Project.json");Check Build ($r.exit -eq 0 -and (Test-Path "$project/CodeBuild.log"))
        $r=InvokeLauncher Smoke @('--smoke-project',"$project/Project.json");Check Smoke ($r.exit -eq 0)
    }
} finally {
    $env:PAINENGINE_ENGINE_STORE=$previousStore
    $results|ConvertTo-Json|Set-Content "$Output/results.json" -Encoding utf8
}
if(!$Observe -and @($results|Where-Object {!$_.passed}).Count){throw 'CLI regression failed'}
$global:LASTEXITCODE=0

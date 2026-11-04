param([string]$Output="$PSScriptRoot/../../Engine/build/cpp-workflow/$([guid]::NewGuid().ToString('N'))")
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/../..")
$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory; existing projects are never overwritten.'}
[IO.Directory]::CreateDirectory($Output)|Out-Null
$launcher="$repo/Game/build/x64/Release/EngineLauncher.exe"
$commandlet="$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe"
$project="$Output/Project/Project.json"
$previousStore=$env:PAINENGINE_ENGINE_STORE;$previousModules=$env:PSModulePath
$results=[Collections.Generic.List[object]]::new()
function Check($id,$condition){$results.Add(@{id=$id;passed=[bool]$condition});if(!$condition){throw "Failed: $id"}}
function Launcher($arguments){
    $p=Start-Process -FilePath $launcher -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit(180000)){$p.Kill();throw 'Launcher timeout'}
    return $p.ExitCode
}
try {
    # No installed runtime may silently replace the build being tested.
    $env:PAINENGINE_ENGINE_STORE="$Output/EmptyEngineStore"
    $env:PSModulePath='C:/Program Files/PowerShell/7/Modules'
    Check Create ((Launcher @('--create-project',('"'+"$Output/Project"+'"'),'CppRegression')) -eq 0)
    Check BUG005_LauncherEnable ((Launcher @('--enable-code',('"'+$project+'"'))) -eq 0)
    Copy-Item "$Output/Project/CodeBuild.log" "$Output/launcher-enable.log"
    $before=Get-Content $project -Raw|ConvertFrom-Json
    $source="$Output/Project/Source/ProjectComponent.cpp"
    $original=[IO.File]::ReadAllText($source)
    [IO.File]::WriteAllText($source,$original+"`n#error Кириллица_проверка_сборки`n",[Text.UTF8Encoding]::new($false))
    @{project=$project}|ConvertTo-Json|Set-Content "$Output/build.request.json" -Encoding utf8
    & $commandlet project.build "$Output/build.request.json" "$Output/failed.result.json" *> "$Output/failed.log"
    $failedExit=$LASTEXITCODE;$failed=Get-Content "$Output/failed.result.json" -Raw|ConvertFrom-Json
    Check BUG004_FailureReturns ($failedExit -eq 1 -and $failed.windowEnabled -and !$failed.replaced)
    Check BUG006_Utf8 ($failed.console.Contains('Кириллица_проверка_сборки') -and !$failed.console.Contains([char]0xfffd))
    Check BUG005_EditorModules (!$failed.console.Contains('Get-FileHash') -and $failed.console.Contains('error C1189'))
    Check PreviousDllPreserved ((Get-Content $project -Raw|ConvertFrom-Json).module -eq $before.module)
    [IO.File]::WriteAllText($source,$original+"`n// rebuild after rejected compiler candidate`n",[Text.UTF8Encoding]::new($false))
    & $commandlet project.build "$Output/build.request.json" "$Output/success.result.json" *> "$Output/success.log"
    $success=Get-Content "$Output/success.result.json" -Raw|ConvertFrom-Json
    Check BUG004_SuccessReturns ($LASTEXITCODE -eq 0 -and $success.windowEnabled -and $success.replaced -and $success.buildSucceeded)
    Check NewDllActivated ((Get-Content $project -Raw|ConvertFrom-Json).module -ne $before.module)
} finally {
    $env:PAINENGINE_ENGINE_STORE=$previousStore;$env:PSModulePath=$previousModules
    $results|ConvertTo-Json|Set-Content "$Output/results.json" -Encoding utf8
    $results|ForEach-Object {Write-Output "$($_.id): $($_.passed)"}
}

param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto', [switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$NoBuild) {
    & "$repo/Engine/build.ps1" -Target EngineLauncher -Configuration $Configuration -PlatformToolset $PlatformToolset
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/runtime/Fixtures.proj" -Configuration $Configuration -PlatformToolset $PlatformToolset
}
$output = "$repo/Game/build/x64/$Configuration"
$runtime = "$output/Runtime/EngineRuntime.exe"
$project = "$output/Runtime/Projects/RuntimeExample/Project.json"
$fixtures = "$repo/Engine/build/x64/$Configuration/RuntimeFixtures"
$work = "$repo/Engine/build/runtime-checks/$Configuration/$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $work -Force | Out-Null
$script:passed = 0

function Run([string]$Name, [string]$File, [string]$Arguments, [int]$Expected = 0, [string]$InputText = '') {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $File
    $info.Arguments = $Arguments
    $info.WorkingDirectory = $work
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.RedirectStandardInput = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.Write($InputText)
        $process.StandardInput.Close()
        if (!$process.WaitForExit(20000)) { $process.Kill(); throw "Timeout: $Name" }
        $log = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        $log | Set-Content -LiteralPath "$work/$Name.log" -Encoding UTF8
        if ($process.ExitCode -ne $Expected) { throw "$Name expected $Expected, got $($process.ExitCode):`n$log" }
        return $log
    } finally { $process.Dispose() }
}
function Require([bool]$Value, [string]$Message) { if (!$Value) { throw $Message } }
function Count([string]$Log, [string]$Event) { return @($Log -split '\r?\n' | Where-Object { $_ -ceq $Event }).Count }
function Ordered([string]$Log, [string[]]$Events) {
    $cursor = 0
    foreach ($event in $Events) {
        $index = $Log.IndexOf($event, $cursor, [StringComparison]::Ordinal)
        Require ($index -ge 0) "Missing/out-of-order event: $event`n$Log"
        $cursor = $index + $event.Length
    }
}
function Pass([string]$Name) { $script:passed++; Write-Host "PASS: $Name" }
function ProjectFile([string]$Directory, [string]$Module) {
    New-Item -ItemType Directory -Path "$Directory/Content" -Force | Out-Null
    @{ name = 'Test project'; module = $Module; content = 'Content' } | ConvertTo-Json | Set-Content -LiteralPath "$Directory/Project.json" -Encoding UTF8
    return "$Directory/Project.json"
}

foreach ($mode in @('game','editor')) {
    $prefix = if ($mode -eq 'game') { 'Game' } else { 'Editor' }
    $log = Run $mode $runtime "--project `"$project`" --mode $mode --frames 3"
    Ordered $log @('EngineStarting','ResourcesReady','EngineReady','ModuleLoaded',"${prefix}Create","${prefix}Start", "${prefix}Update", 'EngineUpdate', 'EngineRender')
    Ordered $log @("${prefix}Updates=3", "${prefix}Stop", "${prefix}Destroy", 'ModuleUnloaded', 'ResourcesStopped', 'EngineStopped')
    Require ((Count $log "${prefix}Update") -eq 3) 'Incorrect update count'
    Require ((Count $log 'EngineFrameEnd (stub)') -eq 3) 'Incorrect frame count'
    Require ((Count $log "${prefix}Destroy") -eq 1) 'Session must be destroyed exactly once'
    if ($mode -eq 'editor') {
        Require ($log -notmatch 'Game(Create|Start|Update)') 'Editor started gameplay'
        Ordered $log @('EngineRender', 'EditorOverlay', 'EngineFrameEnd')
        Require ((Count $log 'EditorOverlay (stub)') -eq 3) 'Incorrect overlay count'
    }
    Pass $mode
}

$log = Run 'engine-failure' $runtime "--project `"$project`" --mode game --frames 3 --test-fail-engine" 1
Require ($log -notmatch 'ModuleLoaded|GameCreate|EngineReady') 'Module called after engine failure'
Ordered $log @('ResourcesReady','ResourcesStopped','EngineStopped')
Pass 'engine failure cleanup'

$missing = ProjectFile "$work/missing" 'absent.dll'
$log = Run 'missing-dll' $runtime "--project `"$missing`" --mode game --frames 3" 1
Require ($log -match 'Cannot load module') 'Missing DLL error not reported'
Ordered $log @('EngineReady','ResourcesStopped','EngineStopped')
Pass 'missing DLL'

foreach ($case in @('BadVersion','WrongKind','MissingExport','FailGameStart','RequestExit','FailUpdate')) {
    $p = ProjectFile "$work/$case" "$fixtures/$case.dll"
    $expected = if ($case -eq 'RequestExit') { 0 } else { 1 }
    $log = Run $case $runtime "--project `"$p`" --mode game --frames 3" $expected
    Ordered $log @('ModuleLoaded','ModuleUnloaded','ResourcesStopped','EngineStopped')
    if ($case -in @('BadVersion','WrongKind','MissingExport')) {
        Require ($log -notmatch 'FixtureCreate') 'Invalid module created a session'
        Require ($log -match 'Incompatible|Missing export') 'Missing ABI error'
    } elseif ($case -eq 'FailGameStart') {
        Ordered $log @('FixtureCreate','FixtureStart','FixtureDestroy','ModuleUnloaded')
        Require ($log -notmatch 'FixtureUpdate|FixtureStop') 'Failed start updated/stopped session'
        Require ((Count $log 'FixtureDestroy') -eq 1) 'Failed session destroy count'
    } else {
        Require ((Count $log 'FixtureUpdate') -eq 1) 'Request exit/update failure did not stop loop'
        Ordered $log @('FixtureUpdate','FixtureStop','FixtureDestroy','ModuleUnloaded')
    }
    Pass $case
}

# Isolated installation: never overwrite the deployed Editor.dll.
$isolated = "$work/editor failure"
New-Item -ItemType Directory -Path $isolated -Force | Out-Null
Copy-Item -LiteralPath $runtime -Destination "$isolated/EngineRuntime.exe"
Copy-Item -LiteralPath "$fixtures/FailEditorStart.dll" -Destination "$isolated/Editor.dll"
$log = Run 'editor-start-failure' "$isolated/EngineRuntime.exe" "--project `"$project`" --mode editor --frames 3" 1
Ordered $log @('EngineReady','FixtureCreate','FixtureStart','FixtureDestroy','ModuleUnloaded','ResourcesStopped','EngineStopped')
Require ($log -notmatch 'FixtureUpdate|FixtureStop') 'Failed editor start ran callbacks'
Require ((Count $log 'FixtureDestroy') -eq 1) 'Editor failure destroy count'
Pass 'editor start failure'

# Editor must work even when the project's game DLL does not exist.
$log = Run 'editor-no-game' $runtime "--project `"$missing`" --mode editor --frames 1"
Require ($log -match 'EditorUpdates=1' -and $log -notmatch 'GameCreate') 'Editor requires game DLL'
Pass 'editor without game DLL'

$log = Run 'interactive-exit' $runtime "--project `"$project`" --mode game" 0 "`nq`n"
Require ($log -match 'GameUpdates=1') 'Interactive Enter/q contract failed'
Pass 'interactive exit'

$index = 0
foreach ($arguments in @('--mode invalid --frames 3','--mode game --frames 0','--mode game --frames -1','--mode game --frames 3x','--mode game --frames 18446744073709551616','--mode game --mode editor','--mode game --unknown value')) {
    $index++
    $log = Run "bad-args-$index" $runtime "--project `"$project`" $arguments" 1
    Require ($log -notmatch 'EngineStarting') 'Invalid arguments reached engine'
}
Pass 'invalid arguments'

$invalid = "$work/invalid.json"
$index = 0
foreach ($json in @('{}','{"name":"x","module":"x","content":1}','{"name":"x","name":"y","module":"x","content":"Content"}', '{"name":"x","module":"x","content":"Content",}', '{"name":"\uD800","module":"x","content":"Content"}', '{"name":"x\u0000","module":"x","content":"Content"}')) {
    $index++
    Set-Content -LiteralPath $invalid -Value $json -Encoding UTF8
    $log = Run "bad-project-$index" $runtime "--project `"$invalid`" --mode game --frames 1" 1
    Require ($log -notmatch 'EngineStarting') 'Invalid project reached engine'
}
Pass 'invalid projects'

# Copy complete new deployment into a path with spaces; launch from unrelated CWD.
$spaced = "$work/deployment with spaces"
New-Item -ItemType Directory -Path $spaced -Force | Out-Null
Copy-Item -LiteralPath "$output/Runtime" -Destination "$spaced/Runtime" -Recurse
Copy-Item -LiteralPath "$output/EngineLauncher.exe" -Destination "$spaced/EngineLauncher.exe"
foreach ($mode in @('game','editor')) {
    $log = Run "spaced-$mode" "$spaced/Runtime/EngineRuntime.exe" "--project Projects/RuntimeExample/Project.json --mode $mode --frames 2"
    Require ($log -match 'Updates=2') 'Relative host project path failed'
}
Pass 'spaces and unrelated cwd'
foreach ($option in @('--check','--check-editor','--smoke','--smoke-editor')) {
    $null = Run "launcher-$($option.TrimStart('-'))" "$spaced/EngineLauncher.exe" "$option RuntimeExample"
}
$null = Run 'legacy-editor-disabled' "$output/EngineLauncher.exe" '--check-editor Pong' 1
Pass 'launcher both modes and legacy editing rejection'
Write-Host "$script:passed runtime checks passed. Logs: $work"

param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$PlatformToolset = 'auto', [switch]$NoBuild, [switch]$EnableAssimp)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$NoBuild) {
    & "$repo/Engine/build.ps1" -Target EngineLauncher -Configuration $Configuration -PlatformToolset $PlatformToolset -EnableAssimp:$EnableAssimp
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

# The editor module is a Qt build, and Qt is not in the repository. When it was not produced the
# editor checks have nothing to run against, so they are reported as skipped instead of failing -
# the same way run-games.ps1 reports Katamari when Assimp is missing.
$editorModule = Join-Path (Split-Path $runtime) 'Editor.dll'
$editorBuilt = (Test-Path -LiteralPath $editorModule) -and
               ((Get-Item -LiteralPath $editorModule).LastWriteTime -ge (Get-Item -LiteralPath $runtime).LastWriteTime)
if (-not $editorBuilt) {
    Write-Host 'Editor module: not built (requires Qt 6). Editor checks are skipped.'
}

$gameLog = $null
$runtimeModes = if ($editorBuilt) { @('game','editor') } else { @('game') }
foreach ($mode in $runtimeModes) {
    $prefix = if ($mode -eq 'game') { 'Game' } else { 'Editor' }
    $log = Run $mode $runtime "--project `"$project`" --mode $mode --frames 3"
    if ($mode -eq 'game') { $gameLog = $log }
    Ordered $log @('EngineStarting','ResourcesReady','EngineReady','ModuleLoaded',"${prefix}Create","${prefix}Start")
    Ordered $log @("${prefix}Updates=3", "${prefix}Stop", "${prefix}Destroy", 'ModuleUnloaded', 'ResourcesStopped', 'EngineStopped')
    $frameEvent = if ($mode -eq 'editor') { 'EngineFrameEnd (viewport)' } else { 'EngineFrameEnd (game window)' }
    Require ((Count $log $frameEvent) -eq 3) 'Incorrect frame count'
    Require ((Count $log "${prefix}Destroy") -eq 1) 'Session must be destroyed exactly once'
    if ($mode -eq 'editor') {
        Require ($log -notmatch 'Game(Create|Start|Update)') 'Editor started gameplay'
        Ordered $log @('ViewportReady', 'EditorViewportAttach ok', 'EngineRender (viewport)', 'ViewportStopped', 'EditorStop')
        Require ((Count $log 'EngineRender (viewport)') -eq 3) 'Incorrect viewport frame count'
    } else {
        Ordered $log @('GameUpdate', 'EngineFrame (game window)', 'EngineFrameEnd (game window)')
        Require ((Count $log 'GameUpdate') -eq 3) 'Incorrect update count'
        $firstReadMatches = [regex]::Matches($log, '(?m)^\[VfsService\] first read via module: (/Engine/Shaders/.+)$')
        Require ($firstReadMatches.Count -eq 1) "Expected exactly 1 host VFS first read via module, got $($firstReadMatches.Count)"
    }
    Pass $mode
}

Require ((Count $gameLog 'EngineFrame (game window)') -eq 3) 'Incorrect game window frame count'
Require ((Count $gameLog 'EngineFrameEnd (stub)') -eq 0) 'Stub frame found in game window log'
Pass 'game window'

if ($gameLog -match '\[RuntimeExample\] SkeletalMesh loaded: bones=\d+ clip=') {
    Pass 'skeletal runtime check (loaded with Assimp)'
} elseif ($gameLog -match '\[RuntimeExample\] SkeletalMesh skipped: Assimp not enabled') {
    Pass 'skeletal runtime check (skipped without Assimp)'
} else {
    throw "Expected SkeletalMesh loaded or skipped log in game output"
}
if ($EnableAssimp) {
    Require ($gameLog -match '\[RuntimeExample\] SkeletalMesh loaded: bones=\d+ clip=') "Expected SkeletalMesh loaded log when -EnableAssimp passed"
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
# The Assimp-enabled runtime also needs the deployment's adjacent DLLs.
Get-ChildItem -LiteralPath (Split-Path $runtime) -Filter '*.dll' | Where-Object Name -ne 'Editor.dll' |
    Copy-Item -Destination $isolated
Copy-Item -LiteralPath "$fixtures/FailEditorStart.dll" -Destination "$isolated/Editor.dll"
$log = Run 'editor-start-failure' "$isolated/EngineRuntime.exe" "--project `"$project`" --mode editor --frames 3" 1
Ordered $log @('EngineReady','FixtureCreate','FixtureStart','FixtureDestroy','ModuleUnloaded','ResourcesStopped','EngineStopped')
Require ($log -notmatch 'FixtureUpdate|FixtureStop') 'Failed editor start ran callbacks'
Require ((Count $log 'FixtureDestroy') -eq 1) 'Editor failure destroy count'
Pass 'editor start failure'

# Editor must work even when the project's game DLL does not exist.
if ($editorBuilt) {
    $log = Run 'editor-no-game' $runtime "--project `"$missing`" --mode editor --frames 1"
    Require ($log -match 'EditorUpdates=1' -and $log -notmatch 'GameCreate') 'Editor requires game DLL'
    Pass 'editor without game DLL'
}

$interactive = ProjectFile "$work/interactive" "$output/Runtime/Projects/RuntimeExample/Game.dll"
$log = Run 'interactive-exit' $runtime "--project `"$interactive`" --mode game" 0 "`nq`n"
Require ($log -match '(?m)^GameUpdates=1\r?$') 'Interactive Enter/q contract failed'
Pass 'interactive exit'

$reloadProj = ProjectFile "$work/reload" "$output/Runtime/Projects/RuntimeExample/Game.dll"
$reloadLog = Run 'interactive-reload' $runtime "--project `"$reloadProj`" --mode game" 0 "`nr`nq`n"
$bindCount = [regex]::Matches($reloadLog, '(?m)^\[VfsRead\] Bind host VFS service v\d+').Count
$unbindCount = [regex]::Matches($reloadLog, '(?m)^\[VfsRead\] Unbind').Count
Require ($bindCount -eq 2) "Expected 2 Bind host VFS service logs in hot-reload, got $bindCount"
Require ($unbindCount -eq 2) "Expected 2 Unbind logs in hot-reload, got $unbindCount"
Require ($reloadLog -notmatch 'ModuleReloadError') "Hot reload contained reload error:`n$reloadLog"
Ordered $reloadLog @('GameCreate','GameStart','GameStop','GameDestroy','ModuleUnloaded','ModuleLoaded','GameCreate','GameStart','ModuleReloadSuccess','GameStop','GameDestroy','ModuleUnloaded')
Pass 'hot reload'


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
foreach ($mode in $runtimeModes) {
    $log = Run "spaced-$mode" "$spaced/Runtime/EngineRuntime.exe" "--project Projects/RuntimeExample/Project.json --mode $mode --frames 2"
    Require ($log -match 'Updates=2') 'Relative host project path failed'
}
Pass 'spaces and unrelated cwd'
$launcherOptions = if ($editorBuilt) { @('--check','--check-editor','--smoke','--smoke-editor') } else { @('--check','--smoke') }
foreach ($option in $launcherOptions) {
    $null = Run "launcher-$($option.TrimStart('-'))" "$spaced/EngineLauncher.exe" "$option RuntimeExample"
}
$null = Run 'katamari-editor-disabled' "$output/EngineLauncher.exe" '--check-editor Katamari' 1
Pass 'launcher both modes and unsupported editing rejection'
if ($editorBuilt) {
    foreach ($scene in @('BasicExamples','Pong','SunGame','Battle')) {
        $null = Run "launcher-preview-$scene" "$output/EngineLauncher.exe" "--smoke-editor $scene"
        $sceneLog = Run "preview-$scene" $runtime "--project Projects/$scene/Project.json --mode editor --frames 3"
        Require ($sceneLog.Contains("PreviewScene: $scene")) 'Launcher selected the wrong preview'
        Ordered $sceneLog @('ViewportReady','EngineRender (viewport)','ViewportStopped','EngineStopped')
        Require ((Count $sceneLog 'EngineRender (viewport)') -eq 3) 'Preview did not render three frames'
        Pass "preview $scene"
    }
    $unknownPreview = ProjectFile "$work/unknown-preview" 'missing.dll'
    @{name='Unknown preview';module='missing.dll';content='Content';preview='UnknownScene'} | ConvertTo-Json | Set-Content -LiteralPath $unknownPreview -Encoding UTF8
    $log = Run 'unknown-preview' $runtime "--project `"$unknownPreview`" --mode editor --frames 1" 1
    Require ($log.Contains('Unsupported preview scene')) 'Unknown preview silently opened another scene'
    Pass 'unknown preview rejected'
}
Write-Host "$script:passed runtime checks passed. Logs: $work"

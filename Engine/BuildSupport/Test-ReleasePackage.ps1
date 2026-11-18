param([Parameter(Mandatory=$true)][string]$Package,
      [Parameter(Mandatory=$true)][string]$Output,
      [switch]$SmokeOnly)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$Package = [IO.Path]::GetFullPath($Package)
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw 'Use a new validation output directory' }
New-Item -ItemType Directory -Path $Output | Out-Null
. "$PSScriptRoot/Toolchain.ps1"
$python = (Get-Command python -ErrorAction Stop).Source
$native = Resolve-EngineToolchain
$runtime = Join-Path $Package 'Runtime'
$launcher = Join-Path $Package 'EngineLauncher.exe'
$results = [Collections.Generic.List[object]]::new()
$previousStore = $env:PAINENGINE_ENGINE_STORE
$previousPath = $env:PATH
$previousEngineContent = $env:ENGINE_CONTENT_ROOT
$previousGameContent = $env:GAME_CONTENT_ROOT
$previousQt = $env:QT_PLUGIN_PATH
function Check([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Run([string]$Name, [string]$Executable, [string[]]$Arguments, [string]$ExpectedText = '') {
    $result = Invoke-ToolchainProcess $Executable $Arguments -Timeout 120000
    [IO.File]::WriteAllText((Join-Path $Output "$Name.log"), $result.stdout + $result.stderr)
    Check ($result.exitCode -eq 0) "$Name failed ($($result.exitCode)); see $Output/$Name.log"
    if ($ExpectedText) { Check (($result.stdout + $result.stderr) -match $ExpectedText) "$Name missing '$ExpectedText'" }
    $results.Add(@{name=$Name;status='passed';exitCode=$result.exitCode;log="$Name.log"})
    Write-Output "PASS $Name"
}
Push-Location $Output
try {
    & $python "$PSScriptRoot/Test-ReleaseFiles.py" --package $Package --dumpbin "$($native.bin)/dumpbin.exe" --output "$Output/files-validation.json"
    Check ($LASTEXITCODE -eq 0) 'Packaged files/dependencies check failed'
    $results.Add(@{name='files-and-dependencies';status='passed';log='files-validation.json'})
    $env:PAINENGINE_ENGINE_STORE = Join-Path $Output 'EngineStore'
    $env:ENGINE_CONTENT_ROOT = $null
    $env:GAME_CONTENT_ROOT = $null
    $env:QT_PLUGIN_PATH = $null
    # Keep only OS binaries and the independently installed build tools. No developer Runtime DLLs.
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0;" +
        "$repo/Engine/build/tools/python/cmake/data/bin;$repo/Engine/build/tools/python/bin"
    Check ((Get-Content "$runtime/EngineVersion.json" -Raw | ConvertFrom-Json).version -eq '0.4.0') 'Runtime version mismatch'
    Check ((Get-Content "$runtime/SDK/Sdk.json" -Raw | ConvertFrom-Json).version -eq '0.4.0') 'SDK version mismatch'
    foreach ($file in @('EngineRuntime.exe','Editor.dll','GameExecutable.exe','EngineCommandlets.exe','Jolt.dll','assimp-vc143-mt.dll','platforms/qwindows.dll','platforms/qoffscreen.dll','SDK/lib/Engine.lib')) {
        Check (Test-Path -LiteralPath (Join-Path $runtime $file)) "Missing $file"
    }
    foreach ($game in @('BasicExamples','Pong','SunGame','Battle','Katamari','RuntimeExample')) {
        Run "game-$game" $launcher @('--smoke',$game)
    }
    foreach ($game in @('BasicExamples','Pong','SunGame','Battle','RuntimeExample')) {
        Run "editor-$game" $launcher @('--smoke-editor',$game)
    }
    foreach ($sample in @('AiTutorial','RepairCorridor')) {
        $manifest = Join-Path $Package "Tutorials/$sample/Project.json"
        Run "sample-$sample-game" "$runtime/EngineRuntime.exe" @('--project',$manifest,'--mode','game','--frames','60') 'GameUpdates=60'
        Run "sample-$sample-editor" $launcher @('--smoke-project-editor',$manifest)
    }
    $project = Join-Path $Output 'Новый проект Ёж'
    Run 'create-project' $launcher @('--create-project',$project,'PackagedTest')
    Run 'project-game' $launcher @('--smoke-project',"$project/Project.json")
    Run 'project-editor' $launcher @('--smoke-project-editor',"$project/Project.json")
    if (!$SmokeOnly) {
        Run 'cpp-enable' 'powershell.exe' @('-NoProfile','-ExecutionPolicy','Bypass','-File',"$runtime/SDK/ProjectTools.ps1",'-Action','Enable','-Project',"$project/Project.json",'-Runtime',$runtime) 'Build validated and activated'
        Run 'cpp-runtime' "$runtime/EngineRuntime.exe" @('--project',"$project/Project.json",'--mode','game','--frames','60') 'GameUpdates=60'
        $export = Join-Path $Output 'Экспорт игры Ёж'
        Run 'cpp-export' "$runtime/EngineRuntime.exe" @('--export-project',"$project/Project.json",'--destination',$export) 'Game export validated'
        Run 'exported-exe' "$export/Game.exe" @('--frames','60') 'GameUpdates=60'
        $request = Join-Path $Output 'input.request.json'
        @{project="$project/Project.json";steps=@(@{kind='control';action='play'},@{kind='frames';count=60},@{kind='control';action='stop'})} |
            ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $request -Encoding utf8
        Run 'commandlet-play' "$runtime/EngineCommandlets.exe" @('project.input',$request,(Join-Path $Output 'input.result.json'))
    }
    $report = @{version='0.4.0';status='passed';checks=$results;scope=if($SmokeOnly){'extracted archive smoke'}else{'staged distribution + SDK + export'};limitations=@('Manual panel interactions remained partial in the earlier playtest; see release notes.','Linux, macOS and clang-cl were not tested for this archive.')}
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Output 'VALIDATION.json') -Encoding utf8
    if (!$SmokeOnly) { Copy-Item -LiteralPath (Join-Path $Output 'VALIDATION.json') -Destination (Join-Path $Package 'VALIDATION.json') }
    Write-Output "Validated $($results.Count) package checks"
} finally {
    Pop-Location
    $env:PAINENGINE_ENGINE_STORE=$previousStore
    $env:PATH=$previousPath
    $env:ENGINE_CONTENT_ROOT=$previousEngineContent
    $env:GAME_CONTENT_ROOT=$previousGameContent
    $env:QT_PLUGIN_PATH=$previousQt
}

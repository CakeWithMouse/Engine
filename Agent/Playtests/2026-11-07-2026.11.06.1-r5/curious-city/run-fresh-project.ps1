$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path 'E:/Engine').Path
$report = Join-Path $repo 'Agent/Playtests/2026-11-07-2026.11.06.1-r5/curious-city'
$output = Join-Path $repo 'Engine/build/playtests/20261107-r5/curious-city'
$evidence = Join-Path $report 'evidence'
$launcher = Join-Path $repo 'Game/build/x64/Release/EngineLauncher.exe'
$project = Join-Path $output 'fresh-city'
$env:PAINENGINE_ENGINE_STORE = Join-Path $output 'EngineStore'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null

function Invoke-CityCli([string]$name, [string[]]$arguments) {
    $stdout = Join-Path $evidence "$name.stdout.log"
    $stderr = Join-Path $evidence "$name.stderr.log"
    $proc = Start-Process -FilePath $launcher -ArgumentList $arguments -PassThru -Wait -NoNewWindow `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    [IO.File]::WriteAllText((Join-Path $evidence "$name.exitcode.txt"), [string]$proc.ExitCode)
    [pscustomobject]@{ step = $name; args = $arguments; exitCode = $proc.ExitCode; stdout = $stdout; stderr = $stderr }
}

$results = [Collections.Generic.List[object]]::new()
$results.Add((Invoke-CityCli 'fresh-create' @('--create-project', $project, 'CuriousCityR5')))
if ($results[0].exitCode -ne 0) { throw "create project failed ($($results[0].exitCode))" }
$results.Add((Invoke-CityCli 'fresh-enable' @('--enable-code', (Join-Path $project 'Project.json'))))
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $evidence 'create-enable.json')
if ($results[1].exitCode -ne 0) { throw "enable code failed ($($results[1].exitCode)); inspect CodeBuild.log" }

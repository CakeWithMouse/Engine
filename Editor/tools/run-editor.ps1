param(
    [switch]$NoLaunch,
    [switch]$Fast,
    [switch]$All
)
$ErrorActionPreference = 'Stop'

if (!$All) {
    $Fast = $true
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))

function Log-Step([string]$msg, [string]$color = "Cyan") {
    $t = (Get-Date).ToString("HH:mm:ss")
    Write-Host "[$t] $msg" -ForegroundColor $color
}

Log-Step "=== 1. Terminating running processes ===" "Cyan"
$processNames = @("EngineLauncher", "EngineRuntime", "Editor", "EditorUiTests", "UiTests", "HostTests", "CoreTests", "EngineTests")
$running = Get-Process -Name $processNames -ErrorAction SilentlyContinue
if ($running) {
    Log-Step "Stopping processes: $($running.ProcessName -join ', ')" "Yellow"
    $running | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

Log-Step "=== 2. Reading engine version ===" "Cyan"
$versionJsonPath = Join-Path $repoRoot "Engine/Version.json"
if (!(Test-Path -LiteralPath $versionJsonPath)) { throw "Version.json not found: $versionJsonPath" }
$version = (Get-Content -LiteralPath $versionJsonPath -Raw | ConvertFrom-Json).version
Log-Step "Current engine version: $version" "Gray"

Log-Step "=== 3. Clearing engine version cache in LocalAppData ===" "Cyan"
$enginesDir = Join-Path $env:LOCALAPPDATA "PainEngine\Engines"
if (Test-Path -LiteralPath $enginesDir) {
    $cachePaths = @(
        (Join-Path $enginesDir $version),
        (Join-Path $enginesDir "2026.10.31.3")
    )
    foreach ($p in $cachePaths) {
        if (Test-Path -LiteralPath $p) {
            Log-Step "Removing engine cache: $p" "DarkGray"
            Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

Log-Step "=== 4. Building Engine + Editor (Debug) [Mode: $(if ($Fast) {'Fast'} else {'All'})] ===" "Cyan"
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild.exe not found.' }

$solutionPath = Join-Path $repoRoot "Engine/Engine.sln"
$buildArgs = @(
    "`"$solutionPath`"",
    "/nologo",
    "/m",
    "/nr:false",
    "/p:Configuration=Debug",
    "/p:Platform=x64",
    "/p:ForceSynchronousPDBWrites=true",
    "/p:PreferredToolArchitecture=x64",
    "/verbosity:minimal"
)

if ($Fast) {
    $fastProjects = @("Engine", "EngineRuntime", "SceneProject", "EditorModule", "EngineLauncher")
    
    # Check if untracked/modified files exist in standalone games or engine tests
    $gameDirs = @("Game/Pong", "Game/Battle", "Game/SunGame", "Game/BasicExamples", "Game/Katamari", "Engine/tests")
    $gitStatus = & git.exe -C $repoRoot status --porcelain -- $gameDirs 2>$null
    if ($gitStatus) {
        Log-Step "Changes detected in Games/Tests; building full solution..." "Yellow"
    } else {
        Log-Step "Fast mode: building target projects ($($fastProjects -join ', '))..." "Green"
        $buildArgs += "/t:$($fastProjects -join ';')"
    }
} else {
    Log-Step "All mode: building entire solution..." "Yellow"
}

Log-Step "Invoking MSBuild on $solutionPath..." "Gray"

# Launch MSBuild with live streaming and 15s heartbeat during long operations / linking
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $msbuild
$psi.Arguments = $buildArgs -join ' '
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::new()
$proc.StartInfo = $psi

if (!$proc.Start()) {
    throw "Failed to start MSBuild"
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastHeartbeat = [System.Diagnostics.Stopwatch]::StartNew()
$currentStage = "Инициализация MSBuild..."

while (!$proc.HasExited) {
    while (!$proc.StandardOutput.EndOfStream) {
        $line = $proc.StandardOutput.ReadLine()
        if ($line) {
            $t = (Get-Date).ToString("HH:mm:ss")
            if ($line -match "->\s*(.+\.(dll|lib|exe))") {
                Write-Host "[$t] [Готово] $($Matches[1])" -ForegroundColor Green
                $lastHeartbeat.Restart()
            } elseif ($line -match "^\s*([A-Za-z0-9_]+)\.vcxproj\s*->") {
                Write-Host "[$t] [Проект] $line" -ForegroundColor Yellow
                $lastHeartbeat.Restart()
            } elseif ($line -match "windeployqt:") {
                Write-Host "[$t] [Деплой] $line" -ForegroundColor DarkCyan
                $lastHeartbeat.Restart()
            } elseif ($line -match "(ClCompile|Link|Lib):") {
                $currentStage = $line.Trim()
                Write-Host "[$t] [Этап] $currentStage" -ForegroundColor Gray
                $lastHeartbeat.Restart()
            } elseif ($line -match "(error|warning)\s*([A-Z0-9]+)?:") {
                $isErr = $line -match "error"
                Write-Host "[$t] $line" -ForegroundColor $(if ($isErr) { "Red" } else { "Yellow" })
            }
        }
    }
    Start-Sleep -Milliseconds 250
    if ($lastHeartbeat.Elapsed.TotalSeconds -ge 15) {
        $elapsedSec = [math]::Round($sw.Elapsed.TotalSeconds)
        $t = (Get-Date).ToString("HH:mm:ss")
        Write-Host "[$t] ... ещё идёт: $elapsedSec сек (текущий этап: $currentStage)" -ForegroundColor DarkYellow
        $lastHeartbeat.Restart()
    }
}
$proc.WaitForExit()

$stdErr = $proc.StandardError.ReadToEnd()
if ($stdErr) {
    Write-Host $stdErr -ForegroundColor Red
}

if ($proc.ExitCode -ne 0) {
    throw "Build failed with exit code $($proc.ExitCode)"
}

$buildTimeSec = [math]::Round($sw.Elapsed.TotalSeconds, 2)
Log-Step "Build finished successfully in $buildTimeSec сек." "Green"

Log-Step "=== 5. Ensuring EditorOverlay.hlsl is deployed ===" "Cyan"
$sourceShader = Join-Path $repoRoot "Engine/Content/Shaders/EditorOverlay.hlsl"
$sourceFxaa = Join-Path $repoRoot "Engine/Content/Shaders/FXAA.hlsl"
$targetShaders = @(
    (Join-Path $repoRoot "Game/build/x64/Debug/Runtime/EngineContent/Shaders/EditorOverlay.hlsl")
)
foreach ($target in $targetShaders) {
    $dir = Split-Path -Parent $target
    if (!(Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Copy-Item -LiteralPath $sourceShader -Destination $target -Force
    Log-Step "Deployed shader: $target" "DarkGray"
}
$targetFxaa = Join-Path $repoRoot "Game/build/x64/Debug/Runtime/EngineContent/Shaders/FXAA.hlsl"
if (Test-Path -LiteralPath $sourceFxaa) {
    Copy-Item -LiteralPath $sourceFxaa -Destination $targetFxaa -Force
    Log-Step "Deployed shader: $targetFxaa" "DarkGray"
}

Log-Step "=== 5b. Synchronizing Game.dll for user projects in Documents ===" "Cyan"
$templateDll = Join-Path $repoRoot "Game/build/x64/Debug/Runtime/Templates/SceneProject/Game.dll"
if (Test-Path -LiteralPath $templateDll) {
    $templateItem = Get-Item -LiteralPath $templateDll
    $docsDir = [Environment]::GetFolderPath('MyDocuments')
    $projectFiles = @()
    $registryJson = Join-Path $env:LOCALAPPDATA "EngineLauncher\projects.json"
    if (Test-Path -LiteralPath $registryJson) {
        try {
            $regProjects = Get-Content -LiteralPath $registryJson -Raw | ConvertFrom-Json
            foreach ($p in $regProjects) { if (Test-Path -LiteralPath $p) { $projectFiles += $p } }
        } catch {}
    }
    if (Test-Path -LiteralPath $docsDir) {
        $found = Get-ChildItem -LiteralPath $docsDir -Filter "Project.json" -Recurse -Depth 2 -ErrorAction SilentlyContinue
        foreach ($f in $found) { $projectFiles += $f.FullName }
    }
    $projectFiles = $projectFiles | Select-Object -Unique
    foreach ($pJson in $projectFiles) {
        $pDir = Split-Path -Parent $pJson
        $pDll = Join-Path $pDir "Game.dll"
        if (Test-Path -LiteralPath $pDll) {
            $dllItem = Get-Item -LiteralPath $pDll
            if ($dllItem.LastWriteTime -lt $templateItem.LastWriteTime) {
                Copy-Item -LiteralPath $templateDll -Destination $pDll -Force
                Log-Step "Updated outdated Game.dll in $pDir ($($dllItem.LastWriteTime) -> $($templateItem.LastWriteTime))" "Yellow"
            }
        }
    }
}

Log-Step "=== 6. Launching EngineLauncher ===" "Cyan"
$launcherPath = Join-Path $repoRoot "Game/build/x64/Debug/EngineLauncher.exe"
if (!(Test-Path -LiteralPath $launcherPath)) { throw "Launcher not found: $launcherPath" }

if (!$NoLaunch) {
    Log-Step "Starting: $launcherPath" "Green"
    Start-Process -FilePath $launcherPath -WorkingDirectory (Join-Path $repoRoot "Game/build/x64/Debug")
}
Log-Step "=== Completed successfully ===" "Green"

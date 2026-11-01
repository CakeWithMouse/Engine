param(
    [switch]$NoLaunch
)
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))

Write-Host "=== 1. Terminating running processes ===" -ForegroundColor Cyan
$processNames = @("EngineLauncher", "EngineRuntime", "Editor", "EditorUiTests", "UiTests", "HostTests", "CoreTests", "EngineTests")
$running = Get-Process -Name $processNames -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Stopping processes: $($running.ProcessName -join ', ')"
    $running | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

Write-Host "=== 2. Reading engine version ===" -ForegroundColor Cyan
$versionJsonPath = Join-Path $repoRoot "Engine/Version.json"
if (!(Test-Path -LiteralPath $versionJsonPath)) { throw "Version.json not found: $versionJsonPath" }
$version = (Get-Content -LiteralPath $versionJsonPath -Raw | ConvertFrom-Json).version
Write-Host "Current engine version: $version"

Write-Host "=== 3. Clearing engine version cache in LocalAppData ===" -ForegroundColor Cyan
$enginesDir = Join-Path $env:LOCALAPPDATA "PainEngine\Engines"
if (Test-Path -LiteralPath $enginesDir) {
    # Remove version cache for current version and any stale experimental versions
    $cachePaths = @(
        (Join-Path $enginesDir $version),
        (Join-Path $enginesDir "2026.10.31.3")
    )
    foreach ($p in $cachePaths) {
        if (Test-Path -LiteralPath $p) {
            Write-Host "Removing engine cache: $p"
            Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

Write-Host "=== 4. Building Engine + Editor (Debug) ===" -ForegroundColor Cyan
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

Write-Host "Invoking MSBuild on $solutionPath..."
& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "=== 5. Ensuring EditorOverlay.hlsl is deployed ===" -ForegroundColor Cyan
$sourceShader = Join-Path $repoRoot "Engine/Content/Shaders/EditorOverlay.hlsl"
$sourceFxaa = Join-Path $repoRoot "Engine/Content/Shaders/FXAA.hlsl"
$targetShaders = @(
    (Join-Path $repoRoot "Game/build/x64/Debug/Runtime/EngineContent/Shaders/EditorOverlay.hlsl")
)
foreach ($target in $targetShaders) {
    $dir = Split-Path -Parent $target
    if (!(Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Copy-Item -LiteralPath $sourceShader -Destination $target -Force
    Write-Host "Deployed shader: $target"
}
$targetFxaa = Join-Path $repoRoot "Game/build/x64/Debug/Runtime/EngineContent/Shaders/FXAA.hlsl"
if (Test-Path -LiteralPath $sourceFxaa) {
    Copy-Item -LiteralPath $sourceFxaa -Destination $targetFxaa -Force
    Write-Host "Deployed shader: $targetFxaa"
}

Write-Host "=== 5b. Synchronizing Game.dll for user projects in Documents ===" -ForegroundColor Cyan
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
                Write-Host "Updated outdated Game.dll in $pDir ($($dllItem.LastWriteTime) -> $($templateItem.LastWriteTime))" -ForegroundColor Yellow
            }
        }
    }
}

Write-Host "=== 6. Launching EngineLauncher ===" -ForegroundColor Cyan
$launcherPath = Join-Path $repoRoot "Game/build/x64/Debug/EngineLauncher.exe"
if (!(Test-Path -LiteralPath $launcherPath)) { throw "Launcher not found: $launcherPath" }

if (!$NoLaunch) {
    Write-Host "Starting: $launcherPath" -ForegroundColor Green
    Start-Process -FilePath $launcherPath -WorkingDirectory (Join-Path $repoRoot "Game/build/x64/Debug")
}
Write-Host "=== Completed successfully ===" -ForegroundColor Green

param(
    [switch]$Quick,
    [switch]$Full
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

if (!$Quick -and !$Full) {
    $Quick = $true
}

Write-Output "=== PainEngine Editor check.ps1 [Mode: $(if ($Full) {'Full'} else {'Quick'})] ==="

# 1. Check forbidden zones (Engine/, Game/, build.ps1)
Write-Output "--- 1. Checking forbidden zones ---"
$diffForbidden = git -C $repo diff --stat HEAD -- Engine/ Game/ build.ps1
if ($diffForbidden) {
    Write-Error "Forbidden zone modified!`n$diffForbidden"
    exit 1
}
Write-Output "OK: Forbidden zones clean."

# 2. Check architecture layers
Write-Output "--- 2. Checking architectural layers (Core/UI/Host) ---"
$coreDir = Join-Path $repo "Editor/Core"
if (Test-Path $coreDir) {
    $forbiddenTypes = @("QWidget", "QMessageBox", "QFileDialog", "QColorDialog", "QAction", "QMenu", "QDialog")
    foreach ($type in $forbiddenTypes) {
        $matches = Get-ChildItem -Path $coreDir -Recurse -Include *.h, *.cpp | Select-String -Pattern "\b$type\b"
        if ($matches) {
            Write-Error "Core layer violation: forbidden UI type '$type' in:`n$($matches | Out-String)"
            exit 1
        }
    }
    # Check includes: Core must not include UI or Host
    $badIncludes = Get-ChildItem -Path $coreDir -Recurse -Include *.h, *.cpp | Select-String -Pattern '#include\s*[<"](UI/|Host/|MainWindow|ViewportWidget)'
    if ($badIncludes) {
        Write-Error "Core layer violation: Core includes UI or Host headers:`n$($badIncludes | Out-String)"
        exit 1
    }
}

$hostDir = Join-Path $repo "Editor/Host"
if (Test-Path $hostDir) {
    # Host must not include UI
    $badHostIncludes = Get-ChildItem -Path $hostDir -Recurse -Include *.h, *.cpp | Select-String -Pattern '#include\s*[<"](UI/|MainWindow|ViewportWidget|SceneObjectsPanel|ObjectInspectorPanel)'
    if ($badHostIncludes) {
        Write-Error "Host layer violation: Host includes UI headers:`n$($badHostIncludes | Out-String)"
        exit 1
    }
}
Write-Output "OK: Architectural layers check passed."

# 3. Check line count (wc -l <= 500)
Write-Output "--- 3. Checking line counts of modified files ---"
$changedFiles = git -C $repo diff --name-only HEAD -- Editor/
foreach ($f in ($changedFiles -split "`r?`n")) {
    if (!$f -or !(Test-Path (Join-Path $repo $f))) { continue }
    $ext = [IO.Path]::GetExtension($f).ToLower()
    if ($ext -in @('.h', '.hpp', '.cpp', '.cxx')) {
        $lineCount = (Get-Content (Join-Path $repo $f) | Measure-Object -Line).Lines
        if ($lineCount -gt 500) {
            Write-Warning "File $f exceeds 500 lines ($lineCount lines)."
        } else {
            Write-Output "  $f : $lineCount lines"
        }
    }
}

# 4. Find MSBuild via vswhere
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild.exe not found' }

# 5. Build
Write-Output "--- 4. Building Editor ---"
$editorProj = Join-Path $repo "Editor/EditorModule.vcxproj"

Write-Output "Building Debug..."
& $msbuild $editorProj /nologo /verbosity:minimal /m /p:ForceSynchronousPDBWrites=true /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "Editor Debug build failed with exit code $LASTEXITCODE" }

if ($Full) {
    Write-Output "Building Release..."
    & $msbuild $editorProj /nologo /verbosity:minimal /m /p:ForceSynchronousPDBWrites=true /p:Configuration=Release /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "Editor Release build failed with exit code $LASTEXITCODE" }
}
Write-Output "OK: Build succeeded."

# 6. Tests
Write-Output "--- 5. Running tests ---"
if ($Quick) {
    # In Quick mode, run Core unit tests if present
    $coreTestsProj = Join-Path $repo "Editor/tests/CoreTests.vcxproj"
    if (Test-Path $coreTestsProj) {
        Write-Output "Building and running CoreTests..."
        & $msbuild $coreTestsProj /nologo /verbosity:minimal /m /p:ForceSynchronousPDBWrites=true /p:Configuration=Debug /p:Platform=x64
        if ($LASTEXITCODE -ne 0) { throw "CoreTests build failed with exit code $LASTEXITCODE" }
        $coreTestsExe = Join-Path $repo "Game/build/x64/Debug/Runtime/CoreTests.exe"
        & $coreTestsExe
        if ($LASTEXITCODE -ne 0) { throw "CoreTests failed with exit code $LASTEXITCODE" }
        Write-Output "OK: CoreTests passed."
    } else {
        Write-Output "CoreTests.vcxproj not present yet; skipping Core unit tests."
    }
}

if ($Full) {
    Write-Output "Running all baseline tests (-Full)..."
    
    # Engine tests (43/43)
    Write-Output "Engine/tests/run.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/run.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "Engine/tests/run.ps1 failed" }

    # Component generation (20/20)
    Write-Output "Engine/tests/component-generation/run.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/component-generation/run.ps1")
    if ($LASTEXITCODE -ne 0) { throw "component-generation/run.ps1 failed" }

    # Edit session (609/609)
    Write-Output "Engine/tests/edit-session/run.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/edit-session/run.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "edit-session/run.ps1 failed" }

    # Scene module (109/109)
    Write-Output "Engine/tests/scene-module/run.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/scene-module/run.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "scene-module/run.ps1 failed" }

    # Viewport tests
    Write-Output "Engine/tests/run-viewport.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/run-viewport.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "run-viewport.ps1 failed" }

    # Skeletal (8/8)
    Write-Output "Engine/tests/skeletal/run.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Engine/tests/skeletal/run.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "skeletal/run.ps1 failed" }

    # Game checks (27/27)
    Write-Output "Game/tests/run-games.ps1..."
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repo "Game/tests/run-games.ps1") -Configuration Debug
    if ($LASTEXITCODE -ne 0) { throw "run-games.ps1 failed" }

    Write-Output "OK: All baseline tests passed."
}

Write-Output "=== check.ps1: SUCCESS ==="

param(
    [switch]$Quick,
    [switch]$Full,
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

if (!$Quick -and !$Full) {
    $Quick = $true
}

Write-Output "=== PainEngine Editor check.ps1 [Mode: $(if ($Full) {'Full'} else {'Quick'})] ==="

# Helper for git commands preventing PS 5.1 NativeCommandError on stderr warnings
function Invoke-Git {
    param([string[]]$Arguments)
    $prev = $global:ErrorActionPreference
    $global:ErrorActionPreference = 'Continue'
    try {
        $result = & git.exe -c core.safecrlf=false -C $repo @Arguments 2>$null
        return $result
    }
    finally {
        $global:ErrorActionPreference = $prev
    }
}

# 0. Check and kill hanging processes from previous runs
function Stop-HangingProcesses {
    $names = @("EngineLauncher", "Editor", "EditorUiTests", "CoreTests")
    $hanging = Get-Process -Name $names -ErrorAction SilentlyContinue
    if ($hanging) {
        Write-Warning "Found hanging processes from previous run: $($hanging.ProcessName -join ', '). Terminating..."
        $hanging | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
}
Stop-HangingProcesses

# 1. Read engine version dynamically from Engine/Version.json
$versionJsonPath = Join-Path $repo 'Engine/Version.json'
if (!(Test-Path -LiteralPath $versionJsonPath)) { throw "Version.json not found at $versionJsonPath" }
$engineVersion = (Get-Content -LiteralPath $versionJsonPath -Raw | ConvertFrom-Json).version
if (!$engineVersion) { throw "Failed to parse engine version from $versionJsonPath" }
Write-Output "Engine version: $engineVersion"

# 2. Clear engine release cache for this version before tests start
function Clear-EngineCache {
    $cacheDir = Join-Path $env:LOCALAPPDATA ("PainEngine\Engines\" + $engineVersion)
    if (Test-Path -LiteralPath $cacheDir) {
        Write-Output "Clearing cached engine version directory: $cacheDir"
        Remove-Item -LiteralPath $cacheDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
Clear-EngineCache

# Helper for process execution with timeout and process-tree termination
function Invoke-ProcessWithTimeout {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [int]$TimeoutSeconds = 120,
        [string]$Description = "Process"
    )
    Write-Output "Running $Description (Timeout: ${TimeoutSeconds}s)..."
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $FilePath
    $psi.Arguments = $ArgumentList -join ' '
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $repo
    $proc = [System.Diagnostics.Process]::new()
    $proc.StartInfo = $psi
    
    if (!$proc.Start()) {
        throw "Failed to start $FilePath"
    }

    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    
    if (!$proc.WaitForExit($TimeoutSeconds * 1000)) {
        Write-Warning "TIMEOUT: $Description exceeded ${TimeoutSeconds}s. Killing process tree..."
        & taskkill.exe /F /T /PID $proc.Id *>$null
        $outText = if ($stdoutTask.IsCompleted) { $stdoutTask.GetAwaiter().GetResult() } else { "" }
        $errText = if ($stderrTask.IsCompleted) { $stderrTask.GetAwaiter().GetResult() } else { "" }
        $tail = ($outText + "`n" + $errText).Split("`n") | Select-Object -Last 20
        Write-Error "FAIL (timeout): $Description timed out after ${TimeoutSeconds}s.`nOutput tail:`n$($tail -join "`n")"
        throw "Timeout: $Description exceeded ${TimeoutSeconds}s"
    }
    
    $proc.WaitForExit()
    $outText = $stdoutTask.GetAwaiter().GetResult().Trim()
    $errText = $stderrTask.GetAwaiter().GetResult().Trim()
    if ($outText) { Write-Output $outText }
    if ($errText) { Write-Output $errText }
    
    if ($proc.ExitCode -ne 0) {
        throw "$Description failed with exit code $($proc.ExitCode)"
    }
}

# 3. Check forbidden zones (Engine/, Game/, build.ps1)
Write-Output "--- 1. Checking forbidden zones ---"
$diffForbidden = Invoke-Git @("diff", "--stat", "HEAD", "--", "Engine/", "Game/", "build.ps1")
if ($diffForbidden) {
    Write-Error "Forbidden zone modified!`n$diffForbidden"
    exit 1
}
Write-Output "OK: Forbidden zones clean."

# 4. Check architecture layers
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

# 5. Check line count (wc -l <= 500)
Write-Output "--- 3. Checking line counts of modified files ---"
$diffFiles = Invoke-Git @("diff", "--name-only", "HEAD", "--", "Editor/")
$untrackedFiles = Invoke-Git @("ls-files", "--others", "--exclude-standard", "--", "Editor/")
$allCandidateFiles = @($diffFiles -split "`r?`n") + @($untrackedFiles -split "`r?`n") | Select-Object -Unique
$hasOversizedFiles = $false
foreach ($f in $allCandidateFiles) {
    if (!$f -or !(Test-Path (Join-Path $repo $f))) { continue }
    $ext = [IO.Path]::GetExtension($f).ToLower()
    if ($ext -in @('.h', '.hpp', '.cpp', '.cxx')) {
        $lineCount = (Get-Content (Join-Path $repo $f) | Measure-Object -Line).Lines
        if ($lineCount -gt 500) {
            Write-Error "FAIL: File $f exceeds 500 lines ($lineCount lines > 500)."
            $hasOversizedFiles = $true
        } else {
            Write-Output "  $f : $lineCount lines"
        }
    }
}
if ($hasOversizedFiles) {
    throw "Line count check failed: files exceed 500 lines."
}

# 6. Find MSBuild via vswhere
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild.exe not found' }

# 7. Build Editor
Write-Output "--- 4. Building Editor ---"
$editorProj = Join-Path $repo "Editor/EditorModule.vcxproj"

Invoke-ProcessWithTimeout -FilePath $msbuild -ArgumentList @('"' + $editorProj + '"', '/nologo', '/verbosity:minimal', '/m', '/p:ForceSynchronousPDBWrites=true', '/p:Configuration=Debug', '/p:Platform=x64') -TimeoutSeconds $TimeoutSec -Description "Editor Debug Build"

if ($Full) {
    Invoke-ProcessWithTimeout -FilePath $msbuild -ArgumentList @('"' + $editorProj + '"', '/nologo', '/verbosity:minimal', '/m', '/p:ForceSynchronousPDBWrites=true', '/p:Configuration=Release', '/p:Platform=x64') -TimeoutSeconds $TimeoutSec -Description "Editor Release Build"
}
Write-Output "OK: Build succeeded."

# 8. Tests
Write-Output "--- 5. Running tests ---"
# Run Core unit tests if present
$coreTestsProj = Join-Path $repo "Editor/tests/CoreTests.vcxproj"
if (Test-Path $coreTestsProj) {
    Invoke-ProcessWithTimeout -FilePath $msbuild -ArgumentList @('"' + $coreTestsProj + '"', '/nologo', '/verbosity:minimal', '/m', '/p:ForceSynchronousPDBWrites=true', '/p:Configuration=Debug', '/p:Platform=x64') -TimeoutSeconds $TimeoutSec -Description "CoreTests Build"

    $coreTestsExe = Join-Path $repo "Game/build/x64/Debug/Runtime/CoreTests.exe"
    Invoke-ProcessWithTimeout -FilePath $coreTestsExe -TimeoutSeconds $TimeoutSec -Description "CoreTests Execution"
    Write-Output "OK: CoreTests passed."
} else {
    Write-Output "CoreTests.vcxproj not present yet; skipping Core unit tests."
}

if ($Full) {
    Write-Output "Running all baseline tests (-Full)..."
    
    # Engine tests (43/43)
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/run.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "Engine/tests/run.ps1"

    # Component generation (20/20)
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/component-generation/run.ps1") + '"')) -TimeoutSeconds $TimeoutSec -Description "component-generation/run.ps1"

    # Edit session (609/609)
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/edit-session/run.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "edit-session/run.ps1"

    # Scene module (109/109)
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/scene-module/run.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "scene-module/run.ps1"

    # Viewport tests
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/run-viewport.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "run-viewport.ps1"

    # Skeletal (8/8)
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Engine/tests/skeletal/run.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "skeletal/run.ps1"

    # Game checks (27/27)
    Clear-EngineCache
    Stop-HangingProcesses
    Invoke-ProcessWithTimeout -FilePath "powershell.exe" -ArgumentList @('-ExecutionPolicy', 'Bypass', '-File', ('"' + (Join-Path $repo "Game/tests/run-games.ps1") + '"'), '-Configuration', 'Debug') -TimeoutSeconds $TimeoutSec -Description "run-games.ps1"

    Write-Output "OK: All baseline tests passed."
}

Write-Output "=== check.ps1: SUCCESS ==="

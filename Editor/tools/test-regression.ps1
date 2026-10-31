param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [switch]$Full,
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $repo ('Engine/build/editor-regression/' + $Configuration + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$runtime = Join-Path $repo "Game/build/x64/$Configuration/Runtime"
$build = Join-Path $repo 'Engine/BuildSupport/Invoke-MSBuild.ps1'

# Only the process started by this runner is terminated on timeout. Existing
# editors, projects and the user's installed-engine cache are left alone.
function Run-Test([string]$Name, [string]$Executable, [string[]]$Arguments = @()) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    # Our paths have no embedded quotes or trailing slashes.
    $info.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (!$process.Start()) { throw "Cannot start $Name" }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        & taskkill.exe /F /T /PID $process.Id *> $null
        throw "$Name timed out; results: $output"
    }
    $text = $stdout.GetAwaiter().GetResult() + "`n" + $stderr.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $output "$Name.log"), $text)
    if ($process.ExitCode -ne 0) { throw "$Name failed ($($process.ExitCode)); see $output/$Name.log" }
    Write-Output "PASS $Name"
}
function Build-Project([string]$Project, [string]$Name, [switch]$Ui) {
    & $build -Project (Join-Path $repo $Project) -Configuration $Configuration -EnableAssimp -EditorUiTest:$Ui *> (Join-Path $output "$Name-build.log")
}

$previousStore = $env:PAINENGINE_ENGINE_STORE
try {
    $env:PAINENGINE_ENGINE_STORE = Join-Path $output 'Engines'
    foreach ($name in @('CoreTests','HostTests','UiTests')) {
        if (!$SkipBuild) { Build-Project "Editor/tests/$name.vcxproj" $name }
        Run-Test $name (Join-Path $runtime "$name.exe")
    }
    if ($Full) {
        if (!$SkipBuild) {
            Build-Project 'Game/Launcher/EngineLauncher.vcxproj' 'Runtime'
            Build-Project 'Editor/EditorModule.vcxproj' 'EditorUiTests' -Ui
            Build-Project 'Engine/tests/EngineTests.vcxproj' 'EngineTests'
            Build-Project 'Engine/tests/hud/HudTests.vcxproj' 'HudTests'
            Build-Project 'Engine/tests/input/EditorReloadTests.vcxproj' 'EditorReloadTests'
        }
        Run-Test 'EngineTests' (Join-Path $repo "Engine/build/x64/$Configuration/EngineTests.exe")
        Run-Test 'HudTests' (Join-Path $repo "Engine/build/x64/$Configuration/HudTests.exe") @((Join-Path $output 'Hud'), (Join-Path $repo 'Engine/Content'))
        Run-Test 'EditorReloadTests' (Join-Path $repo "Engine/build/x64/$Configuration/EditorReloadTests.exe") @($runtime, (Join-Path $output 'NativeReload'))
        $launcher = Join-Path $repo "Game/build/x64/$Configuration/EngineLauncher.exe"
        foreach ($mode in @('standard','authoring','workflow','layout','reload')) {
            $project = Join-Path $output $mode
            Run-Test "Create-$mode" $launcher @('--create-project', $project, 'Regression')
            $arguments = @($project, (Join-Path $runtime 'EngineContent'))
            if ($mode -ne 'standard') { $arguments += "--$mode" }
            Run-Test "UI-$mode" (Join-Path $runtime 'EditorUiTests.exe') $arguments
            if ($mode -eq 'workflow') {
                Run-Test 'UI-recovery' (Join-Path $runtime 'EditorUiTests.exe') @($project, (Join-Path $runtime 'EngineContent'), '--recovery-check')
            }
        }
    }
    Write-Output "Regression checks passed ($Configuration). Logs: $output"
}
finally { $env:PAINENGINE_ENGINE_STORE = $previousStore }

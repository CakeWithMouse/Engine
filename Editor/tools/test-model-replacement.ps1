param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$runtime = Join-Path $repo "Game/build/x64/$Configuration/Runtime"
$output = Join-Path $repo ('Engine/build/model-replacement/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$previousMode = $env:ModelReplaceTest
$previousStore = $env:PAINENGINE_ENGINE_STORE
try {
    $env:PAINENGINE_ENGINE_STORE = Join-Path $output 'Engines'
    if (!$SkipBuild) {
        $env:ModelReplaceTest = 'true'
        & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EnableAssimp -EditorUiTest *> "$output/build.log"
    }
    function Run([string]$Exe, [string[]]$Arguments, [string]$Log) {
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName = $Exe
        $info.WorkingDirectory = $repo
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
        $info.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $p = [Diagnostics.Process]::new(); $p.StartInfo = $info
        if (!$p.Start()) { throw 'Cannot start model replacement test' }
        $stdout = $p.StandardOutput.ReadToEndAsync(); $stderr = $p.StandardError.ReadToEndAsync()
        if (!$p.WaitForExit(120000)) {
            & taskkill.exe /F /T /PID $p.Id *> $null
            throw "Test timed out: $output"
        }
        [IO.File]::WriteAllText($Log, $stdout.GetAwaiter().GetResult() + "`n" + $stderr.GetAwaiter().GetResult())
        if ($p.ExitCode) { throw "Test failed: $Log" }
    }
    $project = Join-Path $output 'Project'
    Run "$repo/Game/build/x64/$Configuration/EngineLauncher.exe" @('--create-project', $project, 'LegacyModel') "$output/create.log"
    $fixture = 'Agent/Playtests/2026-11-05-2026.11.05.1-r2/curious-city/evidence/final-probes'
    Copy-Item -LiteralPath "$repo/$fixture/texture-final5-main.scene.json" -Destination "$project/Content/Scenes/Main.scene.json"
    New-Item -ItemType Directory -Path "$project/Content/Models/CesiumMan", "$project/Content/Images" -Force | Out-Null
    Copy-Item -LiteralPath "$repo/$fixture/CesiumMan-source.glb" -Destination "$project/Content/Models/CesiumMan/CesiumMan.glb"
    Copy-Item -LiteralPath "$repo/Engine/docs/tutorial/resources/pyramid.obj" -Destination "$project/Content/Models/pyramid.obj"
    # The archived scene references a texture not archived with it. Supply the
    # repository checker at that path; the scene itself stays byte-identical.
    Copy-Item -LiteralPath "$repo/Agent/Playtests/2026-11-04-2026.11.01.5-r1/reports/evidence/calm-city/AuthoringLab/Content/Textures/checker.bmp" -Destination "$project/Content/Images/Renamed.bmp"
    Run "$runtime/ModelReplaceUiTests.exe" @($project, "$runtime/EngineContent") "$output/test.log"
    Write-Output "PASS real legacy model replacement. Logs: $output"
} finally {
    $env:ModelReplaceTest = $previousMode
    $env:PAINENGINE_ENGINE_STORE = $previousStore
}

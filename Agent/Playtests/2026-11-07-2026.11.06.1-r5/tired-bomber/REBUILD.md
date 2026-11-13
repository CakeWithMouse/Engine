# Rebuild Tired Bomber R5

Tested release: PainEngine 2026.11.07.0 Release, HEAD `55d3b0ceea1723c88ffd51d701787f87ccbb6fc4`.

`tired-bomber-r5-source.zip` contains `Source/`, `Content/Models/`, `Content/Scenes/`, `Project.json`, `ProjectTypes.tsv`, `Engine.lock.json`, `Build.ps1`, `Game.vcxproj`, and `Game.sln`. It excludes generated DLLs, Intermediate, runtime frame dumps, and SDK binaries.

PowerShell 7 sequence from `E:\Engine` using the matching 2026.11.07.0 launcher/runtime. Choose new, non-existing project/store paths. Extract the archive to a temporary directory first; copy its `Source` and `Content` trees, plus `ProjectTypes.tsv`, into the fresh project's matching folders. Keep the fresh project's own Build.ps1, Engine.lock.json, Project.json and SDK files. `ProjectTypes.tsv` stores absolute header paths, so replace the archive's old project root with the fresh project root before building.

```powershell
$launcher = 'E:/Engine/Game/build/x64/Release/EngineLauncher.exe'
$runtime = 'E:/Engine/Game/build/x64/Release/Runtime'
$engineStore = 'E:/Engine/Engine/build/playtests/20261107-r5/tired-bomber/EngineStore'
$env:PAINENGINE_ENGINE_STORE = $engineStore
$projectDir = 'E:/Engine/Engine/build/playtests/20261107-r5/tired-bomber/BomberRebuild'
$archiveDir = 'E:/Engine/Engine/build/playtests/20261107-r5/tired-bomber/source-unpacked'

function Invoke-EngineLauncher([string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($launcher)
    $start.UseShellExecute = $false
    $start.Environment['PAINENGINE_ENGINE_STORE'] = $engineStore
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    $code = $process.ExitCode
    $process.Dispose()
    if ($code -ne 0) { throw "EngineLauncher exited $code" }
}

Invoke-EngineLauncher @('--install-engine', $runtime)
Invoke-EngineLauncher @('--create-project', $projectDir, 'Tired Bomber R5')
Invoke-EngineLauncher @('--enable-code', "$projectDir/Project.json")
Expand-Archive 'E:/Engine/Agent/Playtests/2026-11-07-2026.11.06.1-r5/tired-bomber/tired-bomber-r5-source.zip' $archiveDir
Copy-Item "$archiveDir/Source/*" "$projectDir/Source" -Recurse -Force
Copy-Item "$archiveDir/Content/*" "$projectDir/Content" -Recurse -Force
Copy-Item "$archiveDir/ProjectTypes.tsv" $projectDir -Force
$types = Get-Content "$projectDir/ProjectTypes.tsv" -Raw
$types = $types.Replace('E:\Engine\Engine\build\playtests\20261107-r5\tired-bomber\BomberFinalR5', $projectDir)
Set-Content "$projectDir/ProjectTypes.tsv" $types -Encoding utf8
# Touch copied C++ files to avoid stale incremental objects after a filesystem copy.
Get-ChildItem "$projectDir/Source" -Filter '*.cpp' | ForEach-Object { $_.LastWriteTime = Get-Date }
Invoke-EngineLauncher @('--build-project', "$projectDir/Project.json")
& "$runtime/EngineRuntime.exe" --project "$projectDir/Project.json" --mode game --frames 220
```

The recorded final project was first created from an earlier launcher, then upgraded through official `--upgrade-project` to 2026.11.07.0 before the successful build. On the tested machine, standalone gameplay exited 0; bomb release occurred at 1.01667 s and exact trace hit unit 7 at 1.95 s. Full gameplay log and hit frame are in `evidence/`.

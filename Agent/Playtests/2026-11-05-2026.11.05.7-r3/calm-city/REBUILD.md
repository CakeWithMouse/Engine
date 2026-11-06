# REBUILD — Calm City r3, persona 1

Project was created via the .7 launcher CLI and uses the frozen Runtime .7. The source archive contains `Source`, `Content`, project metadata, and Build.ps1; it deliberately omits SDK, DLLs, and Intermediate.

```powershell
$root = 'E:/Engine/Engine/build/playtests/20261105-r3/calm-city'
$launcher = 'E:/Engine/Engine/build/playtests/20261105-r3/EngineLauncher.exe'
$runtime = 'E:/Engine/Engine/build/playtests/20261105-r3/Runtime/EngineRuntime.exe'
$env:PAINENGINE_ENGINE_STORE = "$root/EngineStore"
$env:TEMP = "$root/Temp"
$env:TMP = $env:TEMP
function Invoke-CalmLauncher([string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($launcher)
    $start.UseShellExecute = $false
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    $code = $process.ExitCode
    $process.Dispose()
    if ($code -ne 0) { throw "Launcher failed: $code" }
}

# Fresh CLI project in a NEW directory; enable-code also performs its first C++ build.
Invoke-CalmLauncher @('--create-project', "$root/CalmCity", 'Calm City r3')
Invoke-CalmLauncher @('--enable-code', "$root/CalmCity/Project.json")

# Transfer Source and Content from CalmCity-r3-source.zip into the new project.
# Keep its newly generated Project.json, Engine.lock.json, Build.ps1 and vcxproj.
# Set Project.json startupScene to Content/Scenes/CalmCityImported.scene.json.
Invoke-CalmLauncher @('--build-project', "$root/CalmCity/Project.json")

# Render a real game frame (CALM_FRAME_DUMP requests BMP after the first completed frame).
$env:CALM_FRAME_DUMP = '1'
& $runtime --project "$root/CalmCity/Project.json" --mode game --frames 30

# Save the moving car and the complete scene; launch a second process to reload.
$env:CALM_SAVE_TEST = 'save'
& $runtime --project "$root/CalmCity/Project.json" --mode game --frames 180
$env:CALM_SAVE_TEST = 'load'
& $runtime --project "$root/CalmCity/Project.json" --mode game --frames 180
```

`evidence/` stores command inputs/results, runtime and build logs, visual BMPs, SaveGame JSON, and the role evidence table. Blender import in the original run used `PAINENGINE_BLENDER='C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'` and frozen `Runtime/EngineCommandlets.exe asset.import` with `source='E:/Engine/Engine/build/imported-model-probe/fixtures/blend-suzanne-multiple.blend'` and the project Content directory. No Engine/Editor/SDK source was changed by this persona.

# Curious City r7 — rebuild and replay

Runtime: PainEngine 2026.11.11.0, x64 Release. Build the private project through `ProjectTools.ps1` using the current Release Runtime/SDK; do not use the stale EngineStore SDK inside older project copies. The final source-only archive is `source/curious-city-r7-source-only-final.zip` (119 entries; SHA-256 `F3EB6F293F0B7BDAD93AD1E95564D75750D97A6F25450A951389C0F5AE620E0C`). The older 114-entry ZIP is retained unchanged.

## Fresh extraction and build

```powershell
$archive = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\curious-city\source\curious-city-r7-source-only-final.zip'
$root = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract'
if (Test-Path -LiteralPath $root) { throw "Choose a new extraction directory: $root" }
Expand-Archive -LiteralPath $archive -DestinationPath $root
$project = Join-Path $root 'Project.json'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& 'E:\Engine\Engine\BuildSupport\ProjectTools.ps1' -Action Build -Runtime $runtime -Project $project -Rebuild
```

The project manifest starts `Content/Scenes/Main.scene.json`, mounts its top-level `Content`, and points at the ordinary `Content/UI/CityHud.ui`. Main has the ground, starter cube, and two lights. The test-only `SaveGameSlotProbe` is in `Content/Scenes/SlotRenderProbe.scene.json`.

## Run the normal city, HUD, purchase and SaveGame cycle

`EngineCommandlets.exe` takes positional arguments: `<command> <request.json> <result.json>`. The request has no output field. The saved request is copied and only its `project` field is changed to the fresh extraction. The environment variables keep logs, SaveGames and D3D frame output inside the run folder.

```powershell
$report = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\curious-city'
$root = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract'
$run = Join-Path $root 'run-clean-main'
New-Item -ItemType Directory -Force -Path (Join-Path $run 'saves'), (Join-Path $run 'frames') | Out-Null
$request = Join-Path $run 'city.request.json'
$scenario = Get-Content (Join-Path $report 'evidence\requests\city-hud-save-cycle.request.json') -Raw | ConvertFrom-Json
$scenario.project = Join-Path $root 'Project.json'
$scenario | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $request -Encoding utf8
$env:CURIOUS_CITY_LOG = Join-Path $run 'city.log'
$env:CURIOUS_CITY_SAVE_ROOT = Join-Path $run 'saves'
$env:CURIOUS_CITY_FRAME = Join-Path $run 'frames\main.bmp'
& 'E:\Engine\Game\build\x64\Release\Runtime\EngineCommandlets.exe' project.input $request (Join-Path $run 'city.result.json') *> (Join-Path $run 'commandlet.log')
$exit = $LASTEXITCODE
Remove-Item Env:\CURIOUS_CITY_LOG, Env:\CURIOUS_CITY_SAVE_ROOT, Env:\CURIOUS_CITY_FRAME -ErrorAction SilentlyContinue
if ($exit -ne 0) { throw "project.input failed: $exit" }
```

Expected in `city.log`: `/Game` resolves under this extraction’s `Content`; `CityHud.ui` and city config exist; clicking Build changes balance 500→400 and houses 0→1; Save completes; another purchase after income occurs; Load restores one house and balance 400. `main.bmp` is the actual 640×480 D3D frame. The archived fresh-root replay and its result, log, BMP and build log are in `evidence/source-archive-final/`.

## Run material-slot persistence in its dedicated scene

This check intentionally changes only the scratch project’s startup scene and restores Main afterward. The saved request uses the same city Save/Load path. The source slot material is assigned through the project’s supported slot API; the SaveGame log must preserve both `/Game` paths.

```powershell
$root = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract'
$project = Join-Path $root 'Project.json'
$manifest = Get-Content $project -Raw | ConvertFrom-Json
$oldScene = $manifest.startupScene
try {
    $manifest.startupScene = 'Content/Scenes/SlotRenderProbe.scene.json'
    $manifest | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $project -Encoding utf8
    $run = Join-Path $root 'run-slot-probe'
    New-Item -ItemType Directory -Force -Path (Join-Path $run 'saves'), (Join-Path $run 'frames') | Out-Null
    $request = Join-Path $run 'slots.request.json'
    $scenario = Get-Content 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\curious-city\evidence\requests\city-hud-save-cycle.request.json' -Raw | ConvertFrom-Json
    $scenario.project = $project
    $scenario | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $request -Encoding utf8
    $env:CURIOUS_CITY_LOG = Join-Path $run 'slots.log'
    $env:CURIOUS_CITY_SAVE_ROOT = Join-Path $run 'saves'
    $env:CURIOUS_CITY_FRAME = Join-Path $run 'frames\slots.bmp'
    $env:CURIOUS_CITY_SLOT_MATERIAL = '/Game/Materials/Hologram.mat'
    & 'E:\Engine\Game\build\x64\Release\Runtime\EngineCommandlets.exe' project.input $request (Join-Path $run 'slots.result.json') *> (Join-Path $run 'commandlet.log')
    if ($LASTEXITCODE -ne 0) { throw "Slot probe failed: $LASTEXITCODE" }
} finally {
    Remove-Item Env:\CURIOUS_CITY_LOG, Env:\CURIOUS_CITY_SAVE_ROOT, Env:\CURIOUS_CITY_FRAME, Env:\CURIOUS_CITY_SLOT_MATERIAL -ErrorAction SilentlyContinue
    $manifest = Get-Content $project -Raw | ConvertFrom-Json
    $manifest.startupScene = $oldScene
    $manifest | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $project -Encoding utf8
}
```

Expected `slots.log` contains two slots after load: slot 0 `/Game/Materials/Hologram.mat` and slot 1 `/Game/Models/blend-suzanne-multiple/Materials/Material_1.mat`. The final source-archive replay’s request/result/log/frame/save are preserved in `evidence/source-archive-slot-probe/`.

## Reproduce camera FOV and actual resize

The FOV belongs to the controlled Character. Editing a transient `Player::SetProjection` in `AfterInitialize` is overwritten by the controller’s `Character.cameraSettings`, so the test edits `CuriousPlayer` in `PlayerProbe.scene.json`. The project file is restored after the loop.

```powershell
$root = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract'
$projectPath = Join-Path $root 'Project.json'
$project = Get-Content $projectPath -Raw | ConvertFrom-Json
$oldScene = $project.startupScene
$scenePath = Join-Path $root 'Content\Scenes\PlayerProbe.scene.json'
$scene = Get-Content $scenePath -Raw | ConvertFrom-Json
$player = $scene.objects | Where-Object name -eq 'CuriousPlayer'
$oldFov = $player.projectData.fov
try {
    $project.startupScene = 'Content/Scenes/PlayerProbe.scene.json'
    $project | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $projectPath -Encoding utf8
    foreach ($degrees in @(40, 80)) {
        $player.projectData.fov = $degrees
        $scene | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $scenePath -Encoding utf8
        $run = Join-Path $root "camera-$degrees"
        New-Item -ItemType Directory -Force -Path (Join-Path $run 'frames'), (Join-Path $run 'saves') | Out-Null
        $request = Join-Path $run 'camera.request.json'
        $input = [ordered]@{ project=$projectPath; steps=@(@{kind='control';action='play'},@{kind='focus';enabled=$true},@{kind='frames';count=4},@{kind='resize';width=1280;height=720},@{kind='frames';count=3},@{kind='control';action='stop'}) }
        $input | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $request -Encoding utf8
        $env:PAINENGINE_D3D_SCREENSHOT = Join-Path $run 'frames\post-resize.bmp'
        $env:PAINENGINE_D3D_SCREENSHOT_FRAME = '7'
        $env:CURIOUS_CITY_SAVE_ROOT = Join-Path $run 'saves'
        & 'E:\Engine\Game\build\x64\Release\Runtime\EngineCommandlets.exe' project.input $request (Join-Path $run 'camera.result.json') *> (Join-Path $run 'commandlet.log')
        if ($LASTEXITCODE -ne 0) { throw "FOV $degrees run failed: $LASTEXITCODE" }
        Remove-Item Env:\PAINENGINE_D3D_SCREENSHOT, Env:\PAINENGINE_D3D_SCREENSHOT_FRAME, Env:\CURIOUS_CITY_SAVE_ROOT -ErrorAction SilentlyContinue
    }
} finally {
    Remove-Item Env:\PAINENGINE_D3D_SCREENSHOT, Env:\PAINENGINE_D3D_SCREENSHOT_FRAME, Env:\CURIOUS_CITY_SAVE_ROOT -ErrorAction SilentlyContinue
    $player.projectData.fov = $oldFov
    $scene | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $scenePath -Encoding utf8
    $project = Get-Content $projectPath -Raw | ConvertFrom-Json
    $project.startupScene = $oldScene
    $project | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $projectPath -Encoding utf8
}
```

For each returned actual Play `camera.projection`, derive vertical FOV as `2*atan(1/P[5])`. Expected: 40° and 80° at both 640×480 and 1280×720; `P[0]/P[5]` changes from 0.75 to 0.5625 on resize. Captures are real D3D backbuffers.

## Reproduce visible trace A/B

Use the same Main project, fresh processes, and actual frame 40. The private city sample draws an actual ray/hit cross at a visible ground location; the environment flag retains it for 60 wall-clock seconds so cold shader setup cannot expire the line before capture.

```powershell
$project = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract\Project.json'
$runRoot = 'E:\Engine\Engine\build\playtests\20261114-r7\curious-city\my-fresh-extract\trace-runs'
foreach ($mode in @('keep', 'clear')) {
    $run = Join-Path $runRoot $mode
    New-Item -ItemType Directory -Force -Path (Join-Path $run 'frames'), (Join-Path $run 'saves') | Out-Null
    $request = Join-Path $run 'trace.request.json'
    [ordered]@{project=$project;steps=@(@{kind='control';action='play'},@{kind='focus';enabled=$true},@{kind='frames';count=45},@{kind='control';action='stop'})} |
        ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $request -Encoding utf8
    $env:PAINENGINE_D3D_SCREENSHOT = Join-Path $run 'frames\trace-frame40.bmp'
    $env:PAINENGINE_D3D_SCREENSHOT_FRAME = '40'
    $env:CURIOUS_CITY_LOG = Join-Path $run 'trace.log'
    $env:CURIOUS_CITY_SAVE_ROOT = Join-Path $run 'saves'
    if ($mode -eq 'keep') { $env:CURIOUS_CITY_TRACE_KEEP = '1' } else { Remove-Item Env:\CURIOUS_CITY_TRACE_KEEP -ErrorAction SilentlyContinue }
    & 'E:\Engine\Game\build\x64\Release\Runtime\EngineCommandlets.exe' project.input $request (Join-Path $run 'trace.result.json') *> (Join-Path $run 'commandlet.log')
    if ($LASTEXITCODE -ne 0) { throw "Trace $mode run failed: $LASTEXITCODE" }
}
Remove-Item Env:\PAINENGINE_D3D_SCREENSHOT, Env:\PAINENGINE_D3D_SCREENSHOT_FRAME, Env:\CURIOUS_CITY_LOG, Env:\CURIOUS_CITY_SAVE_ROOT, Env:\CURIOUS_CITY_TRACE_KEEP -ErrorAction SilentlyContinue
```

Expected retained trace count is five before/after (60-second lifetime); the clear run records five then zero. Inspect the matched frame-40 BMPs: the keep frame has a green ray and hit cross, the clear frame does not.

## Archive and evidence

The final ZIP excludes `Binaries`, `Intermediate`, `.painengine`, SDK, EngineStore, and Runtime. Its 119 entries passed a full CRC read. Fresh extraction build id was `7d634a7354a94a46918cc5bb3a92ab9c`; build and gameplay evidence is under `evidence/source-archive-final/`. Material-slot relocation evidence is under `evidence/source-archive-slot-probe/`. The original r6 project at `E:\Engine\Engine\build\playtests\20261114-r6\curious-city\upgraded-city` was not modified.


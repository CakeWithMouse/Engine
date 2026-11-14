# Diary — уставший автор 2D бомбардировки, r6

## Подготовка и сборка
- Base source: PainEngine 2026.11.07.6, d74aebb; r6 working fixes are layered on that base. Shared weekly baseline: 7%; this role has no direct usage counter.
- Installed current Release runtime in my own `EngineStore`; created a fresh CLI project and enabled C++ (both exit 0). Upgraded the real r5 `BomberFinalR5` DLL project into `BomberUpgrade`; `UpgradeReport.json` says `originalPreserved=true`, `standardModuleUpdated=false`; lock is 2026.11.07.6.
- First SDK build was successful before the SceneEditSession fix. After BUG-R6-003, rebuilt BomberUpgrade with `Build.ps1 -Runtime E:/Engine/Game/build/x64/Release/Runtime` because that private same-version store held an older SDK. Current SDK path `.../Runtime/SDK`; Engine.lib SHA-256 `9FD749170E1C7433EC99047591C9B1CBAF6C4A10F38FA72C42BC1E88ADAC8149`; ProjectTools fingerprint `Z/8FdOoCny8rf6DL1TO7cCMhY7BUqy82SqZ4JNpo7sU=`; buildId `445a9ff7edfa4165995bb06683c6e6bf`. Build validated/activated, and the subsequent explicit Build.ps1 run reports `Project is already up to date`.
- Small project-only improvement: HUD records the last bomb trace and displays `last trace: hit unit N` or `miss (ground)` on a separate second line. The original one-line version clipped in BMP; after visual inspection I split it into two lines and rebuilt. Final frame shows both lines fully.

## Gameplay, input, SaveGame
- Current Runtime standalone 240 frames, exit 0, RTX 4070. Bomb hit moving target: `HIT unit id=7 t=1.95s trace=Hit queries=58 triangles=28`. Final 1000x500 image visually checked: HUD shows hits 1, misses 0 and `last trace: hit unit 1`.
- Saved `round-1` at simulation frame 180 after pausing. A separate process loaded it with `TB_LOAD_SLOT=round-1`, `TB_START_PAUSED=1`; exit 0, log restores elapsed=2.98333, hits=1, misses=0, units=5, simulation=0.
- Pre-fix own `project.input` request reproduced BUG-R6-003: command exit 0 but Play left the game simulation flag off, so no bomb/HIT and the target stayed at its initial position. Captured request/result/log outside build.
- Coordinator fixed BUG-R6-003 and rebuilt current Release. I rebuilt BomberUpgrade against the new Engine.lib and repeated the exact same request: exit 0, 150 frames. It now logs bomb release at 1.008s and `HIT unit id=7 t=1.936s trace=Hit queries=60 triangles=28`. `Bomber.Unit.1` moves from `[-6,0.42,-2]` to `[-4.655859,0.42,-1.782786]` during Play; it stays exactly still over 10 paused frames; after Resume it moves another 0.05343. Marked BUG-R6-003 confirmed and fixed.

## Import settings
- On separate `BomberFresh`, current `project.import` commandlet imported the Suzanne model with scale 0.025. It exited 0, accepted=true; `.importsettings.json` persisted version 1 / scale 0.025, and the newly placed model transform reads 0.025 on all three axes. Before/after object counts were 4/5. First attempt used my older private commandlet package, which predated this command; reran successfully with the current rebuilt Runtime commandlet. No Engine source was changed by me.

## GPU commandlets
- Own `render.transparency` request exercised skeletal and crowd/VAT crossed surfaces, intersections, animated time, orbit, LOD and resize. Forward and Deferred each exited 0 with four frames on RTX 4070; 320x256 and 640x360. DrawCalls changed 133/183 and LOD buckets changed from [2,2,0] to [2,0,0]. One Forward frame visually inspected; it shows overlapping semitransparent surfaces. No independent pixel oracle was run, so this is a GPU capture pass rather than full numerical validation.
- Own `render.point-shadows` sequence animated skeletal caster, moved the light, switched shadows off, and restored the initial state. Counts 1,1,0,1 and slots 0,0,-1,0; map hashes changed during motion and restored to the original hashes. Inspected BMP shows the caster and its point-light shadow on the receiver.
- These were my parameterized experiments; shared regression suites were not counted as my tests. They use hidden D3D commandlets and do not replace the visible editor A/B.

## Evidence boundaries
- Code: read-only review before coordinator fix, followed by exact post-fix request through current SDK/runtime.
- Build: fresh project, official upgrade copy, current SDK build, and project-only HUD rebuild.
- CPU/game: standalone hit, project.input hit with Play/Pause/Resume, and cross-process Save/reopen passed after fix.
- GPU: parameterized hidden commandlets and BMP readback passed for the tested states.
- Visual: game hit HUD, transparency Forward and point shadow images inspected. Visible Edit view is not tested by this role.

# REBUILD — Tired Bomber r6

Tested project: PainEngine 2026.11.07.6 Release, base d74aebb with r6 working fixes. Previous real DLL project `BomberFinalR5` was upgraded into the isolated copy `BomberUpgrade`; the source ZIP is `tired-bomber-r6-source.zip` beside this file. The archive excludes SDK files, DLLs, `Binaries`, `Intermediate`, generated screenshots and SaveSlots. It includes `Project.code.json`, project metadata, Source, Content/Models, Content/Scenes, and SOURCE-REBUILD.md.

## Create, enable, upgrade, build

The recorded layout is `Engine/build/playtests/20261114-r6/tired-bomber`; use new nonexisting output paths for a separate run. Launcher: `Game/build/x64/Release/EngineLauncher.exe`; runtime: `Game/build/x64/Release/Runtime`. The project-specific `PAINENGINE_ENGINE_STORE` was `Engine/build/playtests/20261114-r6/tired-bomber/EngineStore`.

```powershell
$env:PAINENGINE_ENGINE_STORE = 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/EngineStore'
$launcher = 'E:/Engine/Game/build/x64/Release/EngineLauncher.exe'
$runtime = 'E:/Engine/Game/build/x64/Release/Runtime'
& $launcher --install-engine $runtime
& $launcher --create-project 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/BomberFresh' 'Tired Bomber R6'
& $launcher --enable-code 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/BomberFresh/Project.json'
& $launcher --upgrade-project 'E:/Engine/Engine/build/playtests/20261107-r5/tired-bomber/BomberFinalR5/Project.json' 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/BomberUpgrade'
```

After BUG-R6-003, build the project with the **current rebuilt runtime SDK**, because installing an already-present same-version runtime does not overwrite the private SDK package:

```powershell
& 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/BomberUpgrade/Build.ps1' -Runtime 'E:/Engine/Game/build/x64/Release/Runtime'
```

Recorded SDK: `E:/Engine/Game/build/x64/Release/Runtime/SDK`, version 2026.11.07.6 Release. `Engine.lib` SHA256: `9FD749170E1C7433EC99047591C9B1CBAF6C4A10F38FA72C42BC1E88ADAC8149`; ProjectTools fingerprint `Z/8FdOoCny8rf6DL2DL1TO7cCMhY7BUqy82SqZ4JNpo7sU=`; buildId `445a9ff7edfa4165995bb06683c6e6bf`. Build was validated and activated. Sandbox hit a confirmed MSVC C1902/PDB issue; elevated retries succeeded only in this isolated project.

Upgrade report: `originalPreserved=true`, `standardModuleUpdated=false`; original r5 project was not edited.

## Reproduce gameplay and Save/reopen

Run current Runtime:

```powershell
& 'E:/Engine/Game/build/x64/Release/Runtime/EngineRuntime.exe' --project 'E:/Engine/Engine/build/playtests/20261114-r6/tired-bomber/BomberUpgrade/Project.json' --mode game --frames 240
```

For the recorded SaveGame path, set `TB_SAVE_FRAME=180` and `TB_PAUSE_BEFORE_SAVE=1` in that process. The log confirms a hit at 1.95s and a paused `round-1` save with hits=1/misses=0 at frame 180. In a new process, set `TB_LOAD_SLOT=round-1` and `TB_START_PAUSED=1`; `loaded round` confirms elapsed=2.98333, hits=1, misses=0 and simulation=0. Slot: `BomberUpgrade/Content/SaveSlots/tired-bomber-round/default/round-1.save.json`.

## Reproduce BUG-R6-003 verification

The exact pre-fix request is `evidence/project-input-prefx/request.json`; the exact retry after fix is `evidence/project-input-postfix/request.json`. Both use Play/focus/130 frames/Pause/10 frames/Resume/10 frames/Stop, 150 frames total. Before fix, play had no game hit and left Bomber.Unit.1 at its start position. After coordinator fix and rebuilding with current Runtime SDK, same request logs bomb release at 1.008s and a trace hit at 1.936s; `Bomber.Unit.1` moves during Play, is unchanged during Pause and moves again after Resume. This is BUG-R6-003, confirmed and fixed by coordinator.

## GPU and import commandlets

Own request/result/BMPs are included under `evidence/gpu-transparency`, `evidence/gpu-point-shadows`, and `evidence/import-settings`. `render.transparency` tested skeletal/VAT crossed surfaces, camera orbit, LOD, animation time, resize and Forward/Deferred; `render.point-shadows` moved animated caster/light, disabled shadows and restored the original map. `project.import` on separate BomberFresh wrote sidecar scale 0.025 and placed the model at 0.025 on all axes. These hidden commandlets do not prove visible Edit-view behavior.

## Source archive rebuild

`tired-bomber-r6-source.zip` contains `Source/`, `Content/Models/`, `Content/Scenes/`, `Project.json`, `Project.code.json`, `ProjectTypes.tsv`, Build.ps1, and project files. The archive excludes engine SDK, `Binaries`, DLL, Intermediate, generated captures and SaveSlots. After extracting to a new project root, rewrite the absolute source root in `ProjectTypes.tsv` as documented in the archive's `SOURCE-REBUILD.md`, then build against the matching 2026.11.07.6 Release SDK.

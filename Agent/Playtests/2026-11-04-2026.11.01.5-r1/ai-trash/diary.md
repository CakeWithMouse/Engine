# AI trash game playtest diary

## Target and docs
Role 4, Release `2026.11.01.5`. Project: `Engine/build/playtests/20261104-r1/ai-trash/TrashGame`; isolated store: sibling `EngineStore` using `PAINENGINE_ENGINE_STORE`; runtime: `Game/build/x64/Release/Runtime` (Release x64, SDK v143).

Read the current API Reference pages for lifecycle, GameComponent, Scene, Input::System, Trace exact geometry, camera/Character, HUD/UI markup, assets, materials, ENGINE_OBJECT and ProjectObjectAdapter, plus USER-PROJECTS, PROJECT-ASSETS, RELIABLE-WORKFLOW and `tests/code-projects/run.ps1`.

## Main slice
- Launcher CLI created the no-code project (exit 0); `ProjectTools.ps1 -Action Enable` added the current C++ scaffold. `Content/ProjectGame.txt` explicitly selects `ProjectGame`.
- Replaced the starter cube scene with a throw lane, four walls and cylinder base for a bucket, ground plane, two point lights, sky and a `ThrowLedger` scene object. The ledger saves/loads `{version, throws, hits}` with a `ProjectObjectAdapter` specialization.
- `ProjectGame` binds `Trash.Throw` to `T` and Space. The HUD button also calls the same throw action. Pressing a key/click cycles through cube, cylinder and sphere trash forms, gives each a scripted parabolic path, and checks each movement segment against `Scene::Raycast` using `Trace::Geometry::Exact`. Only a trace hit whose component ID equals `BucketBase` increments the hit score. This uses mesh geometry; no physics engine was added.
- On startup, the game adds a `DirectionalLightComponent`, `SceneCamera` and a `PlayerCharacter` configured with standard controls off and the camera ID. It scans for an existing sun/player first, avoiding duplicates if an authored scene already contains either.
- Project HUD is selected through `Content/ProjectHud.txt`; `Content/UI/Hud.ui` has a score, status and button. Candidate startup logs show the UI shader and font atlas loading.

## Build and repair log
- First C++ build compiled the gameplay but failed in my code at `ProjectGame.cpp` lines 29, 55, 64, 72 and 78: MSVC C2027, use of incomplete `UIComponent`; `Game.h` only forward-declares it. Full log: `Engine/build/playtests/20261104-r1/ai-trash/TrashGame/Binaries/Release/9c3c897fd8f341509cb88c70c411ff30/build.log`. Added `#include <Engine/Components/SpecificComponents/UIComponent.h>`; next build passed.
- First candidate after adding the Object codec and camera rig failed validation: `LaunchError: Candidate scene rejected: Duplicate or zero object ID`. My script appended IDs 5–11 to a scene that already contained those names/IDs. Full log: `Engine/build/playtests/20261104-r1/ai-trash/TrashGame/Binaries/Release/e9dfb74302254354887c03c582d88845/validate-error.log`. Rebuilt the object list after filtering the previously generated names; next candidate validated.
- Final current project build before rollback probing: `bb145a89f658424a918c720d46f8cbd0`; build log at `Engine/build/playtests/20261104-r1/ai-trash/TrashGame/Binaries/Release/bb145a89f658424a918c720d46f8cbd0/build.log`. Candidate validation and three-frame startup passed; `ProjectTypes.tsv` includes `Trash.Projectile`, `ProjectGame` and `Object ThrowLedger`.
- Launcher `--smoke-project` and `--smoke-project-editor` both exited 0 after the main build. The editor smoke reopens the serialized scene/Object codec. This is a headless load, not a UI save/reopen interaction or visual gameplay proof.

## Export
- `Engine/BuildSupport/Export-Project.ps1 -Project .../TrashGame/Project.json -Destination .../TrashGameExport` exited 0. `ExportReport.json` records Release 2026.11.01.5 and successful scene validation/standalone smoke.
- Launched the exported `Start.cmd --frames 3` from its own folder; exit 0, `/Engine` and `/Game` mounted under `TrashGameExport`, then three frames and clean shutdown. Logs: `TrashGameExport/portable-start.stdout.log` and `portable-start.stderr.log`.

## Visual check requested from coordinator
Frame the lane/bucket around x≈[-2.2,2.2], z≈[-6,7], target (0,1.2,0), high three-quarter view from around (10,10,-15). Press **T** or click **Throw [T / Space]**; Space may conflict with the editor Play/Stop shortcut, so T/button are the unambiguous test. Expected evidence: each toss changes shape and a confirmed bucket-base ray hit increments `Hits`. No screenshot or interaction result from the coordinator is in this report yet; motion, trace hit, HUD click/input, camera image and lighting are `visual-unverified` pending that UI pass.

## Reproduction

```powershell
$env:PAINENGINE_ENGINE_STORE = 'Engine/build/playtests/20261104-r1/ai-trash/EngineStore'
& 'Game/build/x64/Release/EngineLauncher.exe' --smoke-project 'Engine/build/playtests/20261104-r1/ai-trash/TrashGame/Project.json'
& 'Game/build/x64/Release/EngineLauncher.exe' --smoke-project-editor 'Engine/build/playtests/20261104-r1/ai-trash/TrashGame/Project.json'
```

## Coordinator UI observation (4 Nov)
- The Play camera and lighting framed the bucket correctly.
- Clicking the HUD button launched the box and produced `Throws: 1 / Hits: 1`; coordinator observed `Nice throw! It landed in the bucket.` The launch/score action was visible.
- Pressing T launched the can (`Throws: 2`), showing the keyboard action works without Space's editor shortcut.
- By the third throw, the can/paper registered as a rim/non-target hit; score stayed at `Hits: 1 / Throws: 3`. Likely cause: the segment trace also sees a previously stopped `TrashProjectile`, because the query ignores only the current component. This is a user-code issue in `TrashProjectile.cpp`, not evidence of an engine defect. It still needs a repeat after adding ignore IDs for prior trash objects.
- The long success sentence exceeded the 300 px panel's status width. This is a game HUD layout issue. I changed it to the shorter `Hit! In the bucket.` in `ProjectGame.cpp`; the coordinator will Hot Reload/rebuild to verify that adjustment.
- Screenshots from the coordinator: `Engine/build/playtests/20261104-r1/ui-evidence/trash-edit.png`, `trash-play-before.png`, `trash-box-flying.png`, `trash-hit1.png`, `trash-hit2.png`, `trash-result3.png`.

## Follow-up code fix (4 Nov)
- Coordinator attempted Hot Reload/build from the UI. Candidate build `ed38389...` failed in my patch at `TrashProjectile.cpp(68,30)`: C2665 because `Scene::GetComponents()` is keyed by `std::string`, while `Trace::Query::ignore` expects `uint64_t`. The failed candidate did not replace the previously active module (`5307...` per coordinator).
- Corrected the ignore list to use `entry.second->GetComponentId()` for every existing `TrashProjectile`. This is a user-code error, not an Engine error. The fix is source-only and has not been rebuilt or re-tested yet; coordinator is separately checking the editor Hot Reload path.

## Final gameplay, reload and package (coordinator, 4 Nov)
- Coordinator rebuilt and activated final module `4c74a1b450764b5a9aaa2df6f3a1fc42` after the component-ID correction. On a fresh project reopen, button and `T` worked; box, can and paper produced `Hits: 3 / Throws: 3`. Short success text fit the HUD. Screenshot: `Engine/build/playtests/20261104-r1/ui-evidence/trash-final-hits3.png`.
- Hot Reload did activate the new DLL, but immediately afterward the HUD and `T` stopped responding. Freshly reopening the project restored input. Coordinator tracks this separately as BUG007; gameplay success above is from the fresh reopen, not the immediate post-reload state.
- Re-exported the final project to `Engine/build/playtests/20261104-r1/ai-trash/TrashGameExportFinal`. Export validation succeeded; standalone `Start.cmd --frames 3` from the package folder exited 0, mounted its own Runtime and Content and stopped cleanly. Logs: `TrashGameExportFinal/ExportReport.json`, `final-start.stdout.log`, `final-start.stderr.log`.
- Created a tiny separate `AIProbe` from the CLI/SDK C++ scaffold and compiled the shipped `SDK/Examples/ApiReference/ReferenceComponent.h` and `ReferenceExamples.cpp`. It exercises the documented `EngineModuleVfs::Resolve("/Game/Config/message.txt")` bridge and the SDK sample's `ReadGameLine()` helper. Build `abad89b961d9405ab653825c8f646687` validated/activated; smoke exited 0 and logged `Host VFS resolve: 1 read=host VFS ready`. This confirms mounted host VFS path resolution and content read through the SDK bridge in standalone runtime; no custom host service was registered by game code.
- First SDK Enable build in sandbox failed with MSVC LNK1101/MSPDB140; authorized external build of the isolated project passed. No shared Engine/SDK outputs were changed.

## Frozen compatibility and TraceScreen CLI probe
- For the second persona, extracted the checked-in `pre-lights.zip` fixture into `AIProbe/FrozenCompatibility`, verified all seven files against the archive SHA256SUMS, and copied the DLL/dependencies into a separate fixture project pinned to `2026.10.24.0`. `--check-project` exited 0. Running the runtime directly with that old pin rejected it as expected; `EngineLauncher --smoke-project-current` forced the current runtime's compatible-engine route and exited 0. Frozen `Game.dll` hash before/after was unchanged at `0B26410EF32692CAD532CAB3FA6AE8265B702FA5C89534DA8D58B78CC88F1500`. This confirms current-runtime load/start/three-frame smoke for the pre-lights DLL. It does not establish old-runtime selection, interactive upgrade, or full CompatibilityTests coverage. Logs: `.../AIProbe/FrozenCompatibility/result.txt`, `sha256-verified.txt`.
- Extended the separate AIProbe to create a `SceneCamera` and player, then call public `Game::ScreenPointToRay` and `Game::RaycastScreen` for a centered physical pixel and a fractional logical point. Build `5ed0fedab505437b8f23ed61f6803b01` validated/activated; launcher smoke exited 0. Startup reported `TraceScreen center ray=Success hit=Hit fractional=Hit triangles=4`. This is deterministic runtime evidence for screen-ray generation and world geometry hit without desktop UI. Log: `.../AIProbe/Binaries/Release/5ed0fedab505437b8f23ed61f6803b01/startup.log`.

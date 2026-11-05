# Issues and errors

## Build attempt: subsystem incorrectly assumed Game access

- Category: `user-code-error` (corrected in project source)
- Command: project `Build.ps1 -Runtime E:/Engine/Engine/build/playtests/20261105-r2/Runtime` (standard MSVC outside sandbox)
- Expected: compile the current game module and activate the candidate DLL.
- Fact: ComponentGenerator completed and found all 6 annotated types, then MSVC rejected `GetGame()` in `CityEconomySubsystem.cpp`; `GameSubsystem` has no Game accessor. Candidate was rejected and the previous module remained active.
- Diagnostic: [build-attempt-1.log](../../../Engine/build/playtests/20261105-r2/curious-city/fresh-city/build-attempt-1.log) and full SDK build log [build.log](../../../Engine/build/playtests/20261105-r2/curious-city/fresh-city/Binaries/Release/747354d08a444e1ba344bfbb4f42cbf3/build.log).
- Resolution: pass the active `Scene&` to the subsystem purchase method; keep Save/Load calls in the GameComponent callback, where the current `Game` is available. Rebuild pending.

## Build attempt: incomplete SaveGame type in callback translation unit

- Category: `user-code-error` (corrected in project source)
- Command: same project build via SDK ProjectTools/MSVC.
- Expected: compile and activate the fixed module.
- Fact: ComponentGenerator found all 6 annotated types; MSVC rejected `game->Saves().RequestSave/RequestLoad` because `Game.h` forward declares `SaveGame` and its full declaration was not included at the call site. Candidate stayed unactivated.
- Diagnostic: [build-attempt-2.log](../../../Engine/build/playtests/20261105-r2/curious-city/fresh-city/build-attempt-2.log), full log [build.log](../../../Engine/build/playtests/20261105-r2/curious-city/fresh-city/Binaries/Release/6f7a13b388b24aec92c60028966cb25e/build.log).
- Resolution: include `Engine/SaveGame/SaveGame.h` in `CityInteractionComponent.cpp`; rebuild pending.

## `project.input` first request used the wrong step shape

- Category: `user-code-error` (probe invocation, not an Engine defect)
- Command: `E:/Engine/Game/build/x64/Release/Runtime/EngineCommandlets.exe project.input button-click-request.json button-click-result.json`
- Expected: Play, focus, move cursor over Build, click, and record changed objects.
- Fact: The connected host initialized a real D3D11 device, then returned exit 2 and `{"ok":false,"error":"unknown project.input step"}` because the request used guessed `{control:...}` / `{frames:...}` objects.
- Log: [button-click-launch.log](../../../Engine/build/playtests/20261105-r2/curious-city/button-click-launch.log), [button-click-request.json](../../../Engine/build/playtests/20261105-r2/curious-city/button-click-request.json), [button-click-result.json](../../../Engine/build/playtests/20261105-r2/curious-city/button-click-result.json).
- Resolution: waiting for the command author's exact step JSON contract, then retry against the existing scratch project.

## Archived old project has no game DLL for direct upgrade

- Category: `documented-limit` of this source archive (no Engine defect found).
- Command: baseline `EngineLauncher.exe --upgrade-project old-copy/Project.json old-upgraded` (MSVC permitted, separate destination).
- Expected: upgrade the archived project into a new 2026.11.05.1 copy and leave `old-copy` intact.
- Fact: launcher returned exit 1 with its generic upgrade message. Direct Runtime diagnostic returned exit 1: `LaunchError: Cannot hash game DLL`; the extracted `curious-city-final.zip` contains `Project.json` that points to a `Binaries/.../Game.dll`, but the archive intentionally contains no `Binaries` directory. No destination was accepted; the source copy and user library remain untouched.
- Logs: [old-project-upgrade.log](../../../Engine/build/playtests/20261105-r2/curious-city/old-project-upgrade.log), [old-runtime-upgrade.log](../../../Engine/build/playtests/20261105-r2/curious-city/old-runtime-upgrade.log).
- Next: create a new current-version scaffold in the run folder and import only the old `Source`/`Content`, then build/launch that migrated copy. This keeps the source archive intact and gives it a valid current module to test.

## First wrapper launch attempt had an invalid `cmd /c` argument boundary

- Category: `user-code-error` (probe invocation; the exported runtime itself passed a direct three-frame launch).
- Command: `cmd.exe /c Start.cmd --frames 3` via `ProcessStartInfo.ArgumentList` with an unquoted path containing spaces.
- Expected: run the exported `Start.cmd` wrapper for three frames.
- Fact: exit 1, `The filename, directory name, or volume label syntax is incorrect.` This was command-shell quoting from the probe harness; direct execution of the packaged `Runtime/EngineRuntime.exe --project Game/Project.json --mode game --frames 3` already returned exit 0.
- Log: [StartCmd-smoke.log](../../../Engine/build/playtests/20261105-r2/curious-city/CityExport/StartCmd-smoke.log).
- Resolution: invoking `cmd.exe /c call E:\...\CityExport\Start.cmd --frames 3` as one executable command with backslash path and no extra quoting layer completed successfully with exit 0. The earlier failures were entirely in the probe's quoting, not the exported package.

## `project.input` initially rejected `Keys::LeftButton`

- Category: `tool-defect` (root commandlet; fixed by its author).
- Command: `EngineCommandlets.exe project.input` with cursor at the Build button and key 1, then retry with key 500.
- Expected: one left click triggers the Build button and adds exactly one house.
- Fact: key 1 did not represent LeftButton; the public key enum value is 500. Initial commandlet range rejected 500. After the commandlet was changed to allow the documented 0..507 key range, the key-500 probe logged `build_ui_click` and `house_add_deferred_accepted_and_charged balance=400 houses=1`. Repeating six clicks charged five houses ($100 each) and rejected the sixth at balance 0 without adding a house.
- Logs/results: `button-click-probe.log`, `button-click-key500-final.log`, `build-budget.log`, `build-budget-result.json` in the project run folder; parent tracked the commandlet defect as BUG-R2-003.
- Resolution: commandlet accepts key 500; game-side HUD and click behavior work.

## First `project.scene` GameObject invocation lacked a writable temp directory

- Category: `probe-configuration-error` (not an Engine defect; docs require TEMP/TMP override in restricted hosts).
- Command: `EngineCommandlets.exe project.scene neighborhood-create-only.request.json neighborhood-create-only.result.json`.
- Expected: create a fresh project scene and custom `CuriousCity.Neighborhood` object.
- Fact: command exited 0 but operation statuses were 6. Result reported `Windows error 5` while creating `C:/Users/serge/AppData/Local/Packages/.../Temp/EngineCommandlets-.../*.scene.json.tmp`; the next dependent operation therefore had no `NorthHomeA` object. No project scene was saved or changed.
- Logs: `neighborhood-create-only.result.json`, `neighborhood-create-only.launch.log`, `neighborhood-object.result.json` in the run folder.
- Resolution: set commandlet process `TEMP` and `TMP` to run-local `commandlet-temp` before retry, as documented in Engine/docs/COMMANDLETS.md.

## HUD label width clipped an overlong Russian button caption in the project copy

- Category: `project-layout-adjustment` (HUD markup sizing, not an Engine defect).
- Command: `EngineCommandlets.exe ui.edit-text` on `hud-editor-copy/Content/UI/CityHud.ui`, then standalone `EngineRuntime.exe --project hud-editor-copy/Project.json --mode game --frames 30` with `CURIOUS_CITY_FRAME_DUMP_PATH`.
- Expected: edited label is saved and readable after the game loads the changed HUD file.
- Fact: the editor persisted `Построить дом • 100 ₽`, but it exceeded the existing 96 px Build button. I shortened it to `Дом 100 ₽`; the relaunched HUD displayed it fully in the same 96×32 button. The edited `balance` label also persisted in markup, then the game's live balance binding replaced its placeholder text as designed.
- Evidence: `ui-edit-button.result.json`, `ui-edit-button-final.result.json`, `hud-edit-reloaded-clean.bmp`, `hud-edit-reloaded-clean.launch.log` in the run folder.
- Resolution: keep the short Russian caption in this project copy; no Engine change required.

## Play resume required a fresh key-down event for continued character movement

- Category: `observed-input-behavior` (no Engine defect established).
- Command: `EngineCommandlets.exe project.input` on `old-import/Project.json` with W held across Pause and Resume.
- Expected: Player advances during Playing, stays still while Paused, and advances after Resume while movement input is active.
- Fact: Z moved `0 → -0.768` in Playing and remained `-0.768` through 10 paused frames. A repeated down=true without an intervening release did not advance after Resume; after explicit key-up/key-down, Z advanced to `-1.536`, then remained there after release frames. Stop restored the edit-scene position 0.
- Evidence: `play-pause-resume.result.json`, `play-pause-resume-retry.result.json`, and adjacent `.launch.log` files in the run folder.
- Resolution: final test explicitly released/re-pressed W after Resume; continue to treat held input across pause as unverified without that re-press.

## Initial .3 `--enable-code` link failed under sandbox MSPDB environment

- Category: `sandbox-toolchain-environment` (known MSPDB140 failure; not an Engine/source defect).
- Command: `Engine/build/playtests/20261105-r2/final/EngineLauncher.exe --enable-code Engine/build/playtests/20261105-r2/curious-city/final-city-probe/Project.json`.
- Expected: enable C++ and activate the first project module against final SDK 2026.11.05.3.
- Fact: ComponentGenerator succeeded and compiled Module.cpp, ProjectComponent.cpp, ProjectGame.cpp, RotatingCube.cpp, GeneratedComponents.cpp; linker failed with `LINK : fatal error LNK1101: incorrect version of MSPDB140.DLL`. No candidate module activated.
- Log: `Engine/build/playtests/20261105-r2/curious-city/final-city-probe/Binaries/Release/eb1bf97eddbf40b3ab0928445143b241/build.log`; launcher result `Engine/build/playtests/20261105-r2/curious-city/final-city-enable-code.log`.
- Resolution: rerun the project's standard `Build.ps1` under the authorized MSVC escalation.

## First lighting scene request referenced an already-removed template cube

- Category: `probe-request-error` (stale name in our scratch request; no Engine defect).
- Command: final Runtime `EngineCommandlets.exe project.scene final-light-scene.request.json final-light-scene.result.json`.
- Expected: remove template Cube, add two city houses, save pre/post lighting scenes.
- Fact: the project Main.scene was manually prepared without the template Cube while adding CesiumResident; the first commandlet step therefore returned `Object not found: Cube` and aborted before any save or mutation.
- Log/result: `Engine/build/playtests/20261105-r2/curious-city/final-light-scene.launch.log`, `final-light-scene.result.json`.
- Resolution: remove the stale delete step and rerun the same scratch scenario.

## Texture authoring fixture destination was already occupied

- Category: `probe-fixture-conflict` (our scratch setup; no Engine defect).
- Command: final Runtime `EditorUiTests.exe <final-city-probe> <EngineContent> --authoring`.
- Expected: import a red 2x2 texture, reimport a blue source, then move it to `Content/Images/Renamed.bmp`.
- Fact: first run exited 1 with `status=6 (Choose a new destination inside Content with the same extension)` because the red control texture already occupied the target. We moved the control file outside Content and reran; the editor then failed its scene fixture load because the scene referenced the missing control texture (`Failed to open virtual file: /Game/Images/Renamed.bmp`).
- Logs: `Engine/build/playtests/20261105-r2/curious-city/texture-reimport-authoring.log` and `texture-reimport-authoring-retry.log`.
- Resolution: temporarily removed only the scene reference during the UI authoring run, then restored the exact pre-authoring city scene. Final run passed 211 checks; output BMP is blue. The original uninstrumented GPU A/B was invalid because neither frame had a texture SRV; see BUG-R2-007 below for the corrected explicit-rebind A/B.

## First custom render-probe build used derived unique_ptr at base-reference API

- Category: `project-build-error` (our probe source typing mistake; no Engine defect).
- Command: standard final SDK `Build.ps1` after adding `CityRenderProbe`.
- Expected: compile isolated project module.
- Fact: MSVC C2664 rejected conversion of `unique_ptr<CityRenderProbe>&` to `unique_ptr<GameComponent>&` at `Scene::AddComponent`.
- Log: `Engine/build/playtests/20261105-r2/curious-city/final-city-probe/Binaries/Release/e700813bfa654fd293c3b46e33450125/build.log`.
- Resolution: declare the local as `std::unique_ptr<GameComponent>` before assigning `make_unique<CityRenderProbe>()`; standard Build.ps1 then succeeded and activated module `d03a23696aa34637b585802ef164c1a0`.

## BUG-R2-007: scene-restored mesh texture pending load (.3 reproduced; .5 fix independently verified)

- Category: `engine-runtime-texture-lifecycle` (reproducible with the final .3 Runtime; sent to root for fix/regression).
- Command: launch `EngineRuntime.exe --project .../final-city-probe/Project.json --mode game --frames 50` with `CURIOUS_CITY_TEXTURE_PROBE` set; scene file is `Engine/build/playtests/20261105-r2/curious-city/final-city-probe/Content/Scenes/Main.scene.json`.
- Expected: restored `WestHome` id 5 has path `/Game/Images/Renamed.bmp`; after Game has a D3D device/context and components are synchronized, its pending scene texture should be loaded and `HasTexture()`/`GetTexture()` should be true without manually reassigning it.
- Fact: at PreUpdate frames 1, 2, 3, 10, and 20, found=1 and texture_path is correct, while `HasTexture=0`, `texture_srv=0`, even though `device=1`, `context=1`. Explicit `TrySetTexture` of the same path at frame 2 returned true; frames 2/3/10/20 then reported HasTexture=1/SRV=1 and rendered colored pixels. The legacy no-rebind A/B screenshots were therefore both untextured and pixel-identical.
- Repro evidence: `Engine/build/playtests/20261105-r2/curious-city/texture-instrumented-red.log`, `texture-instrumented-red.explicit.log`, plus matched GPU frames under `Agent/Playtests/2026-11-05-2026.11.05.1-r2/curious-city/evidence/final-probes/`.
- Likely cause (root inspection): project `SceneProject::Cube` is a `MeshComponent`; `MeshComponent::CreateBuffers` does not call `GameComponent::LoadPendingTexture`, unlike the base component path. No Engine/SDK changes were made by this persona.
- A/B after explicit rebind: same scene/camera and 50 frames; red 2×2 BMP vs the actual blue 2×2 authoring-reimport output. Full-frame delta is 11,582 pixels, all within the WestHome cube ROI `(618..740, 202..297)`; 11,462 red pixels became 11,582 blue pixels.


## Initial .5 texture-probe copy omitted generated Visual Studio files

- Category: `probe-copy-setup-error` (our isolated copy setup; no Engine defect).
- Command: `Build.ps1 -Runtime E:/Engine/Engine/build/playtests/20261105-r2/final5/Runtime` from `Engine/build/playtests/20261105-r2/curious-city/final5-texture-probe`.
- Expected: compile the independent texture probe against .5.
- Fact: ProjectTools stopped before compilation while hashing missing `Game.vcxproj`; this copy was intentionally assembled from Source/Content and manifest but omitted the generated solution/project files.
- Log: `Engine/build/playtests/20261105-r2/curious-city/final5-copy-build-first-attempt.log`.
- Resolution: copied the generated project-relative `Game.vcxproj` and `Game.sln` from the source scratch, then retry.

### Independent final .5 retest

- Command: built a separate source copy with `Build.ps1 -Runtime E:/Engine/Engine/build/playtests/20261105-r2/final5/Runtime`; the project lock is 2026.11.05.5 and Engine.lib SHA256 is `0FA010193F59C1BBE25B2CFA8EAAB70B51B8C08027E90A0273B9C6F2285F48FA`.
- Removed the explicit `TrySetTexture` workaround from the copied probe source before building.
- Expected: scene restore loads the texture during component resource creation; no project setter is needed.
- Fact: red and blue runs each reported `found=1`, path `/Game/Images/Renamed.bmp`, `has_texture=1`, `texture_srv=1`, `try_texture=0` from frame 1, with Device/Context available. Both game processes exited 0 after 50 frames and dumped 1000×500 BMPs. Full A/B diff is 11,582 pixels, confined to WestHome ROI x=618..740/y=202..297; red 11,462 pixels, blue 11,582.
- Evidence: `Engine/build/playtests/20261105-r2/curious-city/final5-texture-red.bmp`, `final5-texture-blue.bmp`, and adjacent `.log`/`.launch.log`; report copies under `evidence/final-probes/texture-final5-*`.
- Result: fixed in .5; the independent source-only reproduction archive is `CuriousCity-final5-texture-probe-source.zip`. No Engine or SDK file was modified by this agent.


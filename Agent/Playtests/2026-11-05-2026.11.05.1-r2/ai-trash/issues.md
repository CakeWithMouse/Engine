# Issues

## r1 project archive cannot be upgraded without its DLL
- Action/time: 2026-11-05, archive migration attempt.
- Command: `EngineLauncher.exe --upgrade-project Engine/build/playtests/20261105-r2/ai-trash/TrashGameSourceArchive/Project.json Engine/build/playtests/20261105-r2/ai-trash/TrashGameDev`.
- Expected: current .1 copy created from the archived project.
- Actual: launcher returned exit 1, `Project copy could not be upgraded. Original preserved. Check unknown components, missing assets or custom DLL compatibility.` The archive manifest references `Binaries/Release/4c74a1b450764b5a9aaa2df6f3a1fc42/Game.dll`, which is intentionally absent from the source-only archive.
- Exit: 1.
- Log: `Engine/build/playtests/20261105-r2/ai-trash/upgrade.log`.
- Category: environment-blocker (archive lacks the old compiled module required by this migration path; no claim of an engine defect).
- Follow-up: preserve the archive, create a new .1 C++ scaffold, then copy only archived Source/Content into that isolated project and rebuild.

## Archive rebuild linked against stale template object files
- Action/time: 2026-11-05, first build of the isolated archive-based TrashGameDev after copying r1 Source/Content.
- Command: `EngineLauncher.exe --build-project Engine/build/playtests/20261105-r2/ai-trash/TrashGameDev/Project.json`.
- Expected: compile archived game sources and activate the current DLL.
- Actual: C++ compile ran, but incremental MSBuild reused a newer template `ProjectGame.obj` from the just-created scaffold because ZIP source timestamps were older; link failed with unresolved `ProjectGame::RegisterHit` and `ProjectGame::RegisterMiss` from `TrashProjectile.obj`. Previous module was preserved.
- Exit: 1.
- Log: `Engine/build/playtests/20261105-r2/ai-trash/TrashGameDev/CodeBuild.log` and `.../Binaries/Release/820bd44383724b1eb419433f8d3f9a56/build.log`.
- Category: user-code-error (stale local incremental outputs after replacing a template source archive; not an Engine/SDK defect).
- Follow-up: clean only this project's own `Intermediate/Release` and rebuild with MSVC.

## project.reload-input rejected after DLL replacement attempt
- Action/time: 2026-11-05, attempt to verify real T/HUD input before and after reload on TrashGameDev.
- Command: `Editor/tests/ReloadInputTests.ps1 -Project Engine/build/playtests/20261105-r2/ai-trash/TrashGameDev/Project.json -Output Engine/build/playtests/20261105-r2/ai-trash/reload-input`.
- Expected: object snapshots 0 → 1 on T → 2 on HUD click, both before and after a candidate DLL replacement.
- Actual: commandlet reached its post-reload guard and returned `candidate did not replace session`. It did not emit final before/after snapshots, so post-reload input remains unverified.
- Exit: 2 from `EngineCommandlets.exe`; the PS test wrapper threw.
- Log/result: `Engine/build/playtests/20261105-r2/ai-trash/reload-input/console.log`, `result.json`.
- Category: environment-blocker. Root diagnostics show Windows error 5 writing the commandlet temporary checkpoint under sandbox AppData; the earlier candidate-replacement message was secondary. No claim that gameplay input failed.
- Follow-up: resolved for this attempt by rerunning with `TEMP` and `TMP` set to the project-owned `ScratchTemp`; see `Engine/build/playtests/20261105-r2/ai-trash/reload-input-retry/results.json`, which passed six gameplay assertions before and after candidate DLL replacement.

Root diagnosis artifact: Engine/build/playtests/20261105-r2/ai-trash/reload-input/root-diagnostic.json (Windows error 5 on checkpoint temp file).

## project.input first HUD-click attempt used the stale mouse-key value
- Action/time: 2026-11-05, initial HUD click probe on TrashGameDev.
- Command: `EngineCommandlets.exe project.input` with `{"kind":"key","key":1,"down":true}` after setting cursor to `(95,131)`.
- Expected: one Trash.Projectile from HUD button click.
- Actual: no projectile; initial published commandlet documentation described left mouse as key 1, but updated docs define Engine Keys 0..507 and `LeftButton=500`. Repeated with key 500 and got one projectile; combined held-T then key-500 sequence ended with exactly `Cardboard box 1` and `Soda can 2`.
- Exit: 0 for both invocations; first action produced no gameplay effect, corrected reruns succeeded.
- Logs/results: `Engine/build/playtests/20261105-r2/ai-trash/project-input-hud.result.json`, `project-input-hud500.result.json`, `project-input-live.result.json` and corresponding `.log` files.
- Category: documentation mismatch / resolved by command contract clarification; root tracks the underlying release regression as BUG-R2-003. Not a new Engine issue.

## material.graph save correctly rejected while graph sidecar was locked
- Action/time: 2026-11-05, write-failure probe on isolated TrashMaterialScratch.
- Command: `EngineCommandlets.exe material.graph <write-failure3.request.json> <write-failure3.result.json>`; the AI graph was pasted, the `.mat.graph.json` sidecar was held with a read-only Windows handle during Save, then Revert and Save were requested.
- Expected: the locked write is rejected without clearing dirty state; Revert restores the original graph, and the subsequent save succeeds.
- Actual: `save accepted=false, dirty=true`, dialog reported access denied on `Cardboard.mat.graph.json`; `revert accepted=true, dirty=false`; final `save accepted=true`.
- Exit: 0 (expected failure was represented in the per-step result, then recovery succeeded).
- Log/result: `Engine/build/playtests/20261105-r2/ai-trash/TrashMaterialScratch/write-failure3.log` and `write-failure3.result.json`.
- Category: expected negative-path behavior; no Engine defect observed.



## Initial EditorUiTests sideprobe runs used unsuitable setup
- Action/time: 2026-11-05, first UI-suite attempts on role4 scratch.
- Command: `EditorUiTests.exe <Trash UI probe Project> <root Runtime/EngineContent> <CesiumMan.glb>`.
- Expected: actual Qt/D3D UI workflow against an untouched four-object CLI project.
- Actual: the first attempt set `QT_QPA_PLATFORM=offscreen` and could not attach an HWND viewport. A later attempt used the Windows backend but reused a post-run root fixture with ten objects and stopped at the suite's `starter hierarchy` assertion. I then tried an altered camera fixture that failed `load UI fixture`. These were setup mistakes and not counted as product issues.
- Exit: 1 for those attempts; corrected run below passed 5028 checks.
- Logs: `Engine/build/playtests/20261105-r2/ai-trash/editor-ui-probe-escalated.log`, `editor-ui-probe-windows.log`, `editor-ui-probe-camera4.log`.
- Category: test setup error.
- Follow-up: created a new unmodified no-code project with the root release launcher, used `QT_QPA_PLATFORM=windows`, root Runtime EngineContent and the original CesiumMan.glb source. `EditorUiTests.exe` exited 0 with `EditorUiTests: 5028 checks passed`; selected role4 feature effects are reported in `evidence.json`, not inferred for untested IDs.
- Successful log/screenshots: `Engine/build/playtests/20261105-r2/ai-trash/ui-probe-fresh.log` and `TrashUiProbeFresh/*.png`.
- A separate attempt to synthesize the UI fixture with `project.scene` returned `{"ok":false,"error":"Unexpected or missing JSON fields"}` (exit 2) before changing the CLI template; see `TrashUiProbeFresh/ui-fixture/scene-fixture.result.json`. This helper attempt was abandoned after using the required clean launcher fixture.

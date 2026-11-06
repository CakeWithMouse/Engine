# Diary — tired bomber, PainEngine 2026.11.05.7

## Setup and first failure
- Started at 41% shared weekly usage. Used a new CLI C++ project, dedicated engine store and TEMP/TMP in this playtest tree; frozen Runtime/SDK .7 was read-only.
- `--create-project` exited 0. I accidentally started `--enable-code` once in the sandbox, did not collect its `session_id`, and then started a second elevated process while the first still held `CodeBuild.log`. The overlapping invocations caused `Cannot create CodeBuild.log` and one sandbox attempt logged `LNK1101 MSPDB140.DLL`. Preserved the second attempt's log/stderr in `incident-concurrent-enable/`; coordinator verified no build processes remained. One clean elevated retry exited 0. Classified as harness/environment; not a PainEngine defect.
- Transferred only the old Source/Content. The new CLI project kept its own Build.ps1, Engine.lock.json, generated type files, SDK link and module output.

## New material-slot round
- Imported the real `blend-suzanne-multiple.blend` and `blend-uv.blend` through .7 `asset.import`. Import returned two Suzanne submeshes and source material IDs 0 and 1; the UV control returned one slot and its own material. Set slot 1 through `project.scene`, saved Main, reset the scene session and loaded Main. `projectData.materialSlots` restored the variant assignment. The C++ game iterates the map rather than assuming ID density; it called `GetMaterialSlots`, `SetMaterialSlot`, invalid-ID rejection, global material set/clear and restored the original slot.
- An early authoring probe tried to set `/transform/position` through the generic component `property` action. It correctly returned `Property is not editable`; I replaced it with documented `move` actions. This was an operation mismatch, not a defect.
- The first UV fixture placement was edge-on to the camera, so its frame did not prove UV behavior. Rotated the control plane toward the orthographic camera and enlarged it. The resulting real D3D frame shows the yellow/blue BaseMap regions.
- Paused A/B runtime captures held the camera and scene constant: translucent/no-shadow, masked alpha-zero, opaque/shadow-on, opaque/shadow-off. The mask removed one submesh and its shadow; toggling `castShadows` removed the shadow behind the otherwise identical opaque model. Captures are in `evidence/render-ab/` under the build output, with copies in the report.

## Build, play, save and reopen
- After transferring Source/Content and importing the assets, the official launcher `--build-project` compiled and activated the new DLL; exit 0. No Runtime/SDK files were copied into the project.
- Real standalone `EngineRuntime --mode game --frames 220` exited 0. The moving bomb produced an exact trace hit on a unit at 1.95s, HUD hit count reached 1, and GPU hit sparks rendered. Completed-frame counters and LOD detail were logged. The project wrote a round SaveGame; a second Runtime process loaded it at elapsed 2.4s with hits=1, five unit references, sparks/timer and paused state.
- Scene Save→new session→Load restored `projectData.materialSlots`; actual scene JSON is in Content/Scenes/Main.scene.json. This plus the second-process SaveGame is the re-open evidence.

Feature status and exact file paths are in `evidence.json`. The assigned probes are complete; items marked partial record their concrete boundary.

## Final .8 narrow regression
- Coordinator's .8 Runtime was used only for a narrow follow-up. Upgraded the new CLI project into a separate `Final8Bomber`, rebuilt with final8, then ran a live bomb hit and SaveGame at frame 145 (hits=1). A separate Runtime process loaded `round-1`; it restored two source material slots, including source ID 1 mapped to `Material_1_variant.mat`, and the hit round values (elapsed 2.4, hits=1). Both game processes exited 0.
- final8 `project.scene` save → new → load completed with empty errors and retained `projectData.materialSlots["1"]`. The final8 hit image and exact outputs are copied under `evidence/`. This verifies the prior .7 derived-component restoration gap in the upgraded game path; no engine source was changed.
- The first unprivileged final8 scene commandlet could not create its OS temporary file (Windows error 5); rerunning that exact request with the allowed elevated native commandlet succeeded. The initial commandlet usage mistake and this harness permission failure were not engine defects.

- Archived the .7 project source and required Content assets in a compact source-only ZIP. Screenshots in the durable report are PNG; runtime BMPs remain only in the build tree.


## Bounded .8 coverage supplement
- In a separate final8 scratch folder, six `mesh.primitive` calls succeeded (cube, sphere, plane, cylinder, cone, pyramid). A `project.scene` select/move/undo/redo/undo/save/new/load sequence returned status 0 and restored the original Ground position. `gizmo.snap` and `gizmo.center` returned the expected 0.5 grid results. Perspective screen-ray calls at viewport center and upper-right both succeeded with distinct directions.
- `--smoke-project-current` on Final8Bomber exited 0. The initial CLI help inspection alone was not counted as export evidence; a later official export workflow is recorded below. `trace-screen` remains partial because no Game::RaycastScreen/RaycastCursor hit was exercised. SDK examples remain not-tested because the official ReferenceExamples.cpp was not compiled; analogous project APIs do not satisfy that feature. Compact requests/results are in `evidence/final8-additional/`.

## Official export and package launch
- At the coordinator-confirmed 47% shared usage, ran the single approved Engine/BuildSupport/Export-Project.ps1 attempt against Final8Bomber and final8 Runtime, destination Engine/build/playtests/20261105-r3/tired-bomber/ExportedBomber8. Export validation/publish returned exit 0 and produced Game, Runtime, Start.cmd, ExportReport.json and validation/smoke logs. The export's official validation smoke completed 3 frames.
- Launched the exported package from its own folder using exported Runtime/EngineRuntime.exe, exported Game/Project.json, game mode and 90 frames. Exit 0; GameUpdates=90, then GameStop/EngineStopped. This is a real package launch separate from the pre-export smoke.
- Durable export reports/logs and package run output are in vidence/final8-additional/. No build was run.


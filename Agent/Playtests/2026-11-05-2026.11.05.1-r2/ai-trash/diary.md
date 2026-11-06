# Diary — AI Trash (r2)

## Isolated project and small gameplay step

- Created `TrashGameFresh` and `TrashGameDev` from the launcher CLI. Enabled C++ in both, then used only `TrashGameDev` for the archived game continuation. `TrashGameDev` lives under `Engine/build/playtests/20261105-r2/ai-trash/`; the r1 ZIP is preserved in `TrashGameSourceArchive`.
- Source-only r1 archive migration failed because its `Project.json` points to an omitted old `Game.dll`; this is recorded in `issues.md`. The project was rebuilt from a fresh 2026.11.05.1 C++ scaffold and copied source/content. A stale-template-object link failure was followed by a clean rebuild in this project's own `Intermediate`.
- Implemented a three-item loop: cardboard box, soda can, and crumpled paper use different mesh sizes/aim drift; aim changes by ±0.25 m; each round contains three throws; HUD reports hits, throws, accuracy, round, and aim. A throw calls real scene raycast against the bucket and records hit/miss.
- SaveGame serializes round counters, aim, ledger state, and the current projectile's in-flight state. It rebinds input and HUD after load.

## Measured runs

- Direct Runtime run with `TRASH_SAVE_TEST=types`, `--frames 340`: cardboard, can, paper launched in order; three hits, zero misses, 100%, round 2. See `evidence/round-three-items.log` and `evidence/trash-three-items.bmp`.
- Save run with `TRASH_SAVE_TEST=save`, `--frames 55`: saved at 0.483 s, loaded the projectile at 0.500 s, and retained one throw / zero hits. A separate Runtime process loaded the same slot and registered the hit. See save/load logs and `trash-round-save-load.bmp`.
- `project.input`: held T then release produced one moving `Cardboard box 1`; a real left-button event using Engine key 500 at the HUD produced `Soda can 2`. A stale key=1 attempt produced no action; updated docs clarified the key contract. Root tracks the underlying regression as BUG-R2-003.
- `project.reload-input` with writable project-owned TEMP/TMP passed six before/after assertions: 0 → 1 on T → 2 on HUD click, both before and after DLL replacement.
- `project.scene`: 16 editor operations succeeded, including multi-selection group nudge, undo/redo, parenting, save/new/load, duplicate/delete. A second run created and saved all six primitive types (8 steps, every status 0).
- `gizmo.snap`, `gizmo.center`, `camera.screen-ray`, and scene raycast hit/miss were called directly and measured; logs/results are in `evidence/`.
- `asset.import` on an isolated content folder imported CesiumMan GLB (1 mesh/14016 indices) and packed BLEND (converted to GLB, 1 mesh/36 indices). No source fixture was modified.
- `material.graph` opened the real Qt material window, accepted a valid AI graph, then undo/redo/apply/revert/save. A separate locked-sidecar step deliberately failed graph save; dirty state remained set, Revert and a later Save both succeeded.
- A fresh standalone no-code project created by the root release launcher was then run through the real Windows Qt backend (`QT_QPA_PLATFORM=windows`) and D3D on an RTX 4070. `EditorUiTests.exe` exited 0 with 5028 checks. I inspected the specific role4 test paths and resulting snapshots for texture import/reimport, material save/reopen and texture slots, masked blend, a shader graph compile error, point/directional-light edits, shadow toggle, and the HUD editor/settings. Those specific feature IDs are recorded in `evidence.json`; the aggregate suite result is not applied to unrelated feature IDs. Representative outputs are `ui-material-preview.png`, `ui-material-error.png`, `ui-point-light.png`, `ui-directional-light.png`, `ui-hud-editor.png`, and the complete run log.
- Two early UI attempts used offscreen Qt or a post-run ten-object fixture, and did not count toward feature status. Details are in `issues.md`; the final clean fixture run supersedes those setup attempts.

## Artifact scope

`source.zip` contains only `Source`, `Content`, `Project.json`, and `Engine.lock.json`. It excludes SDK, Binaries, Intermediate, Runtime, and SaveData. `evidence.json` maps every feature assigned to role 4 to a status and concrete artifacts; unprobed features remain `not-tested`.

## .4 backfill: particles and rendering on an isolated game copy
- Source copy: `Engine/build/playtests/20261105-r2/ai-trash/TrashBackfill4`, created from the role4 project using final4 launcher upgrade. Frozen SDK/Runtime `.4`; source, engine lock and project manifest are recorded under `evidence/r4/`. The original `TrashGameDev` was not edited.
- Gameplay step: a successful throw (1/1, 100% accuracy) created a transient 64-particle Sparks burst at the bucket on hit. Actual D3D frames at +24 simulation ticks show the burst with the live HUD.
- Depth-collision and sorting were toggled on/off for the same hit phase. The collision pair differs by 685 pixels of 500,000, but is one simulation tick apart; sorting was measured on a frozen particle state and differs by 65 pixels. The visual effects are too small to make a strong claim, so particle-collision is partial.
- Render-mode captures use the same unchanged game/HUD: Forward (type 0), Deferred (type 1), and Deferred with G-buffer debug overlay. Three native frames were written at 1000×500; completed-frame draw calls were 33/34/38. The overlay visibly exposes four G-buffer views.
- The game called `OnWindowResized` for 640×480 and then 320×240. D3D BMP headers confirm both backbuffer dimensions, frame sequence 2/3, and HUD score remains 0/0 in both images.
- The earlier fresh native UI run completed 5028 checks. Targeted CodeClassDialog assertions now support a supplemental partial class-wizard status; the evidence names the tested validations and does not claim creation of all class kinds.
- `source.zip` now contains the .4 role4 game source/content/manifest only. It excludes binaries, SDK, Intermediate, and SaveData. Top-level role4 campaign SDK remains .1; this backfill is explicitly recorded as .4.

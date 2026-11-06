# Issues

## BUG-R3-001 — Skeletal SaveGame material-slot restore failed on .7

- Reproduction: `.7` project game uses the imported two-slot BLEND projectile, saves `round-current.save.json` while it is in flight, requests load in-process.
- Actual: `LastResult Failed message=Material slots do not match model`; runtime exits with failure. The saved component type is `Trash.MultiMaterialProjectile`, and `projectData.materialSlots` records slot0=Material_1, slot1=Material_0.
- Evidence: `evidence/dot7-save-before-fix-failed.log`, `evidence/round-current-dot7-save.json`.
- Retest: the archived exact .7 JSON loaded in a fresh .8 process; slot-bearing projectile restored and resumed to hit. See `evidence/legacy-dot7-on-dot8.stdout.log` and `evidence/dot7-save-loaded-by-dot8.png`.
- Engine fix was supplied by the coordinator in frozen .8; this role did not modify Engine sources.

## Project build issue — interface mismatch (fixed in isolated game code)

Initial compile complained `GetLocalPosition` was not available through `TrashFlight`. Added `GetFlightPosition()` to the project interface/implementations and rebuilt. Evidence is `evidence/incident-build-c2039.log` and `evidence/dot7-CodeBuild.log`.

## Project save setup issues — fixed in isolated game code

An early SaveGame entry used the old `Trash.Projectile` type and did not preserve explicit slot values; it produced `Component has no project data codec`. Corrected the type to `Trash.MultiMaterialProjectile` and assigned explicit material asset paths. The subsequent .7 failure was the engine restore-order issue documented above.

## Supplemental harness observations (not Engine defects)

- Initial `EditorUiTests` attempts exited at `EditorQtHost::Start` and initial `material.graph` attempt returned `host initialization failed`. Their TEMP/TMP values and existence were not recorded exactly at the moment of failure, so no Engine/Qt defect is claimed from those attempts. Recovery, authoring and material graph later passed when using the existing project-owned `ai-trash/Temp` path for both TEMP and TMP, `QT_QPA_PLATFORM=windows`, the correct EngineContent, frozen release context, and elevated native execution.
- `create-cpp` remains partial: this role exercised the official two-step CLI create-project then enable-code flow and build, not a GUI one-step “C++ project” template.

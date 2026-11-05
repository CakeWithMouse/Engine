> Исторический черновик до финальных UI-проверок. Актуальные результаты и устранённые ограничения — в [report.md](report.md).

# Release playtest summary — draft

Release: PainEngine `2026.11.01.5`, x64 Release / v143. Four personas tried small projects: role 1 Calm City, role 2 Curious City, role 3 Tired Bomber, role 4 AI Trash. This draft summarizes the four role diaries and feedback notes; bug totals and feature coverage remain placeholders pending coordinator reconciliation.

## What worked in the playtests

- All four personas created projects and enabled/used C++ through the project workflow. After user-code corrections, the city/economy, bomber, and trash prototypes built and ran. Each role exercised some documented runtime API beyond compile-only checks.
- Calm City confirmed its moving car visually through coordinator screenshots; its separate mesh probe logged asset validation, simplification, generated LODs, and FXAA startup.
- Curious City confirmed three house spawns, income, pause/resume HUD behavior, click selection, GameSession persistence across scenes, scene-document SaveAs/reopen, VFS read, orthographic CharacterProbe configuration, and portable export. The trace debug probe recorded a hit, five debug lines, trace statistics, and zero lines after clearing.
- Tired Bomber logged real ray hits, masks, ignored IDs, particles, session/subsystem behavior, render-mode changes, and VFS reads. The coordinator observed Play/Pause/Resume/Stop; keyboard input remains pending direct confirmation.
- AI Trash had a final coordinator-confirmed run with all three throw shapes and `Hits 3 / Throws 3`, followed by a successful packaged launch. Its SDK VFS probe resolved and read `/Game/Config/message.txt`.
- Isolated project builds that first hit `LNK1101/MSPDB140.DLL` succeeded when rerun outside the sandbox. This was recorded as a toolchain/sandbox issue; no shared Engine or SDK change was needed.

## Actual feedback and feature requests

### Shared requests / friction

- **Inspector text clipping (BUG001):** narrow fields hide their contents. Reported across roles 1–3. Improve field sizing or provide a reliable way to inspect the full value.
- **A beginner-friendly trace example:** roles 3 and 4 asked for a compact moving-target example combining channel, ignore IDs, and a readable Hit/Miss result. The repeated coding work was synchronizing movement with traces and diagnosing collisions with prior projectiles.
- **Visible runtime diagnostics:** role 3 requested Current LOD / Drawn / Culled information in the game view, plus clarification on when `GetFrameStats` becomes readable. Its ordinary `PreUpdate` observations were zero while frames rendered.
- **CharacterController feedback:** role 3 asked for a small example exposing movement speed, pressed keys, and input focus/context. Role 2 prepared a W/D position/key sampler, but manual key movement remains pending.
- **Clearer API usage around ownership:** role 2 found `Scene::AddComponent`’s required `std::unique_ptr<GameComponent>&` contract easy to get wrong with `unique_ptr<Derived>`; a short explicit conversion example would help.
- **Host service scope:** roles 2 and 3 confirmed the current VFS bridge, but noted gameplay `Game` has no direct general `getService` / `GetCapabilities` access. Arbitrary service registration and unsupported-version fallback were not proven by those probes.
- **Asset import environment:** roles 2 and 3 could not load GLB/CesiumMan because the shared Runtime reports `Assimp not enabled`; this is a build configuration limitation, not a C++ project compile failure.

### Role-specific requests and observations

- **Role 1 — Calm City:** Inspector clipping and a true edit/save/reopen of `CarMover.speed` remain open; the serialized `3.25` value is not an Inspector round-trip. The headless frozen-DLL compatibility smoke exited 1 without output, so compatibility needs the normal interactive flow.
- **Role 2 — Curious City:** show a clear CLI message when the destination project folder already exists; add a compact HUD/UIComponent example or starter HUD; expose the direct C++ project-creation checkbox for UI testing. HUD text editing required Enter before Save (BUG003); Save while the field remained active preserved the old text.
- **Role 3 — Tired Bomber:** add an in-game LOD/culling indicator; document or expose a post-render stats hook; provide a practical CharacterController status/input example. The author found the amount of custom code needed for simple collision behavior high.
- **Role 4 — AI Trash:** shorten HUD status text (fixed and visually rechecked) and filter out prior projectiles from hit traces (the coordinator rebuilt and verified 3/3 hits). Hot Reload activated a new DLL but input did not respond until reopening the project (BUG007). The role also left old import/material SDK fixtures and some UI rollback scenarios untested.

## User-code compile errors and environment/tool failures

| Role | User-code compile/validation error | Repair / classification |
|---|---|---|
| 1 — Calm City | `C3861: GetPosition` not found in `CarMover.cpp`; code used a nonexistent getter. Scene candidate also failed validation because a v2 document was labeled with versioned v3 `pointLight` data. | Replaced with documented `GetLocalPosition`; converted both point lights to the v3 property envelope. User code/content format errors. |
| 2 — Curious City | `C2664`: `Scene::AddComponent` expected `std::unique_ptr<GameComponent>&`, but the code passed `std::unique_ptr<HouseComponent>`. | Store/transfer through the base `unique_ptr<GameComponent>`. User-code ownership mismatch. |
| 3 — Tired Bomber | `C2013` from a missing `>` after `<cmath>` in the staged `ProjectGame.cpp`. A separate intentional rollback probe produced `C4430/C2146` from deliberately invalid source. | Fixed the include typo. Intentional error verified failed-build rollback and did not replace the active DLL. |
| 4 — AI Trash | `C2027`: `UIComponent` was incomplete because only `Game.h`’s forward declaration was included. Later `C2665`: string map key passed where `Trace::Query::ignore` requires numeric component IDs. Candidate validation also rejected duplicate/zero object IDs after a staging script appended IDs already present. | Added `UIComponent.h`; ignore IDs now use `GetComponentId()`; object list was rebuilt after filtering generated entries. These were user-code/staging errors. |

**Environment/tool failures, not project-code defects:** `LNK1101` / wrong `MSPDB140.DLL` version occurred in sandbox links for roles 1–4 at least once; authorized isolated-project retries outside sandbox succeeded. GLB/Skeletal import was blocked by the existing Runtime’s `Assimp not enabled` configuration for roles 2–3. Role 2’s first project-create attempt targeted an existing folder (exit 1); its direct `--create-code-project` attempt returned exit 2 because that CLI option is not present. Role 1’s first CLI invocation split a two-word name into argv entries and returned exit 2; a one-token name succeeded. Role 1’s frozen compatibility smoke exited 1 without output; no ABI defect was inferred. Role 4’s initial frozen-DLL headless smoke likewise exited 1 without a useful log.

## Remaining confirmation boundaries

- Visual claims above are limited to screenshots or observations explicitly attributed to the coordinator in role diaries. Headless startup/compile evidence is not presented as visual proof.
- Role 2’s scene-recovery attempt was blocked by an out-of-date `EditorUiTests.exe`; its working project was not used for that recovery attempt. Role 1’s compatibility prompt, role 3’s keyboard movement, role 1’s Inspector round-trip, and role 2’s W/D movement still need the specified interactive checks.
- **Bug count:** `{{BUG_COUNT_PENDING}}`
- **Feature coverage:** `{{FEATURE_COVERAGE_PENDING}}`

## Evidence packaging draft

`archive-evidence.py` is prepared in this report root but intentionally not executed. It allowlists `.log`, `.txt`, `.png`, and `.bmp` evidence, rejects files above 100 MiB, skips secret-looking names/content and dump/runtime/SDK trees, and archives explicitly listed sideprobe `Source`, `Content`, `Project.json`, and `Engine.lock.json` without Binaries/SDK/Intermediate/runtime.

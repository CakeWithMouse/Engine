# Continuation diary — tired bomber r6

Base: PainEngine `2026.11.07.6`, `d74aebb` plus r6 working fixes. Scratch projects/requests/logs/frames remain under `Engine/build/playtests/20261114-r6/tired-bomber/continuation`; role report and copied evidence are under this directory. No shared Engine source edits.

## Editor and scene workflows

- `project.scene` ran actual EditorQtHost actions for primitive creation, select/group movement, parenting, duplicate, rename/delete, Undo/Redo, New, Save/Load. The second duplicate retained its own ID, type and copied transform, accepted a separate move, and survived Save/New/Load with its source object.
- A separate New → Save As → New → Load case restored a cone at `[-4,2,7]` from its new save path while the already existing original scene file stayed byte-for-byte unchanged (same SHA-256).
- Another workflow used create/delete and Undo to restore the object. Requests/results live in `evidence/scene-*.{request,result}.json` and the specific `clone-saveas-second.*` / `scene-saveas-second.*` files.
- Startup action assigned two different saved scenes in two scratch projects; each manifest matched its selected scene. Separate recovery process pairs restored one and then two unsaved markers.
- Camera FOV set to 75 and 42; missing `/notEditable` properties after BUG-R6-004 returned status 6, `Property is not editable`, schema and data instead of a JSON exception.
- The native class wizard generated Character and Component source pairs and rejected `Bad-Name`. The import dialog cycled scale presets 1/.1/.01; accepting .025 imported/spawned a GLB with a matching sidecar, while cancel left the original scene unchanged.
- HUD editor changed/saved two strings, including an em dash. The SDK game triggered a real button callback.

## SDK game and build behavior

`SdkProbe` is an isolated C++ project built with explicit `-Runtime E:/Engine/Game/build/x64/Release/Runtime`. Its source archive contains only `Project.json`, `Source`, and `Content` (no SDK, Intermediate, or binaries). A successful game run exercised subsystem Initialize/SceneReady/Tick/Closing and lookup, a custom controller moving PlayerCharacter, an orthographic screen ray hitting the ground plane, a custom render hook receiving D3D device/context, VFS reads, GameSettings, HUD callback, WAV decode/read/seek, AudioEngine Load/Play2D/Play3D/PlayStream, and SaveGame save/reopen across processes. Save restored the subsystem tick marker. The audio calls were accepted, but hidden playback did not establish audible sound.

The project exported to a self-contained Release package. The exported `Game/Project.json` refers only to relative `Content` and `Binaries/Game.dll`; source has no `__FILE__` usage. Running the package's own `Runtime/EngineRuntime.exe` from the export directory mounted `/Game` and `/Engine` under that package, completed five GameUpdates, and stopped cleanly.

The actual offscreen `project.build` action first received an injected `#error BOMBER_ROLLBACK_PROBE`. It returned compiler C1189, `buildSucceeded:false`, `replaced:false`, kept the editor enabled, and preserved the previous module buildId and SHA-256. After repairing source, a second action returned `buildSucceeded:true`, `replaced:true`, and activated a distinct buildId and DLL hash. The earlier valid source attempt hit host MSVC `LNK1101`/MSPDB140 and also preserved the active DLL; the permitted isolated retry succeeded.

## GPU checks and limits

A custom particle D3D11 A/B with an independently chosen trajectory produced equal early hashes, different late hashes with a collision surface, and equal late hashes without a surface. The collision-on late image shows particles bouncing/collecting above the slab; collision-off falls through. The request records GPU adapter, draw/dispatch counts and images, not inferred collision state.

Custom intersecting skeletal/VAT transparency captures were rendered at 420×300 and 540×320 in Forward and Deferred; draw counts were 133/133 and 134/134. The Forward image was inspected. An animated crowd/VAT point-light capture changed shadow output as caster/light moved and removed its shadow cube when disabled. These are hidden GPU commandlet captures, not visible UI use.

## Feature status and review

`evidence.json` has entries for all 37 assigned role features plus four neighboring matrix candidates. All 37 assigned criteria are marked passed with exact evidence paths. The four neighboring entries (`scene-create-delete`, `model-import`, `trace-world`, `trace-filter`) remain partial: this role supplies a concrete case for each, while the coordinator must combine independent role evidence before changing matrix coverage. No shared matrix was edited.

Read-only review of `ParticleDiagnostics.h`, `MeshDiagnostics.h`, and their `EngineCommandlets.cpp` dispatch found no concrete incorrect counter or hidden state issue. Particle frame `steps` and `elapsed` are cumulative totals across observations, consistent with the 2,000-step total cap and documented cumulative simulation. GPU statistics are read from the completed frame. Mesh result counters come from `ValidateMesh`; SaveGame explicitly rebinds its transient material and labels that limitation. No code was changed.

All checks used hidden commandlets/runtime or non-visible project build. No visible editor usability assessment was performed.

Read-only input validation note for the coordinator: `PointShadowDiagnostics::Number` narrows JSON numeric values to float before `isfinite`, and particle/mesh/render commandlets narrow that result to `int` before range checks. A huge but finite JSON number could therefore reach an out-of-range float-to-int conversion before rejection; fractional values also truncate. Exact locations were sent to the coordinator; no source was changed and no repro was run.

## Integer validation fix

Before editing, the shipped commandlet accepted `mesh.simplify` with `slices=24.5`, returned `ok:true`, and emitted a simplified mesh. Request/result/log are saved as `evidence/integer-fraction-repro.*`.

`PointShadowDiagnostics::Integer` now validates JSON numeric type, finite double, whole-number value, and the caller-supplied inclusive bounds before converting to int. Point-shadow, particle, and mesh diagnostics now use it for all width/height/count/rendering/slices/stacks/steps fields. Added `Editor/tests/CommandletIntegerValidationTests.py` for huge `1e30` and fractional values across these fields. I did not run tests or rebuild the binary; the coordinator will rebuild and run the regression plus existing positive suites.

Coordinator follow-up: rebuilt successfully. All 13 negative integer scenarios passed,
as did Mesh 8/16 GPU frames, particles 8/32 frames and two positive point-shadow
controls. [Final evidence](../../continuation/regressions.json).

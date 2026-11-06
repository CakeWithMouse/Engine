# AI Trash — r3 run diary

## Project and game step

Created a fresh project using the launcher CLI, enabled code through the standard `--enable-code` flow, and migrated only the old role4 `Source` and `Content` into the isolated project. Imported the packed multi-material Suzanne BLEND into this project's Content. The first thrown object is `ImportedTrashProjectile`, a `SkeletalMeshComponent` with two explicit source slot assignments; other throws retain the can/paper gameplay loop.

The .7 project built and ran. The imported item is captured in flight in `trash-imported-inflight.png`. A separate three-item run reached a 1/3 score and round transition; that score is not attributed specifically to the imported throw.

## Material slot and shadow checks

`project.scene` placed the imported model, changed its two material slot paths, exercised undo/redo, and saved/reopened the scene. `material-slots-scene-final.json` is the saved scene. The mid-flight SaveGame JSON includes slot0 `Material_1.mat`, slot1 `Material_0.mat`, and the actual component type `Trash.MultiMaterialProjectile`.

For imported shadows, captured the same imported scene with `Material_0` shadow casting enabled and disabled. The pixel comparison found 667 changed pixels in the shadow region (bbox x=484..543, y=274..300). The texture is visibly assigned, but the capture is too small to certify individual UV chart features; UV remains partial.

## BUG-R3-001: save restoration

The .7 replay failed with `Material slots do not match model` when loading the saved Skeletal-derived projectile. The save file was archived before the .8 rerun. On frozen .8, a new same-process save/load passed, a separate process loaded and resumed a new slot to a hit, and the exact archived .7 failing slot loaded in a separate process and resumed to a hit. This confirms the original slot data and type now restore successfully. No Engine code was changed by this role.

## Corrections and limits

Earlier save failures caused by the project's wrong component type and empty slot assignment were game-code errors and were fixed before identifying the .7 engine restore-order failure. The initial C++ build failure was an interface mismatch (`GetLocalPosition` unavailable through `TrashFlight`); added an interface getter and rebuilt. Other role4 features not listed as passed/partial in `evidence.json` were not independently probed in this r3 run.

## Supplemental .8 workflow/reimport recovery

- Fresh no-code `Workflow` and `Authoring` projects were created with the frozen `.8` launcher. The first UI launches exited at `host.Start()` and are preserved as failed attempts. There is no reliable environment snapshot for those processes, so the failure is classified as a harness/environment issue and is not attributed to Qt or the Engine.
- With the confirmed existing native project-owned `E:\Engine\Engine\build\playtests\20261105-r3\ai-trash\Temp` assigned to both TEMP and TMP, `--recovery-check` passed exit0: it found the crash recovery record, loaded the scene through the DLL, restored exactly seven unsaved objects, and found the module build hash. The coordinator re-ran my workflow request under the same environment; its log is explicitly identified in evidence.
- My `--authoring` run passed 221 checks. It imported a red 2x2 BMP, reimported its source as blue, observed the dirty state/stale-undo reset, and moved the updated asset under Content/Images. This supports texture-import and asset-reimport.
- `material.graph` ran successfully with native preview and a locked-sidecar write failure. The failure dialog reported access denied and retained a memory backup; the graph remained dirty and identical to the edited graph, then retry restored the saved graph. Invalid AI text was rejected; undo/redo and HLSL revert passed.
- Supplemental CLI probes returned geometry for all six primitive shapes, snapped a delta to 0.5, and returned selected GameSettings values. Scene placement, angle snapping and GPU effect remain partial/unverified.


# Model import checks (r7)

## Malformed options guard

`requests/import-options-bad-object.request.json` passed the explicit options string `{`; `requests/import-options-bad-array.request.json` passed `[]`. Both actual `project.import` calls returned exit 0 with `accepted:false`, no asset result/sidecar, and an unchanged 2-object scene (`before` and `after`). Results and command logs are under the matching `results/` and `logs/` names. This independently reproduces the guarded failure path after the coordinator fix.

## Real scale preset dialog and sidecar

I authored a tiny standalone triangle OBJ at `Engine/build/playtests/20261114-r7/curious-city/sources/PresetProbe.obj` with no MTL dependencies. The request `requests/import-presets-v2.request.json` invoked the real ImportSettingsDialog, explicitly disabled `Use file unit scale`, clicked Preset1, Preset01 and Preset001, then accepted. `results/import-presets-v2.result.json` reports scale values 1, 0.1 and 0.01; the imported asset is `/Game/Models/PresetProbe/PresetProbe.obj`, its `.importsettings.json` sidecar exists and records 0.01/useFileUnitScale=false, and the actual `Engine.Model` scene object was spawned. I inspected `frames/import-options-dialog-preset-v2.png`.

The spawned transform remains `[1,1,1]` because settings format v2 bakes scale into imported geometry (`ReadSpawnScale` intentionally returns 1 for v2). The request/result proves the interactive chosen value, saved sidecar and actual import placement. No pixel-size claim is made from that Qt dialog image.

## Reimport/move

`requests/import-dialog2.request.json` used another standalone no-MTL OBJ, accepted scale 0.01, reimported from the same source and moved to `/Game/Models/CityImport/2.obj`. `results/import-dialog2.result.json` returned `ok:true`, `accepted:true`, before/spawned/reimport/move counts 2/3/4/5, and a v2 sidecar retaining 0.01 after move. The spawned Engine.Model transforms remain 1 by design as above.

## Retained setup attempts

- `import-dialog.request.json` used `moveTo` without the `.obj` extension; the importer correctly rejected the destination. The retry includes the extension and passed.
- One preset request left file-unit scaling enabled, so the preset buttons were disabled and all reported 1; this was a test setup mistake. The successful retry disables that option and observes the three distinct values.
- The initial `3.obj` preset attempt had missing MTL dependencies and move failed; replaced by the standalone fixture. These were input/fixture issues, not reported as engine defects.

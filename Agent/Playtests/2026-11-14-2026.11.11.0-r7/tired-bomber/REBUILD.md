# Rebuild — tired-bomber r7

- Engine release: `2026.11.11.0`, local release integration after main `6899a6a`.
- Original r5 upgrade source: `Engine/build/playtests/20261107-r5/tired-bomber/BomberFinalR5`.
- Private working project: `Engine/build/playtests/20261114-r7/tired-bomber/BomberUpgradedR7`.
- Current Runtime/SDK used for the final extracted-source build: `Game/build/x64/Release/Runtime`; `SDK/Lib/Engine.lib` SHA-256 `87F34A76ABF802F85264E20AF0897C85BE3B18C32D77E12DBDD91CAAE18B6527`.
- Final source package: `tired-bomber-r7-source.zip`, SHA-256 `7AB224C99B34B9A14A07B841751B9B83A9B64982CA5C7E7EEEC073264CEA35E2`. It contains 54 project/source/content/text-metadata files and no SDK, Intermediate, EngineStore, DLL, PDB, LIB, or EXP files.
- Fresh extraction: `Engine/build/playtests/20261114-r7/tired-bomber/BomberArchiveCheck`. Its included `Build.ps1 -Runtime E:/Engine/Game/build/x64/Release/Runtime` completed successfully from the extracted archive. Final buildId `145eb8972431411e8776e76663ccdf21`; Game.dll SHA-256 `5AA42C89BCB25D578D00C031A86B1B0D15E10925F2FDECBE698AE2C5567D68D0`; build log `evidence/graphics-followup-build.log`.
- Freshly rebuilt archive project SaveGame run exited0: save accepted at frame35 and completed status2/ticket1. A second Runtime process exited0 after requesting round-1 load, logging loaded elapsed0.583333/hits0/misses0 and explicit resume. Logs and save JSON are under `evidence/graphics-archive-*`.
- The archive-built Runtime independently captured the second-pass skeletal, LOD and instances probes. All three processes exited0. Measurements and image analysis are indexed by new `skeletal`, `lod`, `instances` feature entries in `evidence.json`; exact request, source, logs, screenshots and analysis are under `evidence/`.
- `evidence/archive-verification.json` records the archive/build hashes and artifact exclusion check. After the independent graphics passes, `evidence.json` has 43 passed entries and all referenced evidence paths exist.

## Independent final B graphics pass

Four additional independent rows (`material-blends`, `shadows`, `imported-shadows`, `imported-uv`) use Runtime/SDK `Game/build/x64/Release/Runtime`, SDK Engine.lib SHA-256 `87F34A76ABF802F85264E20AF0897C85BE3B18C32D77E12DBDD91CAAE18B6527`, and v12 EngineCommandlets. Forward+Deferred blend images, matched point-shadow on/off/moved/return images, imported CesiumMan receiver images (point-light shadows enabled/disabled) and a real imported-UV project frame are captured and pixel-analyzed in evidence. A clean scratch Bomber project copy was used for UV chart texture substitution; no tracked source or project archive inputs changed. Therefore the source-only archive SHA remains `7AB224C99B34B9A14A07B841751B9B83A9B64982CA5C7E7EEEC073264CEA35E2`, and no additional archive rebuild was required. Final evidence.json has 43 passed entries; all referenced evidence files exist.


### Literal replay of independent B graphics fixtures

From the repository root, use a new empty output folder:

```powershell
Set-Location E:/Engine
pwsh -NoProfile -ExecutionPolicy Bypass -File Agent/Playtests/2026-11-14-2026.11.11.0-r7/tired-bomber/evidence/replay-final-b.ps1 -EngineRoot E:/Engine -Output E:/Engine/Engine/build/playtests/20261114-r7/tired-bomber/replay-final-b
```

The replay script copies the checked-in material shaders, `.mat` files and mask image from `evidence/`, rewrites captured request destinations into the new output directory, and runs the current `EngineCommandlets.exe render.transparency` and `render.point-shadows` for Forward/Deferred blends, primitive light-shadow A/B/move/return, and imported CesiumMan light-shadow A/B/return. It then expands `tired-bomber-r7-source.zip` into a fresh `BomberUvChartProbe` project, builds it with the explicit current Runtime, copies `evidence/imported-uv-chart.png` to `Content/Models/blend-uv/Textures/BaseColor_0.png`, and runs `EngineLauncher.exe --smoke-project-current` with `TB_HUD_PROBE=1`. The extracted scene already places `UV.Control` with `blend-uv.glb` and `Material_0.mat`, whose `BaseMap` points to that texture. Finally, it runs the colocated pixel analyzer against the captured output and fails if its source-over, mask, shadow-return, or UV-channel assertions fail.

To rerun only the analyzer on the archived report frames, from any working directory:

```powershell
python E:/Engine/Agent/Playtests/2026-11-14-2026.11.11.0-r7/tired-bomber/evidence/material-shadow-uv-pixel-analysis.py
```

The imported-shadow request toggles the point light's `SetCastShadows` flag while keeping imported CesiumMan fixed; the model component's own cast flag was not toggled. This distinction is recorded in the request result and pixel-analysis JSON.


### End-to-end replay validation

Executed the documented `evidence/replay-final-b.ps1` once into a fresh private output directory, `Engine/build/playtests/20261114-r7/tired-bomber/replay-final-b-validation`. The full material and point-shadow commandlet replay, clean source-archive extraction/build, chart replacement, hidden `Start-Process -Wait -PassThru` launcher smoke, and report-local analyzer all completed with aggregate exit code `0`; the analyzer reports `assertions: all passed`. Aggregate log, explicit exit code, analyzer JSON, UV build log, and launcher stdout/stderr are copied into `evidence/replay-final-b-validation-*` and referenced by all four final B feature rows. This was a private project build only; no Engine/SDK/gameplay source or source archive changed.

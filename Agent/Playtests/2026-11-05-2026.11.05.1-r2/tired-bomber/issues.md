# Issues — tired bomber

## BUG-R2-001 — Screen-height LOD depends on camera position under orthographic projection

- Category: defect. Baseline: Runtime/SDK 2026.11.05.1, 1000×500 D3D viewport, cube at origin, screen-height thresholds `{50,20}`, orthographic view, orthoHeight 24.
- Repro: camera Y=8 then Y=24, same orthoHeight=24; then set orthoHeight=8 at Y=24.
- Expected: same LOD at both camera heights; zooming to height 8 selects higher detail.
- Baseline actual: Y8 selected LOD0, Y24 selected LOD1, and zoom remained LOD1. Reproduced; baseline binary untouched.
- Fixed SDK 2026.11.05.2 retest: Y8 and Y24 both LOD1 at orthoHeight=24; Y24/orthoHeight=8 selects LOD0. Exit 0. Distinct BMP and per-frame logs: `evidence/fixed-screenheight-y8-ortho24.bmp`, `fixed-screenheight-y24-ortho24.bmp`, `fixed-screenheight-y24-ortho8.bmp`, `fixed-lod-final.stdout.log`.

## BUG-R2-002 — Loaded disabled CharacterController still processes look input

- Category: defect. Baseline repro: disable controller, save, reload in another process, inject held W and right-button look through `Input::System`.
- Expected: character and camera position/yaw remain unchanged while disabled.
- Baseline actual: character position stayed fixed but yaw changed from -1.57079637 to -3.67079616 and camera moved. Reproduced; baseline binary untouched.
- Fixed SDK 2026.11.05.2 retest: disabled load retained pose/yaw; explicit `SetEnabled(true)` restored movement/look response. Exit 0. Evidence: `evidence/fixed-controller-before.save.json`, `fixed-controller-disabled-after.save.json`, `fixed-controller-enabled-after.save.json`, plus the two fixed controller logs.

## SDK .3 follow-up probes

- `project.scene` selected two serialized objects (IDs 1 and 2), then one other object (ID 3); runtime-only bomber units are created after editor scene loading, so they were not selection targets in this commandlet. The selection UI/tree was not driven.
- `session.lifecycle` ran two sequential scenes in one real GameSession: ready=2, closing=2, ticks=10, elapsed=0.2 seconds, clean Stop.
- A real Runtime probe called `Game::OnWindowResized` for 1000×500→640×480→320×240. `GetDisplay` and captured D3D BMP dimensions matched at each step. At 320×240 the frame advanced, while the game HUD title and LOD panel clipped/truncated. The Cyrillic probe label rendered.
- Fixed SDK .3 `Input::System.SetKey` F7/F8 sequence toggled diagnostics visibility off/on and changed the target to `Component 2/2 Bomber.Markers.LOD`. The same sequence passed in embedded Play through `project.input`. Manual keyboard usage was not claimed. SDK .2's same programmatic F7/F8 sequence had left visibility/target unchanged.
- A fixed SDK .2 Forward render-diagnostics screenshot showed one overlapping center point for three instances despite 3/3 CPU submissions. On fixed SDK .3 the actual gameplay project placed default-material instances at X=-3,0,3 with red/green/blue tints; completed-frame stats reported 3/3 and the real BMP showed three distinct colored silhouettes. `.3` Forward/Deferred/Custom D3D commandlet runs each exited 0 and generated unique 640×480 images; Deferred included the expected G-buffer debug visualization.
- Particle comparison used the same .3 game, target, and t=1.95 hit. Baseline collision+sort, collision disabled, and sorting disabled each ran 150 frames and dumped the burst at frame145. Dispatch stats were 38; baseline versus no-collision changed 2,946 pixels in the burst ROI and baseline versus no-sort changed 2,794 pixels. There is no public per-particle GPU-state readback; comparisons are visual image differences.
- Fixed SDK .3 exact ortho regression: frame59 Y24/height24 LOD1, frame109 Y8/height24 LOD1, frame149 Y24/height24 LOD1, frame189 Y24/height8 LOD0. Disabled-controller SaveGame load retained position and yaw; explicitly enabled load responded to movement/look.

## Additional tracked findings

### BUG-R2-004 — Instance fallback ignores per-instance transforms when default shader is selected

- Configuration: fixed SDK 2026.11.05.2 `render.diagnostics`, 640×480, Forward, three instances centered at X=-2,0,2, panel hidden, no explicit material or shader.
- Expected: three separate instance silhouettes.
- Actual: CPU counters reported drawn=3/3 but the screenshot rendered them as one center square. The coordinator identified the shared RotatedFigure fallback shader as not reading the instance buffer. Evidence: `evidence/api-0.2-render-forward-clean.result.json` and `api-0.2-render-forward-clean.bmp`.
- Retest: frozen SDK .3 game project, three default-material instances at X=-3,0,3 with red/green/blue colors; stats report 3/3 and separate silhouettes are visible in `api-0.3-instance-fallback-3colors.bmp`. Root has fixed the fallback. Neither Runtime nor SDK was modified by this probe.

### BUG-R2-005 — Programmatic F7/F8 input did not update runtime diagnostics on SDK .2

- Configuration: fixed SDK .2 standalone Runtime project. In game `PreUpdate`, inject `Input::System.SetKey(F7,true/false)`, then F8 true/false on separate frames.
- Expected: F7 toggles `IsRenderDiagnosticsVisible`; F8 advances the selected LOD diagnostics component.
- Actual: visibility remained true and rendered panel still selected component 1/2. Evidence: `evidence/api-0.2-resize-diagnostics.stdout.log`.
- Retest: frozen SDK .3 standalone and embedded `project.input` sequences toggled visibility false then true; F8 changed the panel line to Component 2/2. Evidence: `evidence/api-0.3-hotkeys-runtime.stdout.log`, `api-0.3-project-input-f7f8.result.json`, and `api-0.3-project-input-f7f8.stdout.log`. Manual keyboard use remains unverified.

## Scratch harness compilation error (not an engine defect)

- Repro: a PowerShell text replacement expanded the C++ `\\n` escape sequences into literal line breaks in `api-probe-copy/Source/ProjectGame.cpp`.
- Expected: well-formed string literals in the scratch probe.
- Actual: MSVC emitted C2001 newline-in-constant and follow-up C2143 syntax errors. The captured build log is `evidence/api-scratch-literal-newline-compile-failure.log`.
- The scratch harness was discarded for the clean .3 copy. Equivalent probe instrumentation was added with `apply_patch`; the .3 source then built and activated successfully. This is recorded as a probe-authoring error, not an engine issue.

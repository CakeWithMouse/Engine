# Diary — tired bomber, PainEngine 2026.11.05.1 → 2026.11.05.2

## Setup and SaveGame round

Read `Agent/AGENTS.md`, the try-engine skill, and `Engine/docs/SAVEGAME.md`, `RENDER-DIAGNOSTICS.md`, `LAUNCHER-CLI.md`, and `COMMANDLETS.md`. The run started at 28% weekly usage and last checked at 29%; no credits were reset or purchased. Created a clean starter through the launcher CLI, enabled code, and developed the isolated copy of the prior Tired Bomber project. The baseline Runtime/SDK 2026.11.05.1 remained read-only; fixed SDK 2026.11.05.2 tests used a separate upgraded copy.

The round SaveGame stores score, elapsed time, moving units, bomb, player/camera/floor references, active spark and timer, plus paused simulation state. Components added dynamically use registered project factories and reference IDs rather than raw pointers. Saved at frame 125 after a hit, then loaded in two separate Runtime processes. One remained paused; the second resumed and scored another hit. Both processes exited 0. The spark component and timer were restored and sparks rendered again.

## Fixed SDK regression and feature probes

On baseline, reproduced BUG-R2-001 (orthographic screen-height LOD changed when camera Y changed at the same orthoHeight) and BUG-R2-002 (a loaded disabled controller changed camera look). Against the separate 2026.11.05.2 copy, the exact LOD probe stayed at LOD1 for Y=8 and Y=24 with orthoHeight=24, then selected LOD0 at Y=24/orthoHeight=8. Distinct run-specific BMPs and diagnostics are saved. The loaded disabled controller retained its position, yaw, and camera; explicitly enabling it restored input response. The held-W project.input focus-loss/regain path moved once (z 5→4.2) and did not repeat until key release.

Additional fixed SDK probes exercised the CLI class generator and instantiation of generated `BomberTarget`; skeletal GLB loading and animation (19 bones, one 2-second clip); mesh validation plus meaningful dense sphere simplification (224→130 triangles, error 0); trace filters and a live debug-ray call (hit at 6.58, 23 debug lines reported); VFS host mounts/read and project writes; and completed-frame/LOD diagnostics. The class-generator CLI passed; editor wizard UI was not driven. Trace debug geometry was requested and counted, but its ray is not clearly distinguishable in the BMP, so visual confirmation remains partial.

A failed-build rollback probe ran only in scratch `rollback-copy`: an intentional syntax error made the official launcher build exit 1, while the prior module metadata and DLL hash remained unchanged; the old DLL still ran, then source was restored. The probe does not modify either SDK.

## Status and remaining gaps

See `evidence.json` for per-feature statuses and evidence links. Editor selection, multi-scene persistence, viewport resize, manual F7/F8, UI class wizard, and controlled particle collision/sorting comparisons remain untested. These are not claims that the engine lacks those features. `issues.md` records the two baseline defects and the fixed SDK retest status. `fixed-source.zip` contains source/project assets only, without SDK, DLLs, or intermediate outputs.

## Follow-up results on fixed SDK 2026.11.05.3

The later probes filled several gaps. Multi-scene lifecycle reported two `OnSceneReady` and two `OnSceneClosing` callbacks across one session. Scene selection returned IDs for an authored object group. A real game runtime resized 1000×500→640×480→320×240, matched each `GetDisplay` size to the D3D frame dump, and still advanced the frame counter; at 320×240 the existing HUD and LOD overlay truncate. The Cyrillic test label rendered at both sizes.

On .2, API-injected F7/F8 did not toggle diagnostics. On .3, the same `Input::System.SetKey` sequence toggled off/on and advanced the target to the next LOD component both in Runtime and embedded Play commandlet. I did not count this as manual keyboard testing.

The new default-material instance probe made three colored instances at X=-3/0/3. SDK .2 had counters for three submissions but rendered a single central point in its diagnostic path; SDK .3 rendered three separate silhouettes in the gameplay project. Forward, Deferred and Custom commandlet screenshots were captured on .3; Deferred also shows its G-buffer visualization.

The GPU particle comparison held the hit timing and scene constant while toggling depth collision and sorting. Each run dispatched 38 compute workgroups and captured the same burst area. The frame pixel differences support a visual effect for both settings, though there is no public per-particle GPU readback for exact position/sort assertions.

Two further engine findings are documented as BUG-R2-004 (default shader loses instance transforms) and BUG-R2-005 (F7/F8 injection on .2). The coordinator's .3 retests pass. One scratch probe build failed because my PowerShell edit expanded C++ `\\n` escapes into literal newlines; I retained the MSVC log, rebuilt clean instrumentation with `apply_patch`, and recorded it separately from engine defects.

## Second developer backfill (.3 export, SDK examples, render host)

Built a clean second copy and ran the official `Export-Project.ps1` against the frozen .3 Runtime. Validation and its three-frame GameSmoke succeeded. I then launched the exported Runtime in one process, scored a hit and saved a paused round at frame 125; a second process loaded the slot, resumed play and scored another hit. This verifies the exported content and SaveGame behavior across a restart, with the export report, logs, saved slot and hit screenshots captured.

Copied the SDK `Examples/TraceExamples.cpp` unchanged into the separate project, compiled it, and called its `WorldTrace`, `PixelTrace`, and `ScreenRay` functions on a live Runtime frame. World and center-pixel traces both hit the screen-height probe with measurable distances; the fractional screen ray returned a valid origin and direction.

For `render-host`, added a custom `MeshComponent` with an actual public `Render(ID3D11DeviceContext*)` override, called its base mesh renderer, and logged the non-null D3D context and in-progress draw counters after the indexed draw. The next `PreUpdate` read completed-frame counters, and a real frame dump shows the component. No engine defect was added from this follow-up.

The source archives now preserve `Content/Scenes/Main.scene.json` in the path expected by `Project.json`, include `README-REBUILD.md`, and are explicitly source-only: no DLL or SDK is bundled, so they require a matching SDK build before launch. Scene-crash recovery remained untested in this persona; another persona is running the two-process fixture.

Render-hook probe implementation detail: my first override was on `BomberTarget : CubeComponent` after calling `SetPrimitive`. That API uses an internal visual `MeshComponent` proxy for non-MeshComponent owners, so the owning component's `Render` counter remained zero although its proxy was visible. I replaced it with a `MeshComponent` subclass so the overridden method is the actual renderer; that version was compiled and measured in the passing evidence above. This was a harness/API integration correction, not filed as an engine defect.

## Second developer recovery backfill

Created a fresh SceneProject with the official .3 launcher CLI, confirming the template began with four objects. Ran EditorUiTests --workflow in its own Qt process: the fixture added three objects, captured an unsaved recovery snapshot at seven objects, and exited 0 via its intentional abrupt-termination path. A separate --recovery-check process found the record, loaded the snapshot, verified all seven objects and the recorded non-empty module SHA256, and exited 0. The project copy, recovery record/snapshot, and both process logs are retained under vidence/. This is the editor recovery fixture, not the Bomber game scene.

VFS feature-matrix backfill: added a dedicated fs record based on actual standalone and exported process logs. Both mounted /Engine and /Game, and the module's VFS service logged a read of /Engine/Shaders/RotatedFigure.hlsl; the shader then registered from the corresponding EngineContent. Marked partial because I did not exercise a project write through the VFS host API. SaveGame and frame-dump file outputs are not counted as VFS writes.

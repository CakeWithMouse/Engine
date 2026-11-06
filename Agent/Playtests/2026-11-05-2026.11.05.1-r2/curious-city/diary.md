# Role 2 diary — Curious City

## Setup

On 2026-11-05 I created a clean `Curious City: Small Homes` project with the baseline 2026.11.05.1 launcher CLI, then enabled C++ with the supported `--enable-code` step. The project is in `Engine/build/playtests/20261105-r2/curious-city/fresh-city`. The old release source archive was extracted into `old-copy` and left intact. To exercise its actual game code, I created a separate current-version scaffold `old-import`, imported only the archive's `Source` and `Content`, rebuilt it against baseline Runtime/SDK, and restored its startup scene to `Main.scene.json` after probes.

## Game change and real play actions

The small feature is one real house purchase. The HUD Build button creates a `HouseComponent` and calls `Scene::AddComponent`; the subsystem charges $100 and increments the house count only when AddComponent returns `Ok` or `Deferred`. The HUD displays balance, income/sec, and home count. Save and Load buttons request the `city` SaveGame slot. A derived subsystem serializes balance, house count, and income clock under `CuriousCity.Economy`; on load it clears cached HUD references and reconciles houses from the loaded scene, and the game component rebinds on BeginPlay.

Built and activated the project module using standard MSVC `Build.ps1` against the read-only baseline Runtime. The game was launched in real standalone EngineRuntime processes. Process 1 purchased a house, saved at $400/1 home, purchased another house, loaded the save, then advanced income to $410. Process 2 loaded the same slot in a distinct process and continued income from $400/1 home to $410. Logs: `first-process-cycle2.log` and `second-process-cycle2.log`; save data is under `save-slots-cycle2`.

A real EditorQtHost Play session used the root-provided `project.input` commandlet to click Build, Save, Build, and Load. The logs show deferred component addition accepted and charged, second house rolled back from loaded scene state, and HUD reference rebind. A second process loaded the same UI-created slot. Six actual Build clicks consumed $500 for five houses, then the sixth was rejected with balance $0 and house count 5. Evidence: `hud-save-load-click.log`, `hud-second-process-load.log`, `build-budget.log`, and the adjacent commandlet JSONs. GPU output `city-after-ui-button-key500-final.bmp` shows the resulting house and HUD with $400, $10/sec and one home after a single purchase; a separate PreUpdate dump is `city-house-hud.bmp`.

## Assigned feature probes

The real scene commandlet created named Civic Hall and Workshop components, transformed and parented them, group-selected/moved them, undid/redid, duplicated, saved as a new scene, reopened it, deleted a duplicate, and undid/redid deletion. All returned status 0. A second scene probe changed a custom house-height property to 2.25, saved, and reloaded it. Results are under `commandlets/project-scene-*.result.json`.

On the imported game I measured orthographic camera state (true, plane 2, height 12), Character player/camera/view-plane selection, W held movement and post-release stability, screen ray hit versus channel-filter miss, world debug trace counts and clear, `/Game/Config/message.txt` VFS reading, and Assimp GLB import. The old game reused its subsystem across scenes and earned income. Probe log: `old-import-probes.log`; input trace: `controller-w-release.log`; screen trace: `trace-screen.log`.

Completed-frame stats were sampled from PreUpdate: before the first render `valid=false`; later frames were valid with 3 draw calls, 3 visible and 0 culled. A real GPU BMP was captured with `Game::RequestFrameDump` from PreUpdate. Exported the new project with `Export-Project.ps1`; export report validated scene and standalone smoke. Both direct portable runtime and `Start.cmd --frames 3` completed with exit 0 (`CityExport/export-portable-smoke.log`, `CityExport/StartCmd-smoke.log`).

## Problems and limits

The archive has no Binaries/Game.dll, so direct upgrade cannot hash its module. Rather than treat this as an Engine defect, I built the imported source in an isolated current scaffold. Two initial compile failures were my game-code mistakes (`GetGame()` assumption and missing complete SaveGame include); both are recorded and fixed in `issues.md`. The first input probe used the wrong commandlet JSON and key value; key 1 is not LeftButton, whose public enum is 500. Root fixed the commandlet key range and key-500 click now succeeds; see BUG-R2-003 reference in `issues.md`.

Manual GUI interaction, HUD editor, pause/resume semantics, scene-object-specific editing, direct SDK example launch, and comprehensive host-service injection were not tested. The shared feature matrix was not changed. No Engine or SDK files were edited.

## Additional feature-group coverage (follow-up)

- `ENGINE_OBJECT` example: added `NeighborhoodGameObject` to an isolated project copy, built it, and used `project.scene` to create `NorthQuarter` with two child houses. Saved and reopened `NeighborhoodObjectProbe.scene.json`. All 10 operations returned status 0; serialized `projectData` is version 1 with `zoningCode=7`, `lotCount=2`. The object has no renderer by contract, and the saved/reopened hierarchy retained both parent IDs.
- HUD editor: `ui.edit-text` wrote a Cyrillic city-balance label and Russian Build button caption. Standalone game loaded the changed `CityHud.ui`; the final GPU BMP shows the Russian button caption. Game-owned balance text replaced the markup's placeholder after BeginPlay. The first longer caption exceeded 96 px and was shortened. The project's UIElement layout probe measured actual loaded markup at 640×480 and 960×540: panel `(16,16,330,206)`, Build `(28,106,96,32)` at both sizes; TopLeft placement remained stable. This is a project probe of the public UIElement layout path; it does not claim manual HUD-editor GUI coverage.
- Play state: a real character moved from z=0 to -0.768 during Playing; stayed there for 10 paused frames; after Resume and a fresh W key-down moved to -1.536; release held position; Stop restored the edit scene at z=0.
- Startup scene: changed the isolated game's startupScene to `NeighborhoodObjectProbe.scene.json` for one run. Runtime loaded two homes before the first measured game frame; GPU BMP shows both homes and HUD. Restored startupScene to Main afterward.
- SDK example: copied and compiled the pinned SDK `Examples/TraceExamples.cpp` into the isolated module, then called `WorldTrace` and `ScreenRay` from the real Game. World example hit at 10.5 on component 1; screen ray returned Success and direction `(0.632604,0.281944,-0.721332)`.
- Host services: actual project commandlets exercised viewport cursor set for clicks, Authoring setProperty for a custom property, Batch apply for selected group move, and the Reload API to replace the DLL and click Build after replacement. Other extension services were not exhaustively tested.
- The source archive was copied into this report directory as `CuriousCity-source.zip`; it contains no SDK or build tree.


## Final Runtime (.3) follow-up probes

- Imported CesiumMan.glb with the final Runtime `asset.import`: one mesh, 14,016 indices, one clip, `placed=true`. Loaded it in the game and logged frames 30–60: animation time rose 0.5→1.0 seconds, with a 19-bone palette. Captured the resident in a real 1000×500 GPU frame.
- Added QuarterSky and QuarterSun through the scene commandlet, saved before/after city scenes, then rendered both. With both additions in the after scene, 497,709 of 500,000 pixels differed by >6 summed RGB levels. This is combined evidence; it does not isolate light or shadow behavior.
- Reimported a red 2×2 test BMP after changing its source to blue. The final file pixel is `(0,0,255)` and the official authoring fixture passed 211 checks. Restoring the same city scene/camera yielded identical before/after frames (0 changed pixels); I leave rendered texture assignment partial.
- Imported the existing `blend-packed.blend` fixture to one-mesh GLB and placed it with `SceneAssets::CreateModel`; the runtime frame shows the green parcel.
- Added a project-local `CubeComponent` subclass overriding `Render`, called the base draw, and measured non-null `GetDevice`/`GetContext`, successful AddComponent, and render calls 1, 25, and 50. The same real frame shows the magenta component. No Engine or shared SDK files changed.

## Texture probe correction

The first red/blue render comparison was invalid: the scene retained the texture path but the component had no loaded SRV, so both frames were untextured. A project runtime probe measured `HasTexture=0` through frame20 with device/context ready. Reapplying the exact same path via `TrySetTexture` at frame2 returned true and made `HasTexture=1`; matched red and reimported-blue 2×2 frames then changed only the WestHome ROI (11,582 pixels). This exposed an Engine texture lifecycle issue now recorded as BUG-R2-007 candidate and reported to root. `asset-reimport` has separate pass evidence; `texture-import` remains partial until automatic scene restore is fixed.

## Final .5 independent texture fix retest

Built a separate copy against Runtime 2026.11.05.5 after removing the explicit texture setter from the probe source. With the same restored Main scene, red 2×2 control and the authoring-reimported blue 2×2 asset both reported HasTexture/SRV=1 from frame 1; the project setter remained disabled (`try_texture=0`). Two 50-frame GPU dumps differ at 11,582 pixels, only on the WestHome ROI `(618..740, 202..297)`; 11,462 red pixels became 11,582 blue pixels. The .3 pending-texture issue did not reproduce after the root's fix. A source-only .5 project archive is included separately from the main CuriousCity economy archive.

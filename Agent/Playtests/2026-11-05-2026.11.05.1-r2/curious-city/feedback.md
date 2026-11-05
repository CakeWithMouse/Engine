# Role 2 feedback — Curious City

## What worked well

- The standard launcher CLI can create a clean no-code scaffold, and `--enable-code` adds a buildable custom game module. The two-step flow is clear once the supported sequence is known.
- A project-owned subsystem plus `SerializeSaveGame` was sufficient for the city state. The actual queued save/load cycle persisted the house scene and derived balance/income state across processes; no new Engine economy system was needed.
- The UI can drive real scene mutations. A button click added a component, and payment was gated on successful `AddComponent`. The save/load buttons correctly recovered scene and economy state and rebound HUD references after load.
- The scene commandlet exercised named-object editing with move, hierarchy, group transform, undo/redo, duplicate, Save As, reload, delete, and property persistence. Results were deterministic and easy to inspect.
- `Game::RequestFrameDump` from PreUpdate and `GetCompletedFrameStats` made GPU output and render measurements available to a project probe without relying on a manual GUI screenshot.
- Runtime VFS reads, GLB import, camera screen ray, world trace/debug, controller movement, and package export were practical to test with small isolated projects.

## Friction / suggested improvements

- Document the supported CLI sequence near `--create-project` examples: create scaffold first, then `--enable-code`. A direct create-code-project route was not found in the CLI docs.
- `project.input` initially used a key range too small for `Keys::LeftButton` (enum value 500). The first click silently failed to activate the HUD; after the commandlet fix to accept 0..507, the exact same real button workflow passed. The host-provided cursor/input facts were useful in isolating this to the commandlet.
- When a source-only project archive references a missing game DLL, `--upgrade-project` reports a generic failure. A preflight message explicitly naming the missing DLL and suggesting a fresh scaffold plus Source/Content import would make this limitation much easier to diagnose.
- Rebinding UI pointers after load and reconciling scene-derived house counts needed deliberate application code. A short SaveGame example for subsystem data plus OnSaveGameLoaded/BeginPlay rebinding would help users avoid stale references.
- The EditorQtHost commandlets cover substantial scene behavior, but manual UI behavior and HUD editor interactions remain separate work; this playtest does not claim those surfaces.

## Overall

The release supports the small city-building loop with existing APIs: add one home, charge only after scene insertion, display balance/income/homes, save/load, then continue income in a second process. The user-facing value is concrete and the game logic remains in the project module.

## Follow-up findings

- `ENGINE_OBJECT` makes a useful invisible hierarchy node: a small custom codec persisted zoning/lot data while two rendered home components remained ordinary scene children through Save and Load.
- The public UI tree layout can be sampled independently at multiple viewport sizes. In this project the TopLeft HUD stays fixed at `(16,16)`; larger viewports leave more free space, while fixed-width captions still need concise copy or a wider button.
- The UI editor saved UTF-8 Cyrillic and the game loaded it on a subsequent process launch. Game-owned dynamic labels overwrite markup placeholder text during BeginPlay, so static button captions are a better test of persistent markup edits.
- During pause/resume, gameplay simulation froze as expected. A held W state did not resume movement until a fresh key-up/key-down event was sent. This behavior is recorded for follow-up rather than classified as an Engine defect.
- The SDK TraceExamples compiled in the project's actual module and produced a real scene hit plus a valid screen ray. Example source is more useful when it can be copied directly into a project's normal build with a documented call site.
- Root commandlet host probes covered cursor, authoring property edits, batch group transforms, and module reload. Other host extension callbacks remain outside this persona's measured scope.

## Final Runtime follow-up feedback

- The project workflow for custom render hooks is direct: a `CubeComponent` subclass can override `Render`, call the base implementation, and use `Game::GetDevice()` / `GetContext()`; the measured project proof produced a visible magenta cube and repeated callback counts.
- `asset.import` handled both a skeletal GLB (clip and bones visible while ticking) and the available BLEND fixture (converted model, material, and runtime placement). Keeping a compact import result plus a GPU frame makes the conversion result much easier to judge.
- The authoring UI test reimported a changed image and passed 211 assertions, but reloading the same scene produced pixel-identical frames. Asset import/reimport should document or expose the remaining material/texture assignment path so a user can verify a changed surface in-world.
- Sky and directional light both changed in one scene operation, so the measured full-frame delta cannot isolate either one. Shadow toggles and cubemap/reflection behavior still need separate probes.

## Texture lifecycle follow-up

The earlier unchanged A/B was a probe mistake because I compared scenes before actually obtaining a texture SRV. A project component probe now confirms a more precise issue: the texture asset path survives scene restore, but MeshComponent remains untextured through multiple frames despite a live device/context. Explicit TrySetTexture on the same `/Game` path immediately loads it and the matched red→blue GPU frames change only the cube's ROI. The authoring reimport path itself passes; automatic runtime restoration should be fixed and covered by a regression test.

## Final .5 confirmation

I independently rebuilt the texture probe against final Runtime .5 with the manual setter removed. Scene restore now produced a texture SRV on frame 1, and matched red/blue runs rendered the expected color change on WestHome. This resolves the prior texture lifecycle limitation for this workflow.

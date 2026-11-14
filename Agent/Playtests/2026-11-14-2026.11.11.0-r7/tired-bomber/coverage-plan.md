# Tired Bomber r7 coverage plan

Base is a private copy of the r5 `BomberFinalR5` project. Do not execute its old DLL. It is upgraded into `Engine/build/playtests/20261114-r7/tired-bomber/BomberUpgradedR7` with the .11 launcher and rebuilt against `2026.11.11.0`; pristine copy remains `BomberLegacyR5`.

## Previous gaps → this persona’s measurable case

| feature_id | Gap to close | Planned tired-bomber observation |
|---|---|---|
| `scene-create-delete` | Old cases did not pair object search/create/remove/undo with a second persona. | Create a known type, find it by name/type in scene snapshot, delete, confirm absent, undo and confirm same data/ID restored; separate object and type in a second isolated scene. |
| `scene-hierarchy` | Local pose was visible, but world transform was not computed explicitly. | Parent a child under translated/rotated/scaled parent; compare local pose and transformed world point against independently calculated expectation; save/load and repeat. |
| `scene-transform` | Only translation was fully exercised. | Apply nonuniform and negative scale, Euler rotation, translation in local/world mode; query world pose and save/reopen exact values. |
| `scene-codec` | Built-in scene JSON was tested, not a custom registered type payload. | Add a project component with versioned custom data/reference fields; write scene, reopen in a fresh process, verify type, field version, values and reference target. |
| `scene-clone` | Copy identity/data passed; reference remap did not. | Duplicate a parent+child/reference graph; assert new IDs and cloned reference targets the cloned object; save/load the clone graph. |
| `scene-select` | Snapshot selection passed, but viewport picking/marquee/focus remain unobserved. | Use actual viewport click, hierarchy row, marquee and focus action; assert selected IDs and focus camera/object after each. Requires desktop slot or a true viewport-input commandlet. |
| `time` | Delta/tick existence passed, but scaled elapsed and pause invariants were not measured. | Log ticks and accumulated simulation time at time scales 0.5/2.0; compare ratios, then pause across rendered frames and assert no simulation ticks/time. |
| `input` | Key delivery worked, but binding/action/context/subscription lifecycle was not isolated. | Bind the same physical key in two contexts with priorities; push/pop contexts, verify exactly one action event and subscription cancellation/rebind counts. |
| `camera-perspective` | Orthographic ray had evidence; perspective/viewport mapping did not. | Perspective center and off-center rays at 640×480 and 1280×720; compare origin/direction to camera basis/FOV and confirm resize preserves projection. |
| `trace-world` | A screen ray hit a plane, but direct world ray/segment cases were thin. | Cast ray and segment through known cube/plane; assert Hit/Miss, nearest distance, point and normal against geometry. |
| `trace-filter` | One ignored actor case existed. | A/B same query with include/exclude collision mask and ignored component; assert target changes only when filtered, with an unfiltered control. |
| `trace-debug` | Trace results existed without debug visualization/statistics. | Enable ray/normal visualization; render new BMP and assert counters/lines and hit normal correspond to known collider. |
| `hud-project` | UI element actions were tested, project HUD assignment was not. | Assign a `.ui` asset through actual project setting, open Game, assert loaded asset/element IDs and visible frame; remove assignment and verify no stale HUD. |
| `hud-elements` | Label/button callbacks existed; Panel/Bar/property range was incomplete. | Build Panel + Label + Bar + Button; verify text, min/max/value/clamp, click event and rendered pixels. |
| `hud-layout` | Text persistence passed, but anchor/resize/Cyrillic geometry did not. | Four-corner anchors and centered element at 640×480/1280×720 with Cyrillic text; compare expected pixel bounds and screenshots. |
| `compatibility` | Upgrade exists, but old ABI/new SDK relationship needs a real rebuild. | Preserve pristine r5 copy/hash; upgrade copy, verify lock .11, buildId/fingerprint and custom module, then smoke only rebuilt copy; compare source file hashes. |
| `render-modes` | Setting round-trip did not prove rendered Forward/Deferred frames. | Same fixed scene/camera rendered in Forward and Deferred; capture both and compare nonblack pixel coverage plus adapter/frame stats. |
| `render-resize` | Output viewport dimensions were changed in diagnostics but not full camera/backbuffer behavior. | Resize live viewport through 640×480→1280×720→800×600; assert actual backbuffer size, aspect rays and completed frame counters after each. |
| `diagnostics` | Counters were read but not tied to a known render workload. | Add/remove one cube, one instanced batch and one particle system; compare per-frame DrawCalls/Instances/Dispatches and frame dump before/after. |
| `savegame-scene` | Prior SaveGame omitted meaningful scene hierarchy/assets. | Save a two-level scene with transformed child, built-in and custom component, reload in another process and compare IDs/parent/poses/asset paths. |
| `savegame-derived` | One subsystem tick marker passed; custom derived component and subsystem values need independent checks. | Serialize a derived component versioned field and subsystem score; process restart and assert both restore plus continue updating. |
| `completed-frame` | Stats snapshot was not sampled from inside `PreUpdate`. | Log `valid/frameIndex/counters` before first render and from every `PreUpdate`; assert previous completed frame advances once and zero-size minimized frame does not fabricate. |
| `lod-panel` | LOD data existed but the actual panel/reason/resize case was not observed. | Target a known LOD component, show panel, step camera across bucket and cull thresholds, resize, assert panel text/buckets and frame screenshots. |
| `point-shadow-cubes` | One moving light shadow case did not establish two independent cubes. | Two differently placed point lights and two casters; capture/read back both cube maps, toggle each light independently and assert its own map changes. |
| `shadow-cascade-fit` | Cascade fit/stability has only partial coverage. | Sweep camera translation/orbit across texel boundaries; record cascade bounds/snap/blend and assert stable snapped bounds for sub-texel moves, changed bounds for larger move. |
| `audio-play2d` | Play2D success handle is insufficient. | Capture shared-mode WASAPI loopback around unique coded mono tone; assert matched-filter peak/correlation and RMS above baseline during Play2D. |
| `audio-play3d` | Play3D handle did not establish audible position/falloff. | Same coded source/gain/listener, run near and far emitter in independent windows; matched-filter RMS near > far by fixed threshold, baseline noise bounded. |
| `audio-stream` | Stream handle was not audible/continuous. | Capture multiple loop windows while streaming coded WAV; assert correlation in each window, then call StopStream and assert signal drops to baseline after buffer-tail window. |
| `point-shadow-lod-transition` | LOD transition was partial. | Move camera across a known screen-size boundary and back; assert selected LOD changes/restores and shadow map contains/restores caster data. |
| `transparency-camera-sorting` | No persona-independent camera/pixel oracle. | Use three alpha layers with known colors at distinct depths; capture camera order A/B and compare selected pixels against sorted source-over compositing values. |
| `transparency-intersections` | Exact intersecting/self-intersecting pixel evidence was weak. | Render crossed and self-intersecting translucent quads; orbit at fixed angles and compare overlap pixels with a CPU source-over oracle. |
| `transparency-animated-lod-resize` | Animated skeletal/VAT, LOD and resize need one sequence. | Capture skeletal and VAT casters at two camera distances around LOD switch and two viewport sizes; verify nonempty frames, expected motion silhouettes and stable translucent order. |

## AI feature cases (my assigned four)

- `ai-validation`: use `ai-fixtures/Content/AI/ValidationBroken.fsm` with node 11 `Ai.Wait.seconds=-0.5`; actual card/loader should report node 11, reject Play and preserve prior live state. Replace with `ValidationFixed.fsm` (`seconds=.5`), recheck and Play successfully.
- `ai-runtime-debug`: run tutorial Guard in a controlled scene. Query `Engine.Scene.AiMachine` `listUnits/readStates/readMemory/readHistory` at baseline, after `Noise`, after moving player into sight and after hiding; assert active path, `NoiseAt`/target memory, transition history and position changes.
- `ai-script-link`: press E on tutorial Player; require `.vscript` to set Guard `NoiseAt`, send `Noise`, enter `Investigate`, and move Guard. Separately force `OnCaught` to `Player.vscript` and assert a visible side-effect (player reset/message), not only event acceptance.
- `ai-file-reload`: open/save/reopen FSM; induce write failure and retry; change Guard `Ai.WaitRandom` interval, Play once before/after and compare active-state timing/history. Verify file bytes and changed runtime behavior.

## API / commandlet dependencies

- Parent is adding `ai.editor`, which should provide actual MainWindow/AiMachineEditorWindow actions, named widgets, scene select/property, Play/frames, AI extension snapshots, file save/reopen and write-lock/retry. Await exact request schema and binary READY.
- Current published AI extension `Engine.Scene.AiMachine` v1 is sufficient for runtime-state snapshots; current old commandlet has no AI route, so do not invoke it until rebuilt.
- For scene picking/marquee/focus and visible HLSL/HUD workflows, request a viewport-input/UI route or wait for my desktop slot.
- Audio needs a real Windows shared-mode WASAPI loopback capture; current SDK success returns are not audible-output evidence. Calm-city is preparing the observer; I will use a separate coded WAV and cases.
- All game and GPU work waits for matching r7 binaries. No old r6 commandlet/runtime will be used in r7 evidence.

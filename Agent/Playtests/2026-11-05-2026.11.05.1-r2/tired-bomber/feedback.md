# Feedback — tired bomber

The round-level SaveGame was useful for this game: after a process restart I recovered score, moving units, bomb state, references, active spark timer, and paused state, then resumed and scored again. Registering factories for dynamically created components was the main integration wrinkle; references worked once I resolved IDs after load.

The baseline exposed two concrete regressions: screen-height LOD varied with camera Y under an unchanged orthographic height, and a loaded disabled CharacterController still consumed look input. Both exact scenarios passed on the separate fixed SDK 2026.11.05.2 copy. LOD remained stable at Y=8/Y=24 with orthoHeight=24, then changed after zoom; disabled controller pose stayed unchanged, and enabling it restored input. The held-key focus-loss probe also stopped movement while unfocused.

Other real project probes showed skeletal GLB animation advancing, generated component code becoming a usable runtime instance, dense mesh simplification reducing 224 triangles to 130, and trace filter/debug APIs returning a hit. The debug-ray counter advanced, though I could not visually isolate the ray in the captured frame. The editor wizard UI and controlled particle collision/sorting comparison still need manual/targeted coverage.

The follow-up D3D probes gave me a usable picture of resize and rendering diagnostics. The game kept rendering at 640×480 and 320×240 after calling the public resize API, and the Cyrillic label remained readable. At 320×240 the HUD header and diagnostics panel are too large and clip. In the newer SDK, programmatic F7/F8 input also toggled the overlay and advanced its target; manual keyboard behavior is still unverified.

The same default-material instancing setup produced one overlapping shape on SDK .2 despite counters saying three instances. On SDK .3 I saw separate red, green, and blue shapes at the intended positions. Particle collision and sorting were compared in the same game at the same hit time, with visually different burst images when each setting changed. Scene selection and multi-scene subsystem callbacks worked through the project/editor APIs, though I did not drive the editor tree.

For the second developer pass I exported the game with the official Release export script and ran the packaged copy in two independent processes. The first process saved after a hit while paused; the second restored the round, resumed and scored another hit. This gave me an export package I could actually run, rather than only a successful validation report.

The SDK's TraceExamples.cpp also compiled and ran in the game. Both its world trace and center-pixel trace hit a visible component; its fractional screen ray returned a valid direction. A custom `MeshComponent::Render` override received a live D3D context, issued its base draw, and showed nonzero in-progress plus completed-frame counters. Scene recovery itself remains untested here.

Recovery also passed in a second developer run: a fresh launcher-created SceneProject saved seven unsaved objects to its crash-recovery record; the next process restored the recorded scene and verified the module hash. This uses the engine's isolated EditorUiTests fixture and is separate from gameplay SaveGame.

VFS: verified /Engine and /Game mounts and an actual module read of /Engine/Shaders/RotatedFigure.hlsl in standalone and exported Runtime. I left the feature partial because this run did not write through the VFS host API; SaveGame output does not establish VFS write support.

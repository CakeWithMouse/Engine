# Curious City continuation report — r6

## Result

The continuation covers all 36 features assigned to this persona. Evidence records 31 passed and 5 partial. The original r6 report, evidence, and `CuriousCity-r6-source.zip` were preserved. Work used a copy of the upgraded city under `Engine/build/playtests/20261114-r6/curious-city/continuation/city-copy`; the shared Engine, SDK, and desktop were not modified.

## New practical results

- **Project HUD and save workflow:** the UI Editor changed the build button to `New House | 100 credits`, saved it, and the newly built module still handled a click that created `House_1`. On the latest private build, the game created a house at 400 credits, saved it, created a second house after income, then loaded the saved one-house/400-credit state; income continued to 410. A fresh process loaded the same save and continued income to 410.
- **Material failures and recovery:** a bad HLSL file produced compiler diagnostics and the visible magenta/black fallback checker on the assigned imported model. Replacing it with valid unlit green HLSL produced a green model in the next process.
- **Lighting and sky:** own scene property data set a DirectionalLight intensity of 3 with shadows. The GPU frame shows a lit cube; disabling/removing the sun leaves it black. The private `ProjectGame` used public `CreateProceduralCubeMap` and `SkyboxComponent::SetCubeMap` with one scene Skybox. Space and Studio cubemaps gave different frames; `SetCubeMap(nullptr)` at game tick 4 restored the procedural gradient by frame 6.
- **Render host:** an override of the public protected Draw hooks delegated to the base implementation. The selected Forward hook ran repeatedly with non-null D3D device and context, and the gameplay frame dump completed at 640×480.
- **Mesh simplification:** an authored grounded sphere reduced from 624 to 279 triangles, remained usable with zero silhouette drift, and the deliberately ungrounded control was rejected as expected.
- **Material numeric/register boundaries:** actual material rendering accepted finite `1e32`/`1e30` parameters and returned center RGB `[255,255,64]`; `1e127` was rejected as outside finite float range and `1e999` was rejected during JSON parsing. A separate real Material Graph session accepted/applied/saved 121 uniquely named texture nodes with unique `t6..t127` bindings (never `t32`); the 122nd was rejected with maximum 121 and left the prior graph unchanged.
- **Tangent reflection fix:** retained the pre-fix image showing the negative-X V inversion. On the final Runtime, base V was `[128,128,0]`, negative X `[127,127,0]`, and negative Z `[128,127,255]`; negative-X SaveGame round-trip returned the same image hash.
- **Imported UV:** the runtime loaded a scene object referencing the imported BLEND model and generated material, but the captured frame did not show the model/texture. This remains partial rather than claiming the UV mapping was visually verified.

## Remaining partial checks

- `scene-hierarchy`: parent/local transform behavior was observed; direct world-transform readback was not.
- `scene-clone`: duplication succeeded, but no object reference remapping was present to verify.
- `scene-select`: named host selection worked, while viewport picking, marquee selection, and focus were not exercised.
- `material-slot-edit`: host property update, Undo/Redo, and persistence passed; the Inspector widget itself was not clicked.
- `imported-uv`: model and material assignment were in the live scene, but the frame did not expose the mapped texture.

`host-services` evidence records Viewport Overlay service version 3 as found and the project's `/Game/Config/city-settings.txt` read. The commandlet also reported that this VFS read used the local VFS implementation; it is not presented as a host-backed VFS test. The custom trace lines were cleared before rendering, so `render-host` is based on the actual render hook/device/context and completed frame instead of claiming a visible debug trace.

## Evidence and source archive

`evidence.json` has 36 assigned feature records with explicit expected/actual values and paths. The supplemental `CuriousCity-r6-continuation-source.zip` contains only the private project's `Source`, `Content`, and project manifests; it contains no SDK, Intermediate, Binaries, or `.painengine` state. The original source archive remains unchanged.

# Curious City r7 continuation

Runtime and version: PainEngine 2026.11.11.0, x64 Release. The coordinator owns shared Runtime/SDK/commandlet builds; no shared Engine code was changed for this role. The private city project was rebuilt against the current Release SDK.

`evidence.json` contains 39 feature entries: 32 passed and 7 partial. Every evidence path exists. Remaining partial rows (`skeletal`, `lod`, `instances`, `material-blends`, `shadows`, `imported-shadows`, `imported-uv`) are assigned to bomber for independent GPU checks; this role did not run those checks.

## Final continuation observations

- **Camera perspective:** Two fresh `project.input` processes loaded the actual `CuriousPlayer` controlled Character from `PlayerProbe.scene.json`, with that Character's `cameraSettings.fov` set to 40° and 80°. After Play/controller synchronization, the returned actual projection matrix gave `2*atan(1/P[5])` = 40.000°/80.000° at both 640×480 and 1280×720. `P[0]/P[5]` changed from 0.75 to 0.5625 with the 4:3→16:9 resize; D3D captures were verified as 640×480 and 1280×720. The initial transient `Player::SetProjection` probe was overwritten by the controller as designed; the final check uses the configured, active Character camera. Earlier `ScreenPointToRay` center/off-center/resize results and native right-button viewport look remain linked in the camera evidence entry.
- **Trace debug draw:** Paired fresh Play runs captured actual D3D frame 40. With `CURIOUS_CITY_TRACE_KEEP=1`, a 60-second trace remained visible as a green ray and hit cross; the log kept five debug lines. With the flag unset, the log went from five lines to zero and the matched frame contained no trace. Earlier centered captures remain historical diagnostics; their invisibility cause is unproven.
- **HUD layout and gameplay:** The edited top-right HUD was captured in actual D3D at 640×480 and 1280×720. Panel and label remain 16 px from the top/right edges, and the Build, Save and Load buttons stay readable and non-overlapping. A separate fresh archive run uses the normal `Main.scene.json` and intended `UI/CityHud.ui`: it rendered, bought House_1 (500→400), saved, built a second house after income, then loaded House_1 and balance 400.
- **Fresh source archive:** The final 119-entry ZIP was CRC-read, freshly extracted, and rebuilt. Normal `Main.scene.json` contains four playable objects; `SaveGameSlotProbe` remains in `SlotRenderProbe.scene.json`. The original 114-entry ZIP is preserved. Final ZIP SHA-256 is `F3EB6F293F0B7BDAD93AD1E95564D75750D97A6F25450A951389C0F5AE620E0C`.
- **Material preview:** The earlier false limitation is resolved. `material.graph previewOnly:true` produced actual native preview backbuffers at the reported preview size; saved WarmStone preset A/B produced distinct sphere pixels and frame hashes. The Qt grab is not used as evidence for D3D preview pixels.
- **Import limitation:** OBJ geometry and bounds were checked after import and manual supported-material assignment. Automatic OBJ MTL transfer is documented as unsupported in this release; malformed OBJ is rejected without a partial model folder.

## Project preservation and corrections

The r6 source project remains unchanged. `/Game` resolves to the top-level `Content` selected by `Project.json`; runtime logs in the fresh extraction confirm HUD/config files under that root. A copied nested `Content/Content` directory is not mounted. Only the r7 normal Main scene was cleaned of the test-only `SaveGameSlotProbe`; the probe scene and its material-slot evidence remain intact.

The initial camera experiment set FOV after `AfterInitialize`, then mistook the controller's later configured-camera update for a reset defect. The final test changed the controlled Character's own camera settings. The initial centered trace captures also failed to show a line; the final 60-second retained-line A/B closes the visual criterion without guessing why those earlier captures were empty.

## Rebuild artifact

`source/curious-city-r7-source-only-final.zip` is the final source-only archive. It contains project source/content and excludes `Binaries`, `Intermediate`, `.painengine`, EngineStore, SDK, and Runtime. `REBUILD.md` has the fresh extraction, current SDK build, and clean Main gameplay/save/load commands. Fresh build/run files and the archive hash are under `evidence/source-archive-final/`.

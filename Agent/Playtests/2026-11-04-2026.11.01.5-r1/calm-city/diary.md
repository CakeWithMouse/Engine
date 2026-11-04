# Calm city playtest diary

## Target
Release `2026.11.01.5`, role 1. Project path: `Engine/build/playtests/20261104-r1/calm-city/CalmCityProject`. Runtime: `Game/build/x64/Release/Runtime` (Release x64, SDK toolset v143). Source-only archive: `calm-city-source.zip` in this report folder.

## Preparation and design
- Read `Agent/Skills/try-engine/SKILL.md`, `Agent/AGENTS.md`, `Agent/engine-developer.md`, current `Engine/Version.json`, CODE-PROJECTS, USER-PROJECTS, API Reference and tracker 027/029.
- Verified API facts: `MeshPrimitives::Plane` is XZ/+Y; transforms are local; `SetParent` checks validity/cycles; `GameComponent::Tick(float)` uses simulation delta and derived overrides call their immediate base; `GetLocalPosition` is the documented position getter. Point lights have no geometry or mesh shadow.
- Created project through `EngineLauncher.exe --create-project` and enabled C++ with the SDK's `ProjectTools.ps1 -Action Enable`.
- Built a small block: existing 20×20 ground, six differently sized blue blocks, a dark street with two lane stripes, a red moving car and blue roof plus four cylinder wheels parented to the car. Car speed/moving fields use the generated project property codec. `CarMover::Tick` travels between X=-5.6 and X=+5.6 and reverses direction.
- Starter lights were migrated from scene format v2 to the documented v3 `properties.Engine.PointLight` form. Scene saved to `Content/Scenes/Main.scene.json`; `Project.json` points to it as startup.

## Run log and diagnostics
- First CLI call with a two-word name returned exit 2 because I passed the name as two argv entries; no project was created. Retried with one name token (`CalmCity`) using the repository's `tests/code-projects/run.ps1` argument pattern; exit 0.
- C++ enable completed; `Engine.lock.json` pins Release `2026.11.01.5`; isolated store is `Engine/build/playtests/20261104-r1/calm-city/EngineStore` via `PAINENGINE_ENGINE_STORE`.
- First compile error was my code: `CarMover.cpp(16,21): error C3861: GetPosition: identifier not found`. I used API Reference's `GetLocalPosition` and rebuilt.
- Sandbox link then returned `LNK1101: incorrect version of MSPDB140.DLL` while writing the PDB. The coordinator authorized one external project build; that completed, so no Engine/SDK change was required.
- Candidate validation caught my scene format mistake: `Candidate scene rejected: v2 uses versioned properties, not pointLight`. I had raised the starter scene marker without converting its point-light data. Converted both lights to the v3 property envelope and rebuilt.
- Final Release build ID `b53bf9d623d9478e89353842ce057bed` compiled the project DLL, validated candidate, ran three startup frames and activated the module. Evidence: `.../Binaries/Release/b53bf9d623d9478e89353842ce057bed/build.log`, `validate.log`, `startup.log`; `Project.json` and `Project.code.json` now name this build.
- Launcher `--smoke-project` exited 0; launcher `--smoke-project-editor` exited 0. The editor smoke reopened the persisted startup scene. Logs: `Engine/build/playtests/20261104-r1/calm-city/smoke-game.*.log` and `smoke-editor.*.log`.

## Reproduction
From PowerShell, with `PAINENGINE_ENGINE_STORE` set to the project-local `EngineStore`:

```powershell
& 'Game/build/x64/Release/EngineLauncher.exe' --smoke-project 'Engine/build/playtests/20261104-r1/calm-city/CalmCityProject/Project.json'
& 'Game/build/x64/Release/EngineLauncher.exe' --smoke-project-editor 'Engine/build/playtests/20261104-r1/calm-city/CalmCityProject/Project.json'
```

For a visible check, open the project in the editor, frame the block bounds x≈[-7,7], z≈[-6,6], target around (0,1.8,0), with a high three-quarter view from roughly (14,12,-18), then press Play. The expected effect is the red car moving along the dark X-axis road and reversing at both ends. The C++ startup smoke is three frames and does not establish this visual/gameplay effect.

## Evidence limit
No visual screenshot was made in this role. Visual appearance, actual movement over time, property editing, and scene save through editor UI remain `visual-unverified` until the coordinator captures/observes the UI. The saved JSON was authored directly in the project file, then loaded through editor smoke; do not interpret that as a manual editor save test.

## Isolated render/mesh probe
- Created `Engine/build/playtests/20261104-r1/calm-city/CalmCityProbe` as a separate C++ project; it has a custom instanced mesh component with nine differently tinted vehicle instances. It sets two automatic LOD thresholds, screen-height LOD thresholds, LOD debug colors, shadow LOD bias, draw distance and asset validation; game startup selects Forward rendering, disables VSync and requests FXAA.
- Release build ID `e708dc254632461baa7aaec84f6d0b48` built, validated and activated. `EngineLauncher.exe --smoke-project .../CalmCityProbe/Project.json` exited 0. Logs are in `.../CalmCityProbe/Binaries/Release/e708dc254632461baa7aaec84f6d0b48/`.
- Runtime evidence in `startup.log`: asset check reported `344 triangles, 392 vertices, size 1.72 x 1.12 x 2.60 m, pivot y 0.00, uv yes, ok`; simplification at 0.55 ratio returned 188 triangles; automatic LOD generation produced LOD1 `344 -> 188` (target 189) and LOD2 `344 -> 86` (target 86), each with `0%` silhouette drift. Startup also logged `[Info] FXAA pipeline and shaders compiled successfully` and completed three game frames.
- This is runtime/console evidence for mesh validation, simplification, generated LODs and render pipeline initialization. No screenshot was captured for this isolated probe; do not claim its mesh instances or LOD color visualization were visually inspected.

## Coordinator visual confirmation
- The coordinator opened the main CalmCityProject in the UI, corrected its startup camera and added a directional sun. Their later build `7898...` showed the block, shadows and moving car in screenshots `Engine/build/playtests/20261104-r1/ui-evidence/calm-game-a.png`, `calm-game-b.png`, and `calm-game-c.png`. This supersedes the earlier visual-unverified note for the main city scene only; the separate CalmCityProbe has no UI image evidence.
- The coordinator confirmed the generated `CarMover` speed value `3.25` in the saved scene, but has not reported an Inspector edit/save/reopen round-trip. They also noted that narrow Inspector fields clip their contents (BUG001, observed across roles 1–3).

## Particle depth-collision API probe
- Added a custom component derived from `ParticleSystemComponent` in CalmCityProbe, configured 128 GPU particles, lifetime/velocity/acceleration, depth collision, collision response, sorting and simulation rate through the documented public setters.
- Build `e9a04ad5f5a94415996697338812ec8c` compiled, validated, activated; launcher `--smoke-project` exited 0 and completed three game frames. This confirms the component/API integration compiles and starts. The smoke log does not report individual GPU collision events, so depth collision behavior remains unverified visually.

## Frozen compatibility attempt
- Extracted the checked-in `tests/compatibility/fixtures/lights.zip` inside `FrozenCompatibility`; verified all seven packaged files against SHA256SUMS.txt. Copied the frozen DLL/dependencies into a temporary project; its old `Engine.lock.json` pins `2026.10.24.0`. `--check-project` exited 0. Default `--smoke-project` / direct `EngineRuntime --project` do not launch because the project is pinned to another release; the direct runtime emitted `LaunchError: Project is pinned to another engine release. Open it through EngineLauncher.`
- Ran the documented headless route `EngineLauncher --smoke-project-current` to force the current runtime's compatible-engine path. It exited 0; the copied legacy `Game.dll` remained byte-identical at SHA256 `59E5932B978BC8200CACAC806B63CA1307962C1D548613C9A2C6B14B301637EF`. This confirms current-runtime load/start/three-frame smoke for the frozen lights fixture; it does not cover interactive old-runtime selection, project-upgrade prompt, or the full CompatibilityTests harness. Result and hash log: `Engine/build/playtests/20261104-r1/calm-city/FrozenCompatibility/result.txt`, `sha256-verified.txt`.

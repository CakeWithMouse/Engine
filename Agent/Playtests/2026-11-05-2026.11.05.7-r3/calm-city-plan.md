# Calm City r3 preparation

## Scope and source
- Persona: calm city designer; preserve the compact neighborhood, moving cars and saved route from r2.
- Isolated source staging: `Engine/build/playtests/20261105-r3/calm-city/source-stage` copied from `Agent/Playtests/2026-11-05-2026.11.05.1-r2/calm-city/calm-city-source.zip` (source and Content only for transfer; historical files untouched).
- Target project after READY: a fresh CLI project under this role directory, created with `--create-project`, then `--enable-code`; copy the staged Source/Content over generated template while keeping fresh SDK lock/build metadata. Use the frozen .7 Runtime and a role-local EngineStore/Intermediate.

## Small gameplay step
Keep the existing block layout and car route. Add one imported multi-material building and one multi-material car asset if available in the shipped/importer fixture set; verify submesh colors/material assignments in actual D3D pixels. Enable directional shadowing and verify cast/receive on the imported silhouettes and ground. Preserve the route and scene through save, process exit, and reopen; compare object/material/position state and screenshots.

## Planned checks after READY
1. Fresh CLI create and enable-code, preserving logs and exit codes. Transfer old source into fresh project, build against frozen .7, then run the real project.
2. Import assets with multiple material slots; inspect per-slot materials and rendered pixels. Edit one slot, undo/redo where available, save scene and project, reopen in a fresh process.
3. Capture a fixed camera/frame before and after car movement; document viewport, camera and render settings. Capture BMP/PNG evidence for imported materials and shadows.
4. Save/load car route and scene in separate processes; confirm the expected coordinates/material assignments after reopen.
5. Cover only assigned matrix rows with direct evidence. Mark any unavailable UI/editor-only path partial or not-tested; do not infer broad coverage.

## Deferred
No project creation, build, smoke, runtime, UI, or rendering command has been run in this preparation phase. Awaiting coordinator's frozen .7 Runtime and READY.

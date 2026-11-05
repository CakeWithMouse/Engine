# Calm City SaveGame playtest — 2026-11-05

## Scope and project copy

Used the existing Calm City sources as an isolated copy. The previous project under `Engine/build/playtests/20261104-r1/calm-city/CalmCityProject` and `calm-city-source.zip` were not edited. The initial copy is `Engine/build/playtests/savegame-2026-11-05/calm-city/CalmCityProject`; the supported Runtime upgrade produced the final copy at `Engine/build/playtests/savegame-2026-11-05/calm-city/CalmCityProject-upgraded`.

The final copy is pinned to Runtime/SDK `2026.11.05.0`. `ProjectGame::AfterInitialize` configures SaveGame once with ID `calm-city` and a project-local absolute root. `CarMover` calls its parent serializer, then saves `CalmCar` version 1 fields `speed`, `moving`, and `goingRight`. The base serializer saves the component transform/position. The component binds F6 to save and F9 to load slot `manual-1`; F5 is reserved by engine Forward/Deferred handling. This sample has no object references to serialize.

## Build and migration

- Installed Runtime `2026.11.05.0` only into the playtest scratch EngineStore; no user library was changed.
- The first launcher `--upgrade-project` attempt exited 1 with the generic message that project copy upgrade failed. Direct Runtime upgrade initially exited `-1073741515` (`0xC0000135`), due to a missing Assimp runtime dependency.
- The coordinator traced the missing dependency to Runtime deployment being undone by an EditorModule/Commandlet build. After the shared Assimp runtime deployment fix, direct `EngineRuntime --upgrade-project <source> --destination <new-copy>` exited 0 and reported that the copy was upgraded while the original was preserved. This is recorded as `BUG-SAVE-001` by the coordinator.
- A first sandbox-local link attempt failed with `LNK1101: incorrect version of MSPDB140.DLL`; classified environment limitation. Per the authorized project-only build path, subsequent project builds used `require_escalated` and completed. A candidate made during the incomplete SDK sync also crashed during startup (`0xC0000005`) after shader initialization; the old project module ran successfully, and the failure disappeared after the coordinator completed the matching header/Engine.lib SDK sync. Classified integration/environment during SDK refresh, not a SaveGame API defect.
- Final command: `& '.../CalmCityProject-upgraded/Build.ps1' -Runtime 'E:/Engine/Game/build/x64/Release/Runtime'`. It compiled, linked, validated, ran startup smoke and activated build ID `31163a5972644095b82166d68dcd2776`.
- Direct Runtime validation command `EngineRuntime.exe --validate-project Project.json` exited 0 (`runtime-validate-final.stdout.log`). No Engine/Editor build was run by this role.

## Save, advance, load, and process restart

The project has a test-only `CALM_SAVE_TEST` mode. It freezes simulation time only in this mode and advances the car explicitly, leaving normal gameplay timing unchanged. This avoids frame-time-dependent assertions.

1. Process one, `CALM_SAVE_TEST=save`, `EngineRuntime.exe --project <Project.json> --mode game --frames 7`: exit 0. It moves the car to x=5.6, where route direction is left, requests a save, advances to x=3.1, then loads the slot and restores x=5.6. Another 0.1-second car tick continues left to x=5.35. Log: `process=save result=loaded loadedX=5.600000 continuedX=5.350000 PASS`.
2. Process two, new Runtime process, `CALM_SAVE_TEST=load`, same project and slot, `--frames 7`: exit 0. It loads x=5.6 from disk and continues to x=5.35 in the saved leftbound direction. Log: `process=new result=loaded loadedX=5.600000 continuedX=5.350000 PASS`.
3. Save file: `CalmCityProject-upgraded/SaveData/calm-city/default/manual-1.save.json`. Both process logs and full Runtime output are in this playtest's `Engine/build/playtests/savegame-2026-11-05/calm-city` folder.

This verifies Save → advance → Load → continue in one process and loading/continuing the same slot from a separate process. Both processes exited 0.

## Visual and UX evidence

The test requests a frame dump after several post-load frames. Both final dumps are 1000×500 BMPs:

- `.../CalmCityProject-upgraded/SaveData/save-load-same-process.bmp`
- `.../CalmCityProject-upgraded/SaveData/save-load-new-process.bmp`

Both have SHA-256 `F1A8B3775346D0B9DA24D36F39A43A325B798E39923C0361EFC25EBDDBC4DB7F`, each contains 74,175 non-dark pixels, and the decoded preview shows the city block, buildings, road, and car. The test uses FrameDump output; it is not an editor UI capture.

Manual F6/F9 UI input remains unverified. I opened the editor through Launcher, but the CUA `getApp` request timed out with `Computer Use app approval timed out`; per instruction I stopped UI attempts. The test-only headless path verified the same SaveGame service and persistent slot. Sample controls are F6 save and F9 load; there is no visible in-game status message for Accepted/Saved/Loaded/Failed. Wishlist: document reserved engine hotkeys and provide a shared input binding configuration. F5 is already used by the engine.

## Error classification

- `BUG-SAVE-001` — Runtime dependency deployment defect, reproduced as project upgrade failure and `0xC0000135`; coordinator traced and fixed deployment. After fix, upgrade and Runtime validation succeed.
- `LNK1101 MSPDB140.DLL` — sandbox/toolchain environment limitation; project build succeeded through the authorized external SDK path.
- Startup access violation during mixed SDK staging — integration/environment error during coordinator SDK refresh; final synchronized project candidate built and passed startup.
- No SaveGame serialization, save/load, route restoration, or cross-process persistence defect reproduced in the final build.
- Manual keyboard/UI interaction is unverified because of CUA timeout.

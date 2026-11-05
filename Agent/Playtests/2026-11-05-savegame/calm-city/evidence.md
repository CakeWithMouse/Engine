# Compact evidence — Calm City SaveGame

- Runtime/SDK: `2026.11.05.0`, Release x64. Final game build ID: `31163a5972644095b82166d68dcd2776`.
- Project `Build.ps1 -Runtime E:/Engine/Game/build/x64/Release/Runtime`: exit 0; candidate validated, 3-frame startup passed, activated.
- `EngineRuntime.exe --validate-project <CalmCityProject-upgraded/Project.json>`: exit 0.
- Process 1 (`CALM_SAVE_TEST=save`, `--mode game --frames 7`): exit 0; requested save at x=5.6, advanced to x=3.1, loaded x=5.6, continued left to x=5.35; PASS.
- Process 2 (new process, `CALM_SAVE_TEST=load`, same project/slot, `--frames 7`): exit 0; loaded x=5.6 from `SaveData/calm-city/default/manual-1.save.json`, continued left to x=5.35; PASS.
- Log: `SaveGameSelfTest.log`; complete stdout/stderr are in `Engine/build/playtests/savegame-2026-11-05/calm-city`.
- Same-process and new-process FrameDump BMPs: 1000×500, SHA-256 `F1A8B3775346D0B9DA24D36F39A43A325B798E39923C0361EFC25EBDDBC4DB7F` each; 74,175 non-dark pixels each; preview shows city.
- Manual F6/F9 UI input not verified; CUA request timed out. No additional UI attempt.
- `BUG-SAVE-001`: Runtime Assimp dependency deployment omission caused initial project upgrade failure (`0xC0000135`); coordinator fixed deployment. Initial sandbox link `LNK1101` and mixed-SDK startup AV did not reproduce after authorized project build and SDK sync.

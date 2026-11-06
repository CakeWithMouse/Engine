# Rebuild / replay

1. Use `Engine/build/playtests/20261105-r3/ai-trash/Final8Trash` as the upgraded isolated project. It was created with frozen `.8` launcher `--upgrade-project` from the role's own `.7` project; the original `.7` project remains preserved at `.../ai-trash/TrashGame`.
2. To rebuild using the frozen `.8` launcher, run `EngineLauncher.exe --build-project ".../ai-trash/Final8Trash/Project.json"` with MSVC elevation if required by the environment. The game source and imported BLEND content are in the project.
3. From PowerShell, set project-owned `PAINENGINE_ENGINE_STORE`, `TMP`, and `TEMP`, then set `TRASH_SAVE_TEST=save` to create/save/load in-process, or `TRASH_SAVE_TEST=load` to load `round-current` in a fresh process. Run the frozen `.8 Runtime/EngineRuntime.exe --project ".../Final8Trash/Project.json" --mode game --frames 120`.
4. The archived pre-fix `.7` slot is `evidence/round-current-dot7-save.json`. To replay the compatibility test, copy it into `Final8Trash/SaveData/ai-trash/default/round-current.save.json` before launching with `TRASH_SAVE_TEST=load`.
5. PNG captures and compact result logs referenced by `evidence.json` are in this report's `evidence/` folder. Raw frames remain in the isolated build project.

## Supplemental native UI/commandlet probes

The successful native Qt runs used the current x64 Release UI runner and an existing writable temp/store:

```powershell
$env:PAINENGINE_ENGINE_STORE = 'E:\Engine\Engine\build\playtests\20261105-r3\ai-trash\EngineStore'
$env:TEMP = 'E:\Engine\Engine\build\playtests\20261105-r3\ai-trash\Temp'
$env:TMP = $env:TEMP
$env:QT_QPA_PLATFORM = 'windows'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& "$runtime/EditorUiTests.exe" 'E:/Engine/Engine/build/playtests/20261105-r3/ai-trash/Supplemental8/Workflow' "$runtime/EngineContent" --workflow
& "$runtime/EditorUiTests.exe" 'E:/Engine/Engine/build/playtests/20261105-r3/ai-trash/Supplemental8/Workflow' "$runtime/EngineContent" --recovery-check
& "$runtime/EditorUiTests.exe" 'E:/Engine/Engine/build/playtests/20261105-r3/ai-trash/Supplemental8/Authoring' "$runtime/EngineContent" --authoring
```

The `material.graph` locked-sidecar write-failure request/result and render capture are in `evidence/material-graph-writefailure.*`; commandlet request used the isolated `Supplemental8/MaterialGraph` copy. The six `mesh.primitive` requests and result summary, `gizmo.snap`, and `game.settings` request/results are also retained under `evidence/`.

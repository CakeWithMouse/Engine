# REBUILD — AI Trash r5

Исходный проект r3: [ai-trash-r3-source.zip](ai-trash-r3-source.zip). Архив результата r5: [ai-trash-r5-source.zip](ai-trash-r5-source.zip), только `Source`, `Content`, `Project.json`, `Engine.lock.json`, `Project.code.json`, `Build.ps1`.

Runtime/SDK: `2026.11.07.0`, x64 Release. Проверенные команды из PowerShell (команды сборки запускались с отдельным writable EngineStore/TEMP и вне sandbox из-за MSPDB ограничителя):

```powershell
EngineLauncher.exe --create-project "$root/TrashGame" "AI Trash r5"
EngineLauncher.exe --enable-code "$root/TrashGame/Project.json"
# Скопировать Source и Content из ImportedSource; сохранить свежие Project.json, Engine.lock.json.
EngineLauncher.exe --build-project "$root/TrashGame/Project.json"
$env:TRASH_SAVE_TEST='save'; EngineRuntime.exe --project "$root/TrashGame/Project.json" --mode game --frames 180
$env:TRASH_SAVE_TEST='load'; EngineRuntime.exe --project "$root/TrashGame/Project.json" --mode game --frames 180
EngineCommandlets.exe mesh.tangents normal-request.json normal-result.json
EngineCommandlets.exe mesh.tangents mirror-request.json mirror-result.json
EngineCommandlets.exe mesh.tangents degenerate-request.json degenerate-result.json
```

Первый build после source-copy пропустил старый по timestamp ProjectGame.cpp и завершился unresolved symbols; исходный лог сохранён как `evidence/initial-link-failure.log`. Повтор после обновления timestamp исходников внутри проекта прошёл; полный успешный build — `evidence/rebuild-success.log`.

Межпроцессный roundtrip и hit: [RoundSaveTest.log](evidence/RoundSaveTest.log), `save.stdout.txt`, `load.stdout.txt`. Кадр полёта — [trash-round-save-load.bmp](evidence/trash-round-save-load.bmp), 1000×500. Tangent inputs/results/logs — соседние файлы `*-request.json`, `*-result.json`, `*.stdout.txt` в evidence. Export и GPU tangent visual test не запускались.

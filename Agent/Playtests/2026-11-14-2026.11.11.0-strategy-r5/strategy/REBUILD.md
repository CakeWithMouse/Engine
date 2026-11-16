# Пересборка и запуск r5

## Архив и SDK

Исходный архив `RepairCorridor-r5-source.zip`: SHA256 `8AF67B7BD0B20F24830436187B5786C66B5AA8EAC498075626E6344AD85A7C03`, 18 файлов проекта, 17,884 байта. Архив содержит `Game.sln`, `Game.vcxproj`, README, Content и исходники; 18/18 записей совпадают с рабочими исходниками. Binaries, SDK, Intermediate и SaveData отсутствуют. Аудит: `r5-final-zip-audit.json`. Архив до STRATEGY-020 сохранён как `RepairCorridor-r5-source-pre020.zip` (SHA256 `C8D93F47125C17330676F5968351DB7559BE5AA02907F11F56C89098EEACBD75`).

Требуется локальная, неопубликованная сборка PainEngine:

- Версия `2026.11.11.0` (localOnly), база `a5468726` с AI Diagnostics `0947535b`.
- Runtime: `E:\Engine\Game\build\x64\Release\Runtime`.
- Библиотека: `Runtime\SDK\Lib\Engine.lib`.
- SHA256: `4202492B1EFB832315DA1CD86281B9A540B5D3641011AB33BB76D6D1851B3FC2`.
- Исправления: `STRATEGY-019` (AUTO radius группы), `STRATEGY-020` (проекция групповых slot и восстановление пути к клетке, допустимой NavGrid).

Опубликованная сборка с той же версией может не содержать этих исправлений. Сверьте SHA SDK перед сборкой.

## Сборка ZIP

Распакуйте ZIP в новый каталог и передайте абсолютный путь `Project.json`:

```powershell
$project = 'E:\Games\RepairCorridor\Project.json'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$env:PAINENGINE_ENGINE_STORE = 'E:\Games\RepairCorridor\EngineStore'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Rebuild -Project $project -Runtime $runtime
Remove-Item Env:PAINENGINE_ENGINE_STORE
```

Успешная сборка пишет `Build validated and activated` и задаёт актуальный `Project.json` module. Финальная проверенная распаковка находится в `E:\Engine\Engine\build\playtests\strategy-r5\fresh-final-sdk020b\RepairCorridor`; buildId `dfc14c308aae4201bee2ce749c742837`, DLL `Binaries/Release/dfc14c308aae4201bee2ce749c742837/Game.dll`. Логи: `r5-final-clean-build.log` и `.status`.

## Проверенный реальный Play и AI Diagnostics

`project.input` вызывает настоящую игровую DLL в Play и передаёт key/cursor/frame события; формат описан в `Engine/docs/COMMANDLETS.md`. Финальный запрос — `r5-final-clean-play-request.json`. Он выбирает группу рамкой, сдвигает физический контейнер, создаёт приказ в линию, удерживает F ровно для переключения следующего хода, выдаёт новый приказ в колонну, затем ставит игру на паузу для запроса AI Diagnostics. Повтор с собственной распаковкой:

```powershell
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$project = 'E:\Games\RepairCorridor\Project.json'
$request = Get-Content 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-strategy-r5\strategy\r5-final-clean-play-request.json' -Raw -Encoding UTF8 | ConvertFrom-Json
$request.project = $project
$requestPath = Join-Path $env:TEMP 'repair-corridor-r5-request.json'
$resultPath = Join-Path $env:TEMP 'repair-corridor-r5-result.json'
$request | ConvertTo-Json -Depth 40 | Set-Content $requestPath -Encoding UTF8
$env:PAINENGINE_ENGINE_STORE = 'E:\Games\RepairCorridor\EngineStore'
$env:PAINENGINE_STRATEGY_TELEMETRY = Join-Path $env:TEMP 'repair-corridor-r5-telemetry.log'
& "$runtime\EngineCommandlets.exe" project.input $requestPath $resultPath
Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY
Remove-Item Env:PAINENGINE_ENGINE_STORE
```

Финальный запуск завершился exit 0. Группа сообщила `Arrived`, `AUTO radius=0.40000000596`, поле `ready`, A→B `found`, clearance шлюза `0.875`, `navConnectedSelected=3`, `barrierViolationFrames=0`. У S2 position `(-0.791471,-0.971346)` и slot `(-0.875,-0.925)`. Охранник успел пленить отряд; этот кадр доказывает пройденный приказ, не победу. Финальные телеметрия/результат и просмотренный D3D кадр: `r5-final-clean-play-telemetry.log`, `r5-final-clean-play-result.json`, `r5-final-clean-play-arrived.bmp`.

D3D кадр сохранялся viewport hook-ом в `Engine::RenderViewportFrame`; отдельный commandlet request может задать `PAINENGINE_D3D_SCREENSHOT` и `PAINENGINE_D3D_SCREENSHOT_FRAME` с новым абсолютным именем BMP. AI Diagnostics Qt panel на собственном проекте вручную не проверялась: screenshot-only CUA зависал в STRATEGY-018. Зато на проекте вызвано настоящее optional API через `project.input diagnostics`, его реальные слои были нарисованы поверх D3D игры и кадр осмотрен. Корневой UI/API test выполнен отдельно.

## Standalone smoke

Проверена финальная чистая распаковка отдельным настоящим Runtime:

```powershell
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime\EngineRuntime.exe'
$project = 'E:\Engine\Engine\build\playtests\strategy-r5\fresh-final-sdk020b\RepairCorridor\Project.json'
& $runtime --project $project --mode game --frames 60
```

Результат: exit 0, `GameUpdates=60`, `EngineStopped`. Данные: `r5-final-runtime60.stdout.log`, `r5-final-runtime60.stderr.log`, `.exit`.

## Ручное управление

В GUI откройте `Project.json`, соберите Game, нажмите Play. Edit сцена пустая; процедурная карта создаётся игровым bootstrap только при Play. ЛКМ/протяжка выбирают, Shift добавляет или переключает, ПКМ перемещает/атакует, E толкает контейнер, F задаёт строй следующего перемещения. HUD объясняет эти команды.

# REBUILD и запуск «Ремонтного коридора»

Архив `RepairCorridor-source.zip` содержит только C++ исходники, Content, метаданные проекта и build script. Visual Studio Build Tools устанавливаются отдельно. Для корректной игровой физики нужен именно исправленный локальный Runtime: `E:\Engine\Game\build\x64\Release\Runtime`, Engine 2026.11.11.0 Release x64, база `388d7e9` плюс локальная фиксация `STRATEGY-003`. SHA256 `Runtime/SDK/Engine.lib`: `6846D88DEE3BAF892E3A4A06C1C965E7FC71966B78A1E5620972178FE7EE0C5D`. Исправленный SDK не является опубликованным артефактом версии: публичный SDK 2026.11.11.0 содержит дефект плотности и не воспроизведёт физический исход. При переносе используйте этот исправленный Runtime либо эквивалентно собранный SDK.

## Сборка

Распакуйте архив в новую папку, например `E:\Games\RepairCorridor`. В PowerShell укажите путь к установленному Runtime той же версии:

```powershell
$Project = 'E:\Games\RepairCorridor'
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime' # локальный SDK STRATEGY-003
& "$Runtime\SDK\ProjectTools.ps1" -Action Build -Rebuild `
  -Project "$Project\Project.json" -Runtime $Runtime
```

Нужны MSVC v143 (проверено на 14.44.35207) и Windows SDK 10.0.26100.0. Успешный build генерирует и активирует `Game.dll` в `Binaries` проекта.

## Самостоятельная игра

Запустите Runtime в game mode с окном/интерактивной сессией Windows:

```powershell
& "$Runtime\EngineRuntime.exe" --project "$Project\Project.json" --mode game
```

Управление: `1`/`2`/`3` выбирает бойца, `WASD` или стрелки перемещают его, `E` наносит физический импульс контейнеру. Сдвиньте контейнер влево до упора, затем проведите бойца через проход к зелёному выходу. Охранник замечает отряд по зрению и преследует его через FSM.

## Воспроизводимый игровой прогон

В разработческом Runtime проект проверялся настоящим Editor Play через `EngineCommandlets.exe project.input`, который принимает последовательность key/focus/frames и загружает проектную DLL. Для повтора полной победы используйте прилагаемые `evidence/fresh-clear-win-request.json` и `EngineCommandlets.exe project.input <request.json> <result.json>`. Запрос содержит абсолютный путь к папке проверки и при переносе требует заменить поле `project`. Подробный результат, телеметрия и D3D кадр сохранены рядом с отчётом.

`project.input` использует GPU/D3D и Windows desktop session. Для своих обычных игровых сессий запускайте EngineRuntime напрямую, как указано выше.


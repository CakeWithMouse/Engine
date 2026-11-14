# REBUILD и игра — «Ремонтный коридор» r2

Архив `RepairCorridor-r2-source.zip` содержит исходники C++, Content, Game.sln/vcxproj, Build.ps1 и проектные метаданные. В архиве нет SDK, Engine DLL/LIB, DLL проекта, PDB, Binaries, Intermediate или SaveData.

## Важная версия SDK

Для физического результата используйте исправленный локальный Runtime `E:\Engine\Game\build\x64\Release\Runtime`, Engine 2026.11.11.0 Release x64, базовый commit `388d7e9` + локальная фиксация `STRATEGY-003`. `Runtime\SDK\Lib\Engine.lib` SHA256:

`6846D88DEE3BAF892E3A4A06C1C965E7FC71966B78A1E5620972178FE7EE0C5D`

Публичный SDK Engine 2026.11.11.0 без этой локальной фиксации содержит дефект передачи плотности Jolt и не воспроизводит движение контейнера. Исправление пока не опубликовано как отдельный релизный Runtime. Для переноса используйте указанный исправленный Runtime или SDK, собранный с эквивалентным исправлением. Также требуются MSVC v143 (проверено 14.44.35207) и Windows SDK 10.0.26100.0.

## Сборка

Распакуйте архив в новую папку и укажите проект и Runtime:

```powershell
$Project = 'E:\Games\RepairCorridor'
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& "$Runtime\SDK\ProjectTools.ps1" -Action Build -Rebuild `
  -Project "$Project\Project.json" -Runtime $Runtime
```

Успешный журнал заканчивается `Build validated and activated`; собственная DLL проекта появляется в `Binaries`.

## Играть

Для интерактивной сессии Windows запустите игру напрямую:

```powershell
& "$Runtime\EngineRuntime.exe" --project "$Project\Project.json" --mode game
```

Управление: `1`/`2`/`3` выбирает бойца, ПКМ по полу задаёт точку выбранному бойцу, WASD/стрелки вручную двигают его (ручное движение отменяет приказ), E толкает физический контейнер. Приказы остальных бойцов продолжают исполняться после смены выделения. Чтобы победить, откройте проход, убедитесь по HUD, что crate остановился у левого упора, затем направьте бойца к зелёному выходу. Охранник использует FSM Patrol/Pause/Chase и способен перехватить ранний прорыв.

Приказы идут по прямой и не обходят препятствия. Если путь закрыт crate, сперва освободите его импульсом; навигационной сетки/обхода стен в этом этапе нет.

## Воспроизвести D3D Editor Play

Подробный подтверждённый запрос после свежего извлечения находится в `evidence/fresh-order-clear-win-request.json`; для переноса замените абсолютный путь поля `project` на свой `Project.json` и задайте новый файл результата. Команда использует позиционные аргументы (request — входной JSON, result — выходной JSON), без флагов `--input/--output`:

```powershell
$tool = Join-Path $Runtime 'EngineCommandlets.exe'
$Request = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-strategy-r2\strategy\evidence\fresh-order-clear-win-request.json'
$Result = Join-Path $Project 'fresh-order-clear-win-result.json'
& $tool project.input $Request $Result
```

Для показа D3D кадра выставьте перед запуском:

```powershell
$env:PAINENGINE_D3D_SCREENSHOT = 'C:\Temp\repair-win.bmp'
$env:PAINENGINE_D3D_SCREENSHOT_FRAME = '410'
```

`project.input` требует GPU и Windows desktop session. Обычную пользовательскую игру запускайте `EngineRuntime.exe` в `--mode game`, как выше.




# Rebuild and run — Repair Corridor r3

## Архив

`RepairCorridor-r3-source.zip` — исходный проект (16 файлов, SHA256 `5134954F87EBBE5B9660D73862037870716B41F28E1666D88769DF394752E3C1`). Включены `Game.sln`, `Game.vcxproj`, `Build.ps1`, манифесты, `Source`, `Content`. ZIP не содержит SDK, DLL/LIB/PDB, Binaries, Intermediate или SaveData. Проверенная свежая распаковка: `E:\Engine\Engine\build\playtests\strategy-r3\RepairCorridorFresh`.

## Совместимость SDK

Собиралось на Windows x64 / Release, MSVC v143 14.44.35207, Engine 2026.11.11.0 localOnly, база main `388d7e9` плюс локальные STRATEGY-003/007/008/013/015. Это SDK не опубликована как обычный релиз. Использованный Runtime:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$ExpectedEngineLibSha256 = '18622FC582C05F0DEC25753C4297C3276C44C736D764FBFDA460D2E56F35B1B3'
Get-FileHash -Algorithm SHA256 (Join-Path $Runtime 'SDK\Lib\Engine.lib')
```

Для другого Runtime той же версии проверяйте `Runtime\SDK\Lib\Engine.lib`, а не только `Sdk.json`: предыдущие локальные SDK той же версии не содержат всех этих fixes. Для переноса нужен эквивалентно собранный EngineRuntime/SDK с перечисленными исправлениями.

## Сборка

Распакуйте ZIP в новую папку, затем передайте абсолютный `Project.json`:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$Project = 'E:\Engine\Engine\build\playtests\strategy-r3\RepairCorridorFresh\Project.json'
& (Join-Path $Runtime 'SDK\ProjectTools.ps1') `
    -Action Build -Rebuild -Project $Project -Runtime $Runtime
```

Успех: `Build validated and activated`. Проект после этого содержит собственный `Binaries\Release\<buildId>\Game.dll`; SDK в ZIP не копируется.

## Короткий standalone smoke

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$Project = 'E:\Engine\Engine\build\playtests\strategy-r3\RepairCorridorFresh\Project.json'
$ProjectRoot = Split-Path $Project
$Store = 'E:\Engine\Engine\build\playtests\strategy-r3\EngineStore'
$env:PAINENGINE_ENGINE_STORE = $Store
$exe = Join-Path $Runtime 'EngineRuntime.exe'
$arguments = @('--project', ('"' + $Project + '"'), '--mode', 'game', '--frames', '60')
$process = Start-Process -FilePath $exe -ArgumentList $arguments `
    -WorkingDirectory $ProjectRoot -Wait -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $ProjectRoot 'standalone.stdout.log') `
    -RedirectStandardError (Join-Path $ProjectRoot 'standalone.stderr.log')
$process.ExitCode
Remove-Item Env:\PAINENGINE_ENGINE_STORE
```

60 кадров завершились exit 0: `fresh-standalone.stdout.log`, `fresh-standalone.stderr.log`, `fresh-standalone.exit`. Это запуск Runtime; его не считать доказательством D3D-кадра или прохождения сценария.

## Повторить настоящий Play / D3D

В `fresh-central-gate-request.json` уже есть настоящий `project.input` сценарий: группа обходит центральный шкаф; приказ за закрытые ворота возвращает Unreachable; E физически сдвигает crate; сетка обновляет занятые ячейки; после открытия команда повторяется и группа доходит до дальней площадки. При переносе замените поле `project` на свой абсолютный `Project.json`, задайте новый путь для результата и новых файлов:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$Request = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-strategy-r3\strategy\fresh-central-gate-request.json'
$Replay = Join-Path 'E:\Engine\Engine\build\playtests\strategy-r3' ('replay-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Replay | Out-Null
$Result = Join-Path $Replay 'result.json'
$env:PAINENGINE_STRATEGY_TELEMETRY = Join-Path $Replay 'telemetry.log'
$env:PAINENGINE_D3D_SCREENSHOT = Join-Path $Replay 'frame.bmp'
$env:PAINENGINE_D3D_SCREENSHOT_FRAME = '820'
& (Join-Path $Runtime 'EngineCommandlets.exe') project.input $Request $Result
```

Команда возвращает exit 0, кадр — настоящий D3D 640×480. Проверенный кадр `fresh-central-gate.bmp` показывает позднюю точку после достижения приказа; HUD отображает поражение от охранника, а группа находится за crate/bumper. В финальной телеметрии: `gateOpen=1`, `acceptedOrders=3`, `navConnectedSelected=3`, `orderStatus=на месте`, `barrierViolationFrames=0`, `won=0`, `lost=1`. Потеря — игровой исход сценария, а не ошибка сборки/навигации.

## Управление

- ЛКМ по бойцу выбирает его; Shift+ЛКМ добавляет бойца или снимает выбор.
- ПКМ по полу выдаёт выбранной группе общий приказ. Маркер — жёлтый у выбранных и бирюзовый для цели.
- `E` толкает физический ящик к упору, освобождая gate.
- `1`/`2`/`3` выбирают одного бойца; камера фиксирована сверху.

Карта создаётся игровым bootstrap в `ProjectGame`; `Main.scene.json` намеренно пустая. Навигация использует одноуровневую сетку XZ. Нет рамочного выбора, NavMesh, редактируемой карты и команды pause клавишей; pause/resume проверялись управляющим API commandlet.



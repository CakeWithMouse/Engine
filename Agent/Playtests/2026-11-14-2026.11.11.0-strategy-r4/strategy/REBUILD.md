# Пересборка и запуск r4

Исходный проект: [RepairCorridor-r4-source.zip](RepairCorridor-r4-source.zip), SHA256 `D8CAD02B7D333D9149C804372A5DEDE765B1B561A73C0E3A20DFFE2862BCAD48`. Архив содержит 18 файлов проекта, включая Game.sln/Game.vcxproj и README; бинарников, SDK, Intermediate и SaveData нет. CRC/source audit: `zip-audit.json`.

## Точная зависимость Engine

- Engine version: `2026.11.11.0` (локальный, не опубликованный SDK).
- База: main `87a9cf85`, fixes STRATEGY-003/007/008/013/015/016.
- Runtime: `E:\Engine\Game\build\x64\Release\Runtime`.
- Engine library: `Runtime\SDK\Lib\Engine.lib`.
- SHA256: `358E352C9E30FCCD6ED87D7FE54581FC7F3820BBCDD3E6B31CDE7D142626ABEE`.
- Совместимый локальный Runtime с тем же исправлением обязателен; опубликованная старая сборка той же версии не гарантирует STRATEGY-016.

## Сборка новой копии

Распакуйте ZIP в новый каталог, затем укажите точный Runtime:

```powershell
$project = 'E:\Games\RepairCorridor\Project.json'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$env:PAINENGINE_ENGINE_STORE = 'E:\Games\RepairCorridor\EngineStore'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Rebuild -Project $project -Runtime $runtime
Remove-Item Env:PAINENGINE_ENGINE_STORE
```

Успех подтверждается строкой `Build validated and activated` и актуальным `Project.json` module DLL. Проверенный clean build отчёт — `r4-clean-extract-build.log`; статус с путём DLL — `r4-clean-extract-build.status`.

## Игра и диагностика

Откройте `Project.json` в GUI Launcher/редакторе и запустите Play. Сцена создаётся игровым bootstrap во время Play; Edit viewport не содержит готового коридора.

Повтор входного игрового Play (путь проекта замените на свой). Новый каталог
сохраняет результаты повтора отдельно от исторических доказательств:

```powershell
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$project = 'E:\Games\RepairCorridor\Project.json'
$source = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-strategy-r4\strategy\r4-clean-extract-play-request.json'
$replay = Join-Path 'E:\Engine\Engine\build\playtests\strategy-r4' ('replay-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $replay | Out-Null
$request = Get-Content $source -Raw -Encoding UTF8 | ConvertFrom-Json
$request.project = $project
$request | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $replay 'request.json') -Encoding UTF8
$env:PAINENGINE_STRATEGY_TELEMETRY = Join-Path $replay 'telemetry.log'
& "$runtime\EngineCommandlets.exe" project.input (Join-Path $replay 'request.json') (Join-Path $replay 'result.json')
Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY
```

Формат request и `project.input` описаны в `Engine/docs/COMMANDLETS.md`; проверенный окончательный запрос — `r4-clean-extract-play-request.json`. В нём настоящие Play/focus/cursor/key/frame события: выбрать отряд, открыть контейнер клавишей E, подождать пока gate будет проходим, один раз нажать ПКМ на охранника и проиграть кадры до исхода. Не посылайте повторный клик по прежнему экранному месту: охранник движется.

Краткий lifecycle smoke именно финальной чистой распаковки:

```powershell
& 'E:\Engine\Game\build\x64\Release\Runtime\EngineRuntime.exe' --project 'E:\Engine\Engine\build\playtests\strategy-r4\fresh-final\RepairCorridor\Project.json' --mode game --frames 60
```

Проверено: exit 0, `GameUpdates=60`, `EngineStopped`. D3D gameplay кадр победы — `r4-clean-extract-play-win.bmp`.

## Управление

ЛКМ/drag — выбрать рамкой; рамка работает в обе стороны. Shift+рамка добавляет бойцов, Shift+клик переключает выбор одного бойца. Одиночный ЛКМ выбирает бойца. ПКМ по полу — движение, ПКМ по охраннику — атака/преследование. E — импульс контейнеру, чтобы открыть ворота. README.md в проекте описывает правила HP и победы.

# Интеграция стратегии r4 со свежим main

По запросу пользователя все правки r4 сохранены коммитом `49d4c168` на базе
`87a9cf85`. Получен свежий main `0947535b` с AI-диагностикой во вьюпорте.
Объединение — `7b15363a`. Конфликтов не было: Group.h автоматически сохранил
DiagnosticField() из upstream и описание сопровождения ObjectRef из r4.
Исправление преследования, тесты, исходный архив игры и все отчёты сохранены.

## Проверки объединённого дерева

Release/v143, Assimp и Jolt:

- Engine: 83/83, 0 skipped, 0 failed (`engine-tests.log`).
- AI: 57/57, включая сопровождение, замену и удаление цели (`ai-tests.log`).
- Новая AI-диагностика: собран настоящий модуль, EditorUiTests --ai-diagnostics
  прошёл на native viewport. Проверены Edit preview/rebake/A*, Play/Pause,
  выбор контроллера, действующее поле группы, строй, выключение и Stop.
  Это автоматическая регрессия; отдельная ручная съёмка всех панелей не заявляется.
- Editor, Runtime, commandlets и локальный SDK собраны; whole-archive link прошёл.
- Hot reload: 65 проверок (`reload-tests.log`).
- r4 распакован из исходного ZIP в новый scratch, собран на объединённом SDK,
  игровая DLL валидирована и активирована. Настоящий Play одним приказом
  завершил бой: 4 удара, HP0, won1/lost0, activeAttackOrders0, нарушения барьера0.
  `strategy-win.bmp` осмотрен: видны победа, остановленный отряд и отсутствие врага.
- Отдельный Runtime: exit0, GameUpdates60, EngineStopped (`strategy-runtime.log`).

SDK Engine.lib SHA256:
`1A219C4E0656B92C287A97116B7FDE97307D0CE0036E40A27C4D8FE9F7EF5E33`.
Версия остаётся 2026.11.11.0 localOnly; новый релиз не публикуется.
Исторический r4 отчёт с прежним SDK358E352C… описывает исходный агентский прогон;
его доказательства не переписаны результатами этой интеграции.

Исходный архив r4 SHA256:
`D8CAD02B7D333D9149C804372A5DEDE765B1B561A73C0E3A20DFFE2862BCAD48`.
Все 147 файлов отчёта r4 добавлены в git с сохранением точных байтов. В build
остаётся рабочая копия игры; SDK, EXE/DLL/LIB/PDB в git не добавлялись.

## Повтор

Engine проверялся командой:

```powershell
./Engine/tests/run.ps1 -Configuration Release -PlatformToolset v143 -EnableAssimp
```

Остальные команды сохранены в `verify.ps1` и `verify-strategy.ps1`. Передавайте
новые абсолютные Output/Scratch пути, чтобы не перезаписать доказательства:

```powershell
$run = 'E:\Engine\Engine\build\playtests\integration-replay-' + [guid]::NewGuid().ToString('N')
$output = Join-Path $run 'evidence'
$scratch = Join-Path $run 'scratch'
./Agent/Playtests/2026-11-14-strategy-r4-main-integration/verify.ps1 -Output $output -Scratch $scratch
./Agent/Playtests/2026-11-14-strategy-r4-main-integration/verify-strategy.ps1 -Output $output -Scratch $scratch
```

Статус: интеграция и перечисленные проверки завершены. Отправка выполняется
обычным push в main, без force и удаления чужих коммитов.

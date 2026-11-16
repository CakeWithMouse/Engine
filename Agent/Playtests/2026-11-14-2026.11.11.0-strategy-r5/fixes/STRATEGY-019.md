# STRATEGY-019 — AUTO radius группы в AI-диагностике

Подтверждённый дефект Engine.Scene.AiDiagnostics, база main a5468726.
Контроллер Group имеет виртуальный radius=1, тогда как RequestField использует
максимальный radius живых участников. Диагностика брала Controller::Radius(),
из-за чего clearance и пробный A* не соответствовали реальной проходимости группы.

Исправление: в AUTO режиме для Group вычисляется максимальный radius живых
Members; пустая группа сохраняет прежний fallback. Одиночный контроллер и ручной
радиус не изменены. Игровое движение, ABI и модель навигации не менялись.

Регрессия добавлена в Editor/tests/AiDiagnosticsUiTests.h: Character=.6,
Group с бойцами .4 даёт .4, ручной probe=1.7 остаётся1.7.
До изменения native UI-тест завершился exit1 ровно на проверке радиуса группы:
[лог до](auto-radius-before.log), [код выхода](auto-radius-before.exit).
После пересборки диагностического Game.dll весь UI-тест прошёл exit0, включая
слои, реальное поле/слоты, Play/Pause/Stop и очистку overlay:
[лог после](auto-radius-after.log), [код выхода](auto-radius-after.exit).

Дополнительный AiDiagnosticsCommandletTests.ps1 проверил прямой API из
project.input: preview/live/paused/off/resume/stop и отказ на неверном request.
Артефакты находятся в commandlet-regression. Это проверка API, не визуальный отзыв.
Тот же автор повторил на Repair Corridor, SDK4E7FB9EC: paused Group5 вернул
radius=.400000006, field=ready, probe path=found. Ручной radius1 дал partial
и walkable=false на той же клетке clearance=.875. Данные:
../strategy/r5-ai-diagnostics-game-retry-{0,1,3}.json. Ошибка закрыта.

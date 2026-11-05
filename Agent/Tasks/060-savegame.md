# 060 — SaveGame сцены и базовых классов

Контракт изменён пользователем 2026-11-05: сохраняется вся сцена и базовое состояние
движка; наследники дописывают поля. Код экономики/машинок в Engine не включается.

Реализованы SaveArchive, очередь SaveGame/слоты, SceneDocument snapshot + отдельная
подготовленная сцена, переназначение ссылок, методы базовых Game/Component/Subsystem/
Player/Controller и специализированных render/UI-компонентов. SceneModule и editor Play
передают каталог; cold load может подготовить сессию до игрового запуска. Старые C ABI
таблицы не изменены. Прямые указатели и GPU/callback не сохраняются.

Документы: [SAVEGAME](../../Engine/docs/SAVEGAME.md), [HTML](../../Engine/docs/SAVEGAME.html),
API Reference, COMMANDLETS. Версия сборки 2026.11.05.0; публикация/коммит не выполнялись.

Проверено: SaveGameTests 73 проверки с D3D viewport + чтение новым процессом;
9 вызовов SaveGame commandlet; lifecycle подсистем; SceneModule 109; EditSession 651;
проверка публикации SDK при заблокированном каталоге и повторной попытке.
Отчёт агента первой игры и окончательные результаты: [прогон](../Playtests/2026-11-05-savegame/report.md).

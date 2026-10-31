# 043 — startupScene в game mode

Выполнено 18.10.2026. Без изменений Editor и EngineProperty.

- [x] Необязательный EngineGetGameSceneApi v1; прежние ABI сохранены.
- [x] Runtime: create → loadScene → start; Tick остаётся только в update игры.
- [x] RuntimeExample: транзакционное восстановление Scene из общего загрузчика.
- [x] Без startupScene старый lifecycle; при неподдерживаемом API/ошибке загрузки отказ до start.
- [x] Reload повторно читает начальный файл; cleanup ошибочной сессии.
- [x] Документация автора игры: Engine/docs/GAME-STARTUP-SCENE.md.
- [x] Тесты lifecycle, статичного start, одиночного Tick, ошибок и совместимости без редактора.

Окно standalone renderer, свойства и перенос живого состояния при reload не добавлялись.

Проверено Debug v143: 548 проверок edit-session, 109 scene-module; Main.cpp скомпилирован отдельно без Editor.

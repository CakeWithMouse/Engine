# 044 — Отображение startupScene в game mode

Выполнено 18.10.2026. Editor и EngineProperty не изменены.

- [x] Session владеет Game; загрузка и renderer используют его единственную Scene.
- [x] Однокадровый RuntimeFrame без StartGame, одна симуляция на кадр.
- [x] Необязательный GameWindowApi: непрерывный host loop без ожидания Enter.
- [x] Resize, минимизация, отложенное закрытие и корректное уничтожение DLL-окна.
- [x] MovingCube, JSON и развёртывание Content в RuntimeExample.
- [x] Документация: Engine/docs/GAME-SCENE-RENDERING.md.
- [x] Debug v143: 575 проверок сессии/рендера, 109 Scene API, 13/13 базовых.

Свойства и новый формат геометрии/материала не добавлялись.

CLI smoke: EngineRuntime --project Projects/RuntimeExample/Project.json --mode game --frames 3 — exit 0, GameUpdates=3, штатный cleanup.

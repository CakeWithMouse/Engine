# 042 — startupScene в Project.json

Выполнено 18.10.2026. Только путь --scene-api; без изменений Editor/EngineProperty.

- [x] Необязательный относительный путь, разрешение от Project.json.
- [x] Передача host-параметра и loadScene до создания/bind/start потребителя.
- [x] Ошибка загрузки останавливает запуск; нет fallback на пустую сцену.
- [x] Legacy editor/game не открывают startupScene; Play использует текущую edit.
- [x] Документация: Engine/docs/STARTUP-SCENE.md.
- [x] Debug v143: 508 проверок edit-session, компиляция Main.cpp без Editor.

Standalone game автозагрузка, UI документа, упаковщик ресурсов и свойства отложены.

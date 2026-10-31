# 040 — Edit-сессия, viewport и Play через API

Выполнено 2026-10-18. Только движок/runtime, тестовый игровой модуль и документация.
Editor/Qt не изменялись.

- Отдельный EngineGetEditModuleApi v1: edit-safe create без обычных game start/update.
- SceneEditSession: одна Scene для команд и renderer; отдельная копия поддерживаемых
  данных для Play, remap иерархии, Pause/Resume без пересоздания, Stop сохраняет Edit.
- Изменения Edit блокируются в Play/Pause. Общий guard потока/reentry для обоих API.
- В случае исключения Tick игровая копия становится Paused, документ остаётся жив.
- GameEditModule владеет DLL и сессией; API bind/unbind для будущего Editor.dll.
- Runtime opt-in: editModule/--scene-api; старый preview остаётся по умолчанию.
- Тесты без редактора: 129 проверок, в том числе Win32/D3D viewport, настоящий DLL,
  rollback создания/viewport и ABI-only consumer. Регрессия 109 scene-module, 13/13 ядра.

Контракт для интегратора: [EDIT-SESSION-API.md](../../Engine/docs/EDIT-SESSION-API.md).
Ограничения: один HWND на жизнь сессии, нет detach/rebind без закрытия; нет JSON,
свойств, UI, новых материалов/геометрии, input/lifecycle старых игр и hot reload данных.
Фактическое подключение существующего редактора и его UI делает другой разработчик.

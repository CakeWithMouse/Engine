# 037 / 035-B — Scene в ядре, существующий GameComponent

Статус: выполнено, Debug проверен. Согласовано автором: без SceneObject и JSON.

## Объём

- Scene владеет GameComponent, именами, runtime-ID и иерархией.
- API: добавление/удаление/поиск, переименование, transform/parent, revision и snapshot.
- Добавление и удаление во время Tick откладываются до внешней границы обновления.
- Game сохраняет рендер и совместимый RegisterComponent; Components — borrowed view.
- При прямом добавлении в Scene ресурсы создаются Game на кадре; свет без кэша указателей.
- Документ для редактора: [SCENE-EDITOR-API](../../Engine/docs/SCENE-EDITOR-API.md).
  DLL-команды, Inspector, SceneObject, JSON и копирование правок в Play не входят в этап.

## Приёмка

- Scene хранит и изменяет компоненты без окна/GPU; ID не меняется при переименовании.
- Ошибки не забирают владение и не повреждают данные; snapshot независим.
- Циклы и чужие родители отклоняются; удаление очищает ссылки детей/инстансов.
- Самоудаление и добавление в Tick безопасны, новые компоненты тикают со следующего обновления.
- Правки Scene видны в статичном viewport; Play/Pause/Stop и порядок Pong сохранены.
- Проверки ядра, viewport и Qt; результаты ниже.

## Результаты

2026-10-17, MSVC v143, Debug:

- EngineRuntime/Editor и тестовые проекты собраны.
- `run.ps1 -PlatformToolset auto`: 13/13. Три новые группы проверяют CPU-сцену:
  владение/ID, hierarchy/snapshot/validation, deferred/self-removal/exception boundary.
- `run-viewport.ps1`: изменения transform отображаются без Tick; прямое добавление
  получает GPU-ресурсы на кадре; удаление света не оставляет ссылок. Все четыре
  preview проходят Play/Pause/Stop/resize; исходная сцена сохраняется.
- `run-runtime.ps1`: 22/22.
- `run-editor-viewport.ps1 -Project BasicExamples -TestPlayControls`: Space/P,
  Stop из Pause, resize и закрытие проходят в настоящем Qt-окне.
- `git diff --check`: без ошибок whitespace.
- Release и Katamari/Assimp не проверялись. Scene-callbacks для Editor.dll,
  JSON и копирование редактируемых данных в Play остаются следующими этапами.

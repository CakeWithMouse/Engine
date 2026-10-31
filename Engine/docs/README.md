# Документация Engine

- [Меню редактора, ресурсы, прелоадер и Hot Reload](EDITOR-WORKFLOW.md).

- [Ввод, Character, контроллеры и назначение камеры Play](INPUT-CHARACTER.md).

- [Edit-сессия и viewport/Play API: инструкция подключения редактора](EDIT-SESSION-API.md).

- [Каталог игрового DLL и команды Scene: ABI, владение, подключение редактора](SCENE-MODULE-API.md).

- [Пользовательские классы: ENGINE_COMPONENT, генератор и API каталога для редактора](COMPONENT-GENERATION.md).

- [Scene: C++ API и контракт подключения редактора](SCENE-EDITOR-API.md).

- [Один EngineRuntime: запуск игры/редактора, DLL и тесты каркаса](RUNTIME-QUICKSTART.md).

- [Engine управляет игрой: текущее устройство, план перехода и оценка подхода](ENGINE-RUNTIME-PLAN.md).

- [Передача проекта разработчикам](DEVELOPER-HANDOFF.md): текущие два пути запуска и [контракт внедрения Editor.dll](DEVELOPER-HANDOFF.md#8-внедрение-редактора-согласованный-контракт).

- [Архитектура папок, проекты и лаунчер](FOLDER-ARCHITECTURE.md).

- [Техники рендера: что сделано и как](RENDERING.md): проходы кадра, освещение, тени, инстансинг, LOD, спрайты, частицы, диагностика.

- [Экранный интерфейс](UI.md): панели, текст и полосы, атлас шрифта, проход UI в кадре.

- [Отрисовка массовой техники: инстансинг и LOD](UNIT-RENDERING.md): требования к моделям, подключение и замеры.

- [Архитектура и текущие границы систем](Описание%20проекта.md).
- [Руководство по разработке](DEVELOPMENT.md): редактор, инициализация, ресурсы, создание игры, главный цикл и компоненты.
- [Сборка и запуск](../README.md).
- [План разделения Engine/GameExample](../../Agent/Tasks/027-engine-game-separation-roadmap.md).
- [Дальнейшие улучшения](../../Agent/Tasks/028-separation-followups.md).

Пути к исходникам в тексте руководств указаны относительно корня репозитория. Новую документацию по движку добавляем в эту папку и включаем в оглавление.

- [SCENE-JSON.md](SCENE-JSON.md) — формат JSON v1, файловый API v2, ошибки, транзакции и интеграция редактора.

- [STARTUP-SCENE.md](STARTUP-SCENE.md) — startupScene в Project.json, порядок загрузки при --scene-api и интеграция потребителя.

- [GAME-STARTUP-SCENE.md](GAME-STARTUP-SCENE.md) — загрузка перед start, GameSceneApi и lifecycle для автора игры.

- [GAME-SCENE-RENDERING.md](GAME-SCENE-RENDERING.md) — окно Game, рендер startupScene, lifecycle кадра и видимый пример.

## Собственный проект

[Создание проекта, примитивы, сохранение сцен и запуск](USER-PROJECTS.md).

[Материалы: файлы .mat, HLSL-поверхность, окно редактора и код игры](MATERIALS.md).
- [Проекты без кода и с C++: создание, классы, сборка, данные и запуск](CODE-PROJECTS.md).
- [Полная таблица возможностей и видимости классов](PROJECT-CAPABILITIES.md).

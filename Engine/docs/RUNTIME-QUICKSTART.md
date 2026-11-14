# EngineRuntime: игра и редактор — 0.3.0

Готовый Windows x64 пакет распакуйте полностью. Запустите `EngineLauncher.exe`,
создайте проект в новой папке или выберите существующий `Project.json`.
«Играть» запускает игру, «Редактировать» — Editor.dll через EngineRuntime.exe.
Оба режима используют Content выбранного проекта. Enter для кадров не нужен.

## Первый запуск

1. Создайте проект или откройте учебный пример.
2. Откройте редактор, выберите объект в иерархии и измените transform.
3. Сохраните сцену. Play запускает игровое состояние, Pause приостанавливает его,
   Stop возвращает состояние редактирования.
4. Модели импортируйте в Content, материалы и текстуры назначайте через редактор
   материала. Сцену и общий материал сохраняют отдельно.

В viewport: ПКМ и мышь — обзор, WASD — движение, Q/E — вниз/вверх, Shift — ускорение,
колесо — приближение. Горячие клавиши и меню: [рабочий процесс](EDITOR-WORKFLOW.md).

## Сборка из исходников

```powershell
./Engine/build.ps1 -Configuration Release -Target All -EnableAssimp $true
./Game/build/x64/Release/EngineLauncher.exe
```

Visual Studio C++ и Windows SDK нужны для сборки. Qt 6 MSVC x64 находится в
`Editor/ThirdParty/Qt6.7` либо задаётся через `QTDIR`; Assimp — через vcpkg.
Если Qt отсутствует, скрипт может собрать игры без Editor: это не полный релиз.
Результат полного выпуска: `Game/build/x64/Release/Runtime/Editor.dll`,
`EngineRuntime.exe`, шаблон `Templates/SceneProject` и `SDK`.

## Проект и версия

`Project.json` содержит имя, путь Game.dll, Content и startupScene.
`Engine.lock.json` закрепляет runtime. Текущая версия — 0.3.0; сцены v1/v2/v3,
Module API 5, Edit API 6 с адаптерами 4/5/6. Обновляйте старый проект в копии;
не подменяйте неизвестную пользовательскую DLL шаблонной.

Для C++ проекта используйте [CLI](LAUNCHER-CLI.md) и [CODE-PROJECTS](CODE-PROJECTS.md).
Готовый Release SDK находится в `Runtime/SDK`; генерация класса, сборка и reload
выполняются штатными командами. Заголовки и Engine.lib должны быть одного выпуска.

## Старые примеры

BasicExamples, Pong, SunGame, Battle и Katamari имеют отдельные exe.
BasicExamples, Pong, SunGame и Battle также предоставляют preview в редакторе;
его возможности зависят от адаптера сцены. Katamari требует Assimp и не заявлена
как редактируемый пользовательский проект. RuntimeExample использует Game.dll.
Наличие preview у старого exe не делает произвольный exe редактируемой сценой.

## Проверки и диагностика

```powershell
./Editor/tools/test-regression.ps1 -Configuration Release -Full
./Editor/tools/test-model-replacement.ps1 -Configuration Release
./Engine/tests/run-viewport.ps1 -Configuration Release
```

Для тестов нужен Windows и D3D11 GPU. Из готового пакета проверяются запуск игр,
редактор, создание проекта и компиляция C++ с поставляемым SDK.
Если редактор показывает старое поведение, закройте процесс и проверьте путь
фактически загруженного Editor.dll. Логи запуска и ошибок ресурсов помогают
отличить несовместимый модуль от отсутствующего файла.

Полное описание: [передача разработчику](DEVELOPER-HANDOFF.md),
[формат сцены](SCENE-JSON.md), [версии](ENGINE-VERSIONS.md),
[релиз 0.3.0](RELEASE-0.3.0.md). Исторический консольный каркас не описывает этот выпуск.

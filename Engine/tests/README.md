# Проверки движка

Edit/Play API: `./Engine/tests/edit-session/run.ps1 -PlatformToolset auto`.
129 проверок без Editor/Qt, включая отдельное Win32-окно/D3D11, настоящий игровой DLL
и ABI-only consumer. Инструкция: [EDIT-SESSION-API.md](../docs/EDIT-SESSION-API.md).

Игровой DLL и команды Scene: `./Engine/tests/scene-module/run.ps1 -PlatformToolset auto`.
109 проверок с настоящим DLL и загрузчиком runtime; без редактора, окна и GPU.
Контракт: [SCENE-MODULE-API.md](../docs/SCENE-MODULE-API.md).

Генератор компонентов и его C++ API: `./Engine/tests/component-generation/run.ps1`
(`-Configuration Debug -PlatformToolset auto`). Проверяет сканер, генерацию, ошибки
компилятора, создание реального класса и Scene.Tick. Без редактора, окна и GPU.
Контракт и ограничения — [COMPONENT-GENERATION.md](../docs/COMPONENT-GENERATION.md).

`SceneTests.cpp` проверяет владение/ID, иерархию/snapshot и отложенные изменения Scene
без создания Game, окна или GPU. Эти группы входят в общий EngineTests.
ViewportTests дополнительно проверяет отображение правок Scene без Tick и создание
GPU-ресурсов для напрямую добавленного компонента.

Отдельные проверки встроенного viewport: `./Engine/tests/run-viewport.ps1` (D3D11,
пиксели, анимация, resize, владение HWND) и после сборки Runtime
`./Engine/tests/run-editor-viewport.ps1` (настоящий Qt-интерфейс, обновление без stdin,
resize и закрытие). Обе поддерживают `-Configuration Release`.
`run-viewport.ps1` также проверяет Edit/Play/Pause, возврат исходной сцены и ошибку Play.
Для проверки клавиш в настоящем Qt-окне: `./Engine/tests/run-editor-viewport.ps1 -Project BasicExamples -TestPlayControls`.
Обычный набор WARP ниже остаётся безоконным. См. [инструкцию](../docs/RUNTIME-QUICKSTART.md).

Из корня репозитория:

```powershell
./Engine/tests/run.ps1
./Engine/tests/run.ps1 -Configuration Release
```

Нужны Windows, Visual Studio 2026 с MSVC v145 и Windows SDK 10. Для другой
установленной версии toolset передайте, например, `-PlatformToolset v143`.
Скрипт находит MSBuild. Шейдеры копируются рядом с exe, поэтому рабочая директория не важна.
Ненулевой код завершения означает ошибку сборки или проверки. `CHECK` работает
и в Release, в отличие от стандартного `assert`.

Отдельный проект линкуется с настоящей Engine.lib через ProjectReference. Он не требует Assimp,
vcpkg, моделей или текстур. GPU-проверки используют программный D3D11 WARP;
окно и swap chain не создаются.

Проверяются:

- регистрация указателя под разными именами и в разных играх, однократное
  уничтожение, сохранение владельца при отказе;
- ошибки регистрации: nullptr, пустое/занятое имя, отсутствие устройства;
- иерархия трансформаций, инвалидация кеша через родителей, запрет циклов,
  наследование трансформаций без масштаба;
- удаление родителя, очистка ссылок у компонентов и инстансов;
- снимок положения, параметров и состояния источников света;
- компиляция и переключение forward/deferred вариантов, исключение
  отражающего материала из G-buffer;
- реальные результаты vertex shader частиц через stream output: прямой
  порядок без сортировочного SRV, заданная перестановка, возврат к прямому порядку;
- переключение сортировки в ParticleSystemComponent, число compute dispatch,
  ограничение минимального числа частиц и передаваемый на GPU флаг режима;
- обработка WM_QUIT с сохранением кода завершения.

Не покрыты импорт Assimp, оконный resize/Present, аппаратные драйверы и
визуальное качество сцен. Эти проверки не заменяют запуск основного приложения.

После общей сборки можно использовать ./Engine/tests/run.ps1 -NoBuild. Результаты: Engine/build/x64/<Configuration>/EngineTests.exe.

## Игры и лаунчер

После общей сборки: `./Game/tests/run-games.ps1` (или `-Configuration Release`). Проверяются три проекта без Assimp, запуск через лаунчер, независимость от CWD и ошибки отсутствующих файлов. Katamari проверяется при наличии её exe; иначе фиксируется недоступность сборки. Это smoke-проверки, не проверка изображения и управления. Логи в Game/build/launcher-checks.
# Проверки нового Runtime

`./Engine/tests/run-runtime.ps1` собирает host, обе DLL, лаунчер и тестовые DLL, затем проверяет жизненный цикл и ошибки. Для Release добавьте `-Configuration Release`; toolset выбирается автоматически или через `-PlatformToolset v143`. Это отдельные проверки каркаса, не замена тестов старого ядра ниже. См. [инструкцию Runtime](../docs/RUNTIME-QUICKSTART.md).

JSON сцен входит в edit-session/run.ps1: 478 проверок API/JSON/viewport без Editor. Формат и контракт — ../docs/SCENE-JSON.md.

StartupSceneTests.h в edit-session проверяет Project.json и запуск до bind/start без Editor. Текущий набор — 508 проверок Debug v143.

GameStartupTests.h проверяет game lifecycle и одиночный Tick; текущий edit-session набор — 548 проверок, включая startup обоих режимов.

RuntimeRenderTests.h: JSON Scene в Game, draw calls, одиночный Tick, resize/close/failure. Общий набор edit-session: 575 проверок.

# Структура Engine / Game / Agent

Обновление 032: добавлен [каркас одного EngineRuntime.exe с game/editor DLL](RUNTIME-QUICKSTART.md), запускаемый из лаунчера. Его жизненный цикл работает на заглушках, старые примеры и реальный рендер пока используют Game. Вариант отдельного Editor.exe заменён режимом editor общего host.


В корне репозитория находятся три рабочих каталога и файлы Git. Задача переноса: [030](../../Agent/Tasks/030-repository-layout.md). Пути ниже — относительно корня репозитория `O:/Engine/Engine`.

```text
Engine/
  Engine.sln, Engine.slnLaunch   решение и профиль запуска лаунчера
  Engine.vcxproj, Engine.props   библиотека и публичные include-пути
  build.ps1                     общая сборка решения
  BuildSupport/                 настройки MSBuild, копирование контента, скрипты
  Include/Engine/               публичные заголовки
    Base/                       BaseEngine, BaseResources, BaseGameConfig
  Source/                       реализации ядра
  Content/Shaders/              общие шейдеры
  ThirdParty/                   GLM, stb и лицензии
  tests/                       EngineTests и run.ps1
  docs/, README.md             документация пользователя и разработчика
  build/                       результаты сборки Engine и его тестов
Game/
  BasicExamples/                две сцены: «Квадраты» / «Куб»
  Pong/                         ракетки и мяч
  SunGame/                      солнечная система
  Katamari/                     сбор объектов, нужен Assimp
  Common/                       общая обвязка примеров
  Launcher/                     графический EngineLauncher
  tests/run-games.ps1           проверки запуска игр и лаунчера
  Start.cmd                     собрать Debug и открыть лаунчер
  Start-With-Assimp.cmd         сборка с импортёром и Katamari
  README.md                     краткая инструкция запуска
  Archive/1_Lab/                прежние исходники, сцены и проекты для сравнения
  build/                       результаты сборки всех игровых приложений
Agent/
  AGENTS.md                     постоянный контекст и правила
  engine-developer.md           профиль разработчика движка
  Tasks/                        задачи, результаты и планы
  Reviews/                      исходные разборы кода
  README.md                     назначение материалов агента
  Archive/                      прежние результаты сборок, не используются
.git/
.gitignore
.gitattributes
```

## Запуск без терминала

Открыть папку **Game** и дважды щёлкнуть **Start.cmd**. Он вызывает Engine/build.ps1 и открывает лаунчер только после успешной Debug-сборки. При ошибке окно остаётся с сообщением; устаревший exe не запускается. Нужны самостоятельно установленные Visual Studio 2026, C++ toolset v145 и Windows SDK 10.

В лаунчере выбрать проект и нажать «Запустить». BasicExamples дополнительно предлагает «Квадраты» / «Куб»; отмена закрывает приложение. Каждая игра запускается отдельным процессом. Engine.lib — библиотека, лаунчер не является работающим runtime-хостом и не загружает игровые DLL.

В IDE открыть **Engine/Engine.sln**. Лаунчер — первый проект и указан в Engine.slnLaunch. Если IDE сохранила старый стартовый проект, один раз выбрать EngineLauncher вручную. Перед F5 собрать всё решение: отдельная сборка лаунчера не собирает игры.

Каталоги результатов:

```text
Engine/build/x64/Debug/
  Engine.lib
  EngineTests.exe
  EngineContent/
Game/build/x64/Debug/
  EngineLauncher.exe
  BasicExamples/
    BasicExamples.exe
    EngineContent/
    GameContent/
    RequiredFiles.txt
  Pong/                         тот же набор для Pong
  SunGame/                      тот же набор для SunGame
  Katamari/                     после сборки с Assimp
```

Release имеет аналогичное дерево. Объектные файлы ядра находятся в Engine/build/obj; игр и лаунчера — в Game/build/obj. Логи smoke-проверок — Game/build/launcher-checks. Каталоги build не индексируются Git.

Прямой запуск: **Game/build/x64/Debug/<Name>/<Name>.exe**. Чтобы перенести игру, копировать весь её выходной каталог с ресурсами. Для лаунчера со всеми играми копировать целиком Game/build/x64/Debug (или Release). Старая общая папка build сохранена в Agent/Archive/previous-build, её приложения больше не используются.

## Сборка и проверки командами

Из корня репозитория:

```powershell
./Engine/build.ps1
./Engine/build.ps1 -Configuration Release
./Engine/build.ps1 -Target BasicExamples
./Engine/build.ps1 -Target Pong
./Engine/tests/run.ps1 -NoBuild
./Game/tests/run-games.ps1
./Engine/tests/run.ps1 -Configuration Release -NoBuild
./Game/tests/run-games.ps1 -Configuration Release
```

BasicExamples допускает `--scene triangles` и `--scene cube`. `--smoke` выполняет 30 кадров; без явной сцены выбираются квадраты. Проверки отдельно запускают обе сцены, игры через лаунчер, из посторонней рабочей директории и пути с пробелами. Smoke не заменяет проверку изображения и управления вручную.

## Katamari

Полная сцена сохранена в Game/Katamari. Она требует Assimp и по умолчанию исключена из Build в решении, чтобы остальные проекты собирались без внешнего импортёра. После самостоятельной установки vcpkg задать VCPKG_ROOT и запускать **Game/Start-With-Assimp.cmd**. Интеграция manifest может получать и собирать пакеты Engine/vcpkg.json; инструмент vcpkg сам не устанавливается.

В CLI: `./Engine/build.ps1 -EnableAssimp` или `./Engine/build.ps1 -Target Katamari -EnableAssimp`. При прямой сборке проекта MSBuild нужен EngineEnableAssimp=true для него и Engine. Без зависимости возникает явная ошибка, подменной сцены нет. До появления vcpkg проверены только исходники Katamari без линковки; полный запуск остаётся открытой задачей.

## Границы кода

Engine содержит общие механизмы: окно, D3D11, цикл, рендер, ввод, базовые компоненты и загрузчики. Игра содержит свою сцену, правила и настройки; обращается к Engine через ProjectReference и публичные заголовки. Engine не включает игровые исходники и не перечисляет названия игр.

Game/Launcher содержит список проектов и запускает exe через CreateProcess; он не линкуется с Engine.lib. Game/Common содержит общую обвязку примеров, проверку контента и OrderedSceneGame для BasicExamples. Данные двух квадратов — в BasicExamples/Source/SquareGeometry.h. Общие CubeComponent, TriangleComponent и другие компоненты остаются в Engine.

Текущий рабочий цикл по-прежнему принадлежит классу Game в Engine. BaseEngine, BaseResources, BaseGameConfig остаются основой следующего этапа; их незавершённый жизненный цикл не заменён фиктивными методами (028-H). Редактор и DLL hot reload — отдельные будущие задачи.

## Ресурсы

У каждой игры свои Source, Content, RequiredFiles.txt и .vcxproj. Engine/Content копируется в EngineContent рядом с каждым exe, Content игры — в её GameContent. Игры не делят GameContent, одинаковые имена файлов не конфликтуют. Манифест RequiredFiles.txt проверяется лаунчером и прямой точкой входа до старта; содержит обязательные относительные пути, по одному на строку.

По умолчанию пути определяются от exe, без смены рабочей директории. ENGINE_CONTENT_ROOT и GAME_CONTENT_ROOT позволяют указать абсолютные корни для разработки; игровой корень должен соответствовать выбранной игре. Лаунчер наследует окружение. Изменение файла не означает автоматическую перезагрузку GPU-ресурса: пока нужна сборка копирования и перезапуск.

cosy.jpg не предоставлена для SunGame/Katamari и остаётся необязательной. Семь obj/jpg Katamari совпадают с оригиналами; лицензии сохранены в её Content/Models. Нераспакованные mtl и дополнительные текстуры находятся в исторических архивах Game/Archive/1_Lab, их восстановление требует раздельных каталогов из-за одинаковых имён texture.jpg и отдельной визуальной проверки.

Copy обновляет/добавляет контент, но не удаляет устаревшие файлы: задача 028-C. Старый код и результаты сборки сохраняются в Archive без участия в новых проектах.

## Как добавить игру

1. Создать Game/<Name>/Source и Content, игровой класс через публичный API Engine и Main.cpp. Вызвать общую PrepareContent до создания BaseResources и Initialize.
2. Взять BasicExamples.vcxproj как образец: новый GUID, нужные ClCompile/ClInclude, OutDir в Game/build/x64/<Configuration>/<Name>, IntDir в Game/build/obj. Импорты общих настроек — ../../Engine/BuildSupport, ProjectReference — ../../Engine/Engine.vcxproj. Не включать cpp движка повторно.
3. Добавить RequiredFiles.txt, сохранить копирование EngineContent и GameContent рядом с exe.
4. Добавить проект в Engine/Engine.sln с путём ../Game/<Name>/<Name>.vcxproj и имя цели в Engine/build.ps1.
5. Добавить запись в Game/Launcher/Source/Catalog.h, обновить размер массива. Имя записи должно совпадать с выходной папкой и exe.
6. Добавить проверки в Game/tests/run-games.ps1; собрать, проверить запуск, ошибки, завершение и поведение вручную.

Материалы агента отделены от пользовательской документации. Перед новой агентской задачей явно указывать чтение Agent/AGENTS.md и Agent/engine-developer.md; корневого AGENTS.md больше нет. Исторические команды в Agent/Tasks сохраняют пути на момент выполнения, актуальные пути приведены здесь.

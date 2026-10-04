# Учебный движок

Новый [каркас EngineRuntime](docs/RUNTIME-QUICKSTART.md): в лаунчере выберите Runtime Example и «Играть» / «Редактировать». Один exe загружает Game.dll или Editor.dll после подготовки Engine. Пока без графики: Enter — кадр, q — выход. Старые игры ниже сохранены. Скрипты автоматически выбирают установленный v145 или v143; поддержан Visual Studio 2022 с C++ tools и SDK.

[Документ для новых разработчиков](docs/DEVELOPER-HANDOFF.md): текущая структура, порядок запуска и вызовов, пересборка и точки подключения редактора.

C++17, Windows, Direct3D 11. Общие механизмы находятся в Engine, четыре самостоятельных проекта — в Game. Рабочее решение — Engine/Engine.sln.

## Запуск без команд

Дважды щёлкните **Start.cmd** в папке Game: после успешной Debug-сборки откроется EngineLauncher. Выберите проект и нажмите «Запустить». В BasicExamples затем выберите сцену «Квадраты» или «Куб». Нужны Visual Studio с C++ toolset v145 или v143 и Windows SDK 10. Скрипт ничего не устанавливает. При ошибке сборки старые бинарники не запускаются.

Доступные без импортёра игры: **BasicExamples, Pong, SunGame**. **Katamari** требует vcpkg/Assimp; после настройки VCPKG_ROOT используйте **Game/Start-With-Assimp.cmd**. Её полная сцена сохранена, а проект исключён из обычной сборки до подключения зависимости.

Лаунчер запускает отдельные процессы. Старые примеры используют игровые exe; Runtime Example — общий EngineRuntime.exe с Game.dll или Editor.dll. Engine.lib остаётся библиотекой. Hot reload и графический редактор пока отсутствуют.

## Структура

- Engine/Include/Engine и Engine/Source — публичный API и реализация ядра.
- Engine/Include/Engine/Base — BaseEngine, BaseResources, BaseGameConfig; жизненный цикл заглушек подключён к EngineRuntime (032), перенос старого Game ещё впереди.
- Engine/Content и Engine/ThirdParty — общие ресурсы и сторонний код.
- Game/<Name>/Source, Content, <Name>.vcxproj — отдельная игра.
- Game/Common — общая обвязка примеров.
- Game/Launcher — Win32-список игр и запуск exe.
- Engine/BuildSupport — общие настройки MSBuild и скрипты.
- Engine/tests и Game/tests — проверки; Agent/Tasks — план и результаты.
- Game/Archive/1_Lab — исходные лабораторные для сравнения, старый проект не используется.

Подробное дерево, зависимости, ресурсы и добавление новой игры — [архитектура папок](docs/FOLDER-ARCHITECTURE.md). [Вся документация Engine](docs/README.md).

## Сборка и проверки из PowerShell

```powershell
./Engine/build.ps1
./Engine/build.ps1 -Configuration Release
./Engine/build.ps1 -Target Pong
./Engine/build.ps1 -Target Katamari -EnableAssimp
./Engine/tests/run.ps1 -NoBuild
./Game/tests/run-games.ps1
```

Лаунчер: Game/build/x64/Debug/EngineLauncher.exe. Прямой запуск игры: Game/build/x64/Debug/<Name>/<Name>.exe. Release имеет аналогичные пути. Рядом с каждой игрой находятся собственные EngineContent, GameContent и RequiredFiles.txt. Рабочая директория не влияет на поиск ресурсов.

Необязательная cosy.jpg для SunGame/Katamari не предоставлена: положите её в Content соответствующей игры и пересоберите для отражений. Поддержаны ENGINE_CONTENT_ROOT и GAME_CONTENT_ROOT; подробности — в документации папок. Для переноса игры копируйте весь её выходной каталог.

Общие клавиши: Esc — выход, F1/F2 — режим камеры, F3 — G-buffer, F4 — VSync, F5 — Forward/Deferred. Space обнуляет время обычных компонентов в базовом тике; игровые переопределения могут иметь другой порядок. Pong: левая ракетка — стрелки, правая — WASD. Режим --smoke каждой игры выполняет 30 кадров и завершает процесс.

Проверки: [тесты](tests/README.md). Новая структура — [030](../Agent/Tasks/030-repository-layout.md), перенос игр — [029](../Agent/Tasks/029-games-and-launcher.md), развитие — [028](../Agent/Tasks/028-separation-followups.md), история отделения ядра — [027](../Agent/Tasks/027-engine-game-separation-roadmap.md).

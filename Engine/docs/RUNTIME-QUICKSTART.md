# EngineRuntime: игра и режим редактора

Один `EngineRuntime.exe` сначала готовит Engine, затем загружает `Game.dll` или `Editor.dll`. Лаунчер выбирает проект и режим, а не запускает второй экземпляр ядра внутри себя.

Это каркас задачи 032. Загрузка DLL, чтение проекта, вызовы жизненного цикла, обработка ошибок и завершение настоящие. Окно игры, графика, сцена, ресурсы GPU и панели редактора пока заменены сообщениями в консоли. Старые игры используют прежний путь и ещё не перенесены.

## Через лаунчер

1. Запустите `Game/Start.cmd`.
2. После сборки выберите **Runtime Example — игра / редактор (заглушки)**.
3. Нажмите **Играть** или **Редактировать**.
4. В открывшейся консоли нажмите Enter для одного кадра. Для штатного завершения введите `q` и нажмите Enter.

Пошаговый ввод временный: у каркаса ещё нет оконного цикла. Он позволяет видеть порядок событий и не нагружает CPU ожиданием в цикле. В ограниченном тестовом запуске ввод не нужен, задержек нет, шаг времени равен 1/60 секунды. EOF также завершает интерактивную сессию.

Для старых BasicExamples/Pong/SunGame/Katamari кнопка редактирования отключена. Они продолжают запускаться своими exe.

Скрипты сборки выбирают установленный toolset: сначала v145, иначе v143. Нужны Visual Studio с C++ tools и Windows SDK. Ничего не устанавливается автоматически. Проекты IDE по-прежнему указывают v145; при открытии в VS 2022 выберите v143 либо собирайте скриптом. Явный `-PlatformToolset v143` также поддерживается.

## Сборка и прямой запуск

Из корня репозитория:

```powershell
./Engine/build.ps1 -Target EngineLauncher
# Или собрать только новый host и его зависимости:
./Engine/build.ps1 -Target EngineRuntime

./Game/build/x64/Debug/Runtime/EngineRuntime.exe --project Projects/RuntimeExample/Project.json --mode game --frames 3
./Game/build/x64/Debug/Runtime/EngineRuntime.exe --project Projects/RuntimeExample/Project.json --mode editor --frames 3
```

`--frames N` — положительное целое число; без него включён пошаговый режим Enter/q. `--project` и `--mode` обязательны. Относительный путь к проекту считается от каталога EngineRuntime.exe, а не от текущей директории терминала. Абсолютный путь тоже принимается.

Результат сборки:

```text
Game/build/x64/Debug/
  EngineLauncher.exe
  Runtime/
    EngineRuntime.exe
    Editor.dll
    Projects/RuntimeExample/
      Project.json
      Game.dll
      Content/README.md
```

Для Release замените Debug на Release и передайте сборке `-Configuration Release`. Переносите каталог Runtime целиком. Для лаунчера сохраняйте его соседство с Runtime. Собранные DLL требуют совместимого MSVC runtime; Release предназначен для распространения, Debug — для машины разработчика.

## Описание проекта

```json
{
  "name": "Runtime Example",
  "module": "Game.dll",
  "content": "Content"
}
```

В текущем узком формате нужны ровно три непустых строковых поля. Дубликаты, неизвестные поля и неверный JSON отклоняются. Файл — UTF-8 (BOM допустим), до 64 KiB. Поддержаны JSON-экранирование и Unicode. Пути module/content считаются от Project.json; content должен существовать. DLL выбирается только для режима game: редактор может открыть описание проекта даже без Game.dll.

Editor.dll принадлежит установленному host и находится рядом с ним. Путь EngineContent также определяется host; реальные файлы базовых ресурсов в новом каркасе пока не загружаются и не нужны. Глобальная настройка EnginePaths выполняется до создания BaseEngine/BaseResources.

## Порядок работы

```text
EngineStarting → ResourcesReady → EngineServicesReady → EngineReady
  → ModuleLoaded → GameCreate/GameStart или EditorCreate/EditorStart
  → N кадров:
       GameUpdate/EditorUpdate → EngineUpdate → EngineRender
       → EditorOverlay (только editor) → EngineFrameEnd
  → GameStop/EditorStop → GameDestroy/EditorDestroy
  → ModuleUnloaded → ResourcesStopped → EngineStopped
```

Значения счётчиков печатаются при остановке. ModuleUnloaded печатается только после успешного FreeLibrary. При ошибке возвращается ненулевой код и освобождаются созданные части. Если Start сессии не удался, вызывается Destroy без обычного Stop. Ошибка ядра не допускает загрузку DLL.

Один экземпляр BaseEngine допускает один Run; повторный Run отклоняется. Stop после завершения безопасен. Внутри активного цикла остановка является запросом; сначала заканчиваются вызовы модуля и уничтожается сессия, затем освобождается ядро.

## Граница кода

- `Engine/Source/Base/Base.cpp` — жизненный цикл BaseEngine, единственный цикл и загрузка модулей.
- `Engine/Runtime` — exe, аргументы и чтение проекта.
- `Game/RuntimeExample/Source/Module.cpp` — простая игровая сессия.
- `Editor/Source/Module.cpp` — простая сессия редактора без игровой симуляции.
- `Engine/Include/Engine/Runtime/ModuleApi.h` — минимальный ABI v1.

BaseEngine/BaseResources/BaseGameConfig сохранены. У BaseEngine старый публичный набор раздельных стадий заменён одним host-вызовом Run: Initialize, StartUp и Play теперь приватные. Initialize больше не принимает копию BaseResources: ресурсами владеет сам Engine. BaseGameConfig сохранён без изменения полей; его настройки графики пока не используются новым каркасом. Старый класс Game не изменён.

Модули экспортируют EngineGetModuleApi и сообщают версию и тип. Таблица функций не содержит STL и C++-объектов, сессия непрозрачна для host. Создание/уничтожение происходят внутри той же DLL. Переданные строки и контекст живут до Destroy, callbacks синхронные и не должны переживать сессию. Экспортируемые функции не должны выбрасывать исключения; текущие модули используют простые операции и nothrow-выделение. Это архитектурный контракт для доверенных модулей, не изоляция нативного кода.

Контекст содержит журнал, запрос выхода и сведения о проекте. Он не выдаёт BaseEngine, device/context, реестр сцены или управление очисткой. Game.dll и Editor.dll не линкуются с Engine.lib. Их загрузка не создаёт отдельный процесс.

## Проверки

```powershell
./Engine/tests/run-runtime.ps1
./Engine/tests/run-runtime.ps1 -Configuration Release
# Если host, launcher и тестовые DLL уже собраны:
./Engine/tests/run-runtime.ps1 -NoBuild

# Диагностические команды используют тот же путь запуска, что и кнопки:
./Game/build/x64/Debug/EngineLauncher.exe --smoke RuntimeExample
./Game/build/x64/Debug/EngineLauncher.exe --smoke-editor RuntimeExample
```

Тесты собирают настоящие ошибочные DLL в Engine/build, создают изолированные копии размещения и сохраняют журналы в `Engine/build/runtime-checks/<Configuration>/<run-id>`. Проверяют порядок, количество кадров, ошибки ABI/старта/обновления, запрос выхода, аргументы, JSON и запуск из другой директории с пробелами в пути. Рабочие DLL не подменяются.

`--test-fail-engine` — явная диагностика отказа после подготовки ресурсов-заглушек. Лаунчер этот флаг не использует. Ошибки модулей создаются отдельными тестовыми DLL, а не переключателями в игре.

Фактические результаты и ограничения ручной приёмки записаны в [задаче 032](../../Agent/Tasks/032-runtime-game-editor-modes.md). Следующая большая работа — перенос реальных механизмов старого Game, затем сцены и интерфейса редактора; текущий каркас этого не выполняет.

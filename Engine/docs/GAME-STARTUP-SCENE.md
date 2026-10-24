# startupScene в обычном game mode: руководство автора игры

Этап 043. Загрузка добавлена к существующему lifecycle ModuleApi v4.
Редактор, EngineProperty и формат JSON не изменялись.
Формат данных: [SCENE-JSON.md](SCENE-JSON.md); пути проекта:
[STARTUP-SCENE.md](STARTUP-SCENE.md); каталог: [COMPONENT-GENERATION.md](COMPONENT-GENERATION.md).

## Запуск

```json
{
  "name": "MyGame",
  "module": "Game.dll",
  "content": "Content",
  "startupScene": "Content/Scenes/Main.scene.json"
}
```

```powershell
EngineRuntime.exe --project Projects/MyGame/Project.json --mode game --frames 3
```

startupScene по-прежнему необязателен и считается от папки Project.json.
Теперь он действует и в game mode, и в editor --scene-api. Legacy editor без флага
не открывает этот файл. Без поля game mode сохраняет прежний lifecycle.
Runtime не выбирает другой файл при ошибке и не создаёт вместо него пустую сцену.

## Порядок и обязанности

```text
Загрузка DLL → create → запрос GameSceneApi → loadScene → start
                                                        ↓
                                              update (каждый кадр)
                                                        ↓
                                                  stop → destroy
```

Запрос расширения и loadScene выполняются только при заданной startupScene.

| Фаза | Ответственность игрового модуля |
|---|---|
| create | Создать сессию, каталог и контейнер сцены; не запускать симуляцию |
| loadScene | Восстановить данные через каталог в сцену этой сессии; не вызывать Tick |
| start | Использовать уже готовые объекты, настроить игровые системы и ресурсы |
| update | Один раз обновить активную игровую сцену, затем выполнить остальную работу кадра |
| stop | Остановить запущенные системы |
| destroy | Освободить сессию, Scene и компоненты, даже если start никогда не вызывался |

Если старый start строил сцену вручную, отделите этот fallback от инициализации
систем. Для fallback используйте собственный флаг успешной загрузки, а не проверку
числа объектов: пустой JSON-документ тоже является успешно загруженной сценой.
Не очищайте загруженную сцену и не создавайте те же объекты повторно в start.

Runtime вызывает только ModuleApi.update. Дополнительного Scene.Tick или
EditSessionApi.frame для game mode нет. Если ваш Game уже обновляет Scene в своём
кадре, нельзя дополнительно вызывать Scene.Tick из callback update.
В оконном RuntimeExample update вызывает RunRuntimeFrame(dt), а Tick выполняется внутри Game ровно один раз.
В этапе 043 это был пример логики без нового окна/renderer; этап 044 добавляет standalone
графическое окно. У игры с renderer загружать нужно именно сцену, которой владеет
её renderer, а не отдельный параллельный список компонентов.

## Необязательный ABI игрового DLL

Заголовок: `Engine/Runtime/GameSceneApi.h`.

```cpp
extern "C" __declspec(dllexport) int __cdecl EngineGetGameSceneApi(
    uint32_t version, uint32_t outputSize,
    void* moduleSession, EngineGameSceneApi* output);

struct EngineGameSceneApi {
    uint32_t version; // ENGINE_GAME_SCENE_API_VERSION = 1
    void* context;
    uint32_t (__cdecl *loadScene)(void* context, const char* absolutePathUtf8,
                                 char* error, uint32_t capacity);
};
```

Проверяйте version, sizeof таблицы, session и output. При отказе возвращайте 0,
не меняя output. При успехе возвращайте 1 и полную таблицу.
session — тот же указатель, который вернул ModuleApi.create; отдельная edit-сессия
не создаётся. context и функции принадлежат DLL и недействительны после destroy/unload.

Вызовы синхронны на потоке сессии. Через ABI не передаются STL, C++-исключения или
владение выделенной памятью. error принадлежит вызывающей стороне; допустимо nullptr/0.
При положительной capacity нужен буфер указанного размера, диагностика заканчивается
NUL и может быть обрезана. Возвращайте EngineSceneStatus, а не bool.
Основные статусы: Ok, InvalidArgument, UnknownType, InvalidDocument, IoError,
UnsupportedVersion, Failure, WrongThread, Busy и ReadOnly.

Контракт loadScene: только между create и первым вызовом start. После start, в том
числе после stop, возвращается ReadOnly. В RuntimeExample это отдельный startCalled,
а не только started. Повторные загрузки до start допустимы; неудачная загрузка
сохраняет прежнюю сцену. Вложенные вызовы блокируются Busy.

ModuleApi остаётся v4, SceneModuleApi v1 и edit API v2 не меняются.
EngineGetSceneApi по-прежнему запрашивается **после успешного start**.
Старый DLL без нового расширения запускается без startupScene. При наличии поля
отсутствующий, несовместимый или неполный GameSceneApi приводит к ошибке запуска.

## Реализация загрузчика

Полный работающий пример: `Game/RuntimeExample/Source/Module.cpp`.
После этапа 044 сессия владеет unique_ptr<Game>, а Scene принадлежит этому Game; SceneCommands остаётся стабильным сервисом.
Основная операция внутри callback выглядит так:

```cpp
auto data = SceneDocument::Read(absolutePath);
auto candidate = std::make_unique<Game>();
SceneDocument::Restore(data, generatedCatalog, candidate->GetScene());
commands.Rebind(candidate->GetScene());
game.swap(candidate);
```

До этого проверьте поток, состояние, аргументы и абсолютный UTF-8 путь.
Перехватите SceneDocumentError и верните его status/what; остальные исключения
преобразуйте в Failure. Не меняйте старую сцену до завершения Restore.
Restore проверяет структуру и все type keys, вызывает фабрики пользовательских
классов, назначает transforms, восстанавливает родителей вторым проходом.
Сценой и объектами владеет игровой DLL. Их уничтожение также происходит в нём.

Если требуется GPU-подготовка, выполните её в предусмотренной вашей игрой фазе
инициализации. Конструкторы каталожных типов не должны требовать начавшейся игры.
Сериализация свойств, ресурсы нового формата и callbacks BeginPlay/EndPlay этим
изменением не добавляются. Произвольные C++ поля получают значения конструктора.

## Ошибки, cleanup и перезагрузка

При ошибке loadScene runtime не вызывает start/update/stop, но вызывает destroy
для уже созданной сессии и выгружает DLL. Поэтому cleanup в destroy должен работать
для любой частичной инициализации. Ошибка start также приводит к destroy без stop:
это существующий контракт ModuleApi, stop предназначен для успешно запущенной игры.
При штатном завершении порядок: stop → destroy → unload.

Диагностика содержит путь startupScene, status и сообщение загрузчика. В частности,
неизвестный тип не пропускается: запуск целиком завершается с ненулевым результатом.
При отсутствии поддержки API сообщение сообщает о требуемом EngineGetGameSceneApi.

Существующая перезагрузка game DLL создаёт новую сессию и снова загружает startupScene
из того же файла перед start. Это перезапуск из начальных данных, не перенос живого
состояния игры. При отказе старое игровое состояние уже уничтожено; ошибочная новая
сессия освобождается. Edit-сессии по-прежнему запрещают этот путь reload.

## Отличие от редакторского Play

Game mode имеет одну игровую сцену и начинает выполнять её логику с первого update.
В editor --scene-api существует исходная edit и отдельная копия при Play; Stop
возвращает edit. Оба пути используют SceneDocument::Restore и один каталог типов,
но сохраняют свои жизненные циклы. Game mode не вызывает edit API или обычный
игровой start поверх редакторской Play-копии.

## Проверки

Набор `Engine/tests/edit-session/run.ps1` включает GameStartupTests.h: загрузку
настоящего игрового DLL, create/load/start, отсутствие Tick при загрузке/start,
точное изменение x на dt при каждом update, запрет загрузки после start/stop,
сохранность сцены при неизвестном типе, поток вызова, совместимость старого DLL,
отказы runtime до start и destroy без stop. Также сохраняются тесты editor API.
Прогоны выполняются без Editor/Qt. Проверена сборка Debug MSVC v143.

Текущий renderer и непрерывный кадр: [GAME-SCENE-RENDERING.md](GAME-SCENE-RENDERING.md).

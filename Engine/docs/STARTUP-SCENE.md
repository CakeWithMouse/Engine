# Стартовая сцена в Project.json

Этапы 042–043. Только Engine/runtime; Editor и EngineProperty не изменялись.
Формат самой сцены: [SCENE-JSON.md](SCENE-JSON.md).
Подключение потребителя: [EDIT-SESSION-API.md](EDIT-SESSION-API.md).

## Настройка проекта

```json
{
  "name": "MyGame",
  "module": "Game.dll",
  "content": "Content",
  "startupScene": "Content/Scenes/Main.scene.json"
}
```

startupScene необязателен. Если его нет, edit-сессия создаётся пустой, как прежде.
Если поле есть, требуется непустая строка UTF-8 без NUL, CR и LF. Нельзя задавать
null, число или пустую строку для отключения: удалите поле.

Путь считается **от каталога Project.json**, не от content, EngineContent,
исполняемого файла или текущего рабочего каталога. В примере `Content` уже входит
в строку startupScene. Разрешены относительные пути с `.` и `..`; путь лексически
нормализуется. Это не ограничение доступа каталогом проекта.
Абсолютные, UNC и Windows drive-relative пути (`C:scene.json`) отклоняются.
Удобнее использовать `/`; для `\` в JSON нужно экранирование `\\`.

Правила остальных полей сохраняются: обязательные name/module/content, необязательный
preview. startupScene и preview могут присутствовать вместе. Неизвестные и повторные
ключи отклоняются. Project.json — UTF-8, допускается BOM, лимит 64 KiB; каталог
content должен существовать. Проверка структуры Project.json выполняется во всех
режимах, но сам файл сцены открывается только в режиме ниже.

## Где поле действует

```powershell
EngineRuntime.exe --project Projects/MyGame/Project.json --mode editor --scene-api
```

| Запуск | Поведение startupScene |
|---|---|
| `--mode editor --scene-api` | Загружается до передачи API потребителю |
| `--mode editor` без флага | Файл сцены не открывается, прежний preview |
| `--mode game` | Загрузка между create и start через GameSceneApi |
| `--mode game --scene-api` | Ошибка: флаг разрешён только в editor mode |

Относительный аргумент `--project` по-прежнему считается от каталога EngineRuntime.exe.
После нахождения Project.json его startupScene считается уже от папки проекта.
При развёртывании нужно скопировать файл сцены по указанному относительному пути.
Этот этап не добавляет упаковщик и не меняет Project.json существующих примеров.

Настоящий Editor пока не реализует EngineBindEditSession. Поддержка startupScene
сама по себе не подключает его UI к новому API: это остаётся задачей разработчика
редактора. Без совместимого потребителя запуск `--scene-api` завершится ошибкой.

## Порядок запуска и текущий API

Ниже — порядок editor --scene-api. Порядок game mode и контракт автора игры описаны в [GAME-STARTUP-SCENE.md](GAME-STARTUP-SCENE.md).

1. ReadRuntimeProject разбирает описание, разрешает путь и помещает его в
   `RuntimeProject::startupScene`. На этом этапе сцена ещё не читается.
2. Main передаёт путь в `RuntimeLaunchOptions::startupScene` при `--scene-api` и в game mode.
3. Runtime создаёт GameEditModule: загружает игровой DLL, получает каталог и edit API v2.
   Обычные игровые create/start/update при этом не вызываются.
4. Если путь задан, runtime вызывает существующий `api.loadScene` с абсолютным UTF-8
   путём. Отдельного парсера или способа создания объектов для запуска нет.
5. После успеха создаётся потребитель, вызываются EngineBindEditSession и start.
   В переданной таблице `api.scene` уже находится загруженная сцена в состоянии Editing.
6. Потребитель читает snapshot и может подключить viewport. Первый кадр статичен.

ABI не меняется: edit API остаётся v2, ModuleApi v4, SceneModuleApi v1.
Добавлено только host-поле RuntimeLaunchOptions, оно не передаётся через DLL ABI.
При программном запуске задайте mode=Editor, editModule и абсолютный startupScene.
В game mode путь передаётся игровому GameSceneApi; legacy editor без editModule его не использует. Относительный путь в host-поле
не разрешается повторно, а отклоняется существующим loadScene.

Потребителю **не нужно самостоятельно повторно загружать startupScene** в bind/start.
Он получает готовую сцену и строит дерево из readScene. Текущий путь документа/dirty,
выделение, история команд и диалоги по-прежнему относятся к редактору. Отдельного
getter текущего пути в edit API нет: потребитель может взять его из Project.json,
путь к которому уже передаётся в EngineModuleHost.projectFile, или хранить в своём
контроллере документа. Не сохраняйте строковые указатели за пределами их lifetime.

## Ошибки и завершение

Если файл отсутствует, JSON повреждён, версия неизвестна или каталог не содержит тип,
runtime прекращает запуск с ненулевым результатом. В диагностике есть startupScene,
абсолютный путь, числовой status API и пояснение загрузчика. Пример:

```text
RuntimeError: Cannot load startupScene '.../Main.scene.json' (status 2): Unknown component type: MyGame::OldType
```

Пустая сцена вместо ошибочной не открывается. Потребитель ещё не создан, bind/start
не вызываются. Созданные ресурсы edit-сессии и DLL освобождаются обычным Stop/RAII.
После исправления файла можно запустить проект снова.

Правила загрузчика сохраняются: полная проверка до замены, неизвестный тип — ошибка,
без частичного восстановления. Пользовательские конструкторы должны быть edit-safe.
startupScene не является автосохранением: runtime никогда не перезаписывает файл
при открытии, Play, Pause, Stop или завершении приложения.

## Play и последующее редактирование

После запуска редактируется загруженная Scene. Play создаёт отдельную копию её
текущего состояния, включая несохранённые правки. Файл при Play не перечитывается.
Pause замораживает копию на месте. Stop уничтожает копию и возвращает исходную edit.
Save/Save As остаются существующим saveScene: даже в Play/Pause сохраняется edit.
Последующее открытие другого файла выполняется обычным loadScene в Editing;
значение startupScene в Project.json автоматически не меняется.

## Проверки без редактора

```powershell
./Engine/tests/edit-session/run.ps1 -Configuration Debug -PlatformToolset auto
```

508 проверок общего набора, включая новый StartupSceneTests.h и настоящий Project.cpp:
необязательное поле, Unicode-путь, независимость от CWD, недопустимые значения,
наличие данных уже в bind/start, сохранность edit через Play/Pause/Stop, ошибки файла,
типа и версии до создания потребителя, повторный запуск после отказа, game mode
с отсутствующей startupScene. Потребитель — тестовый DLL без Editor/Qt.
Main.cpp отдельно скомпилирован MSVC v143; полноценный UI редактора не собирался
и не запускался. Автозагрузка game mode добавлена в этапе 043: см. [GAME-STARTUP-SCENE.md](GAME-STARTUP-SCENE.md).

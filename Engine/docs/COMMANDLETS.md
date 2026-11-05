# Проверки без интерактивного редактора

Создание/сборка проектов также доступны через [EngineLauncher CLI](LAUNCHER-CLI.md). Его интеграционные регрессии: `Game/Launcher/Tests/CliTests.ps1` (PowerShell 7).

`EngineCommandlets.exe` — локальные параметризованные вызовы существующего Engine/Editor API. Отдельная DLL редактора и процесс EngineRuntime в режиме editor не запускаются. Для UI создаются настоящие Qt-виджеты на offscreen backend; для игровых команд используется скрытый HWND и настоящее D3D11 устройство. Это не обещание работы без GPU. Аналогичный принцип отдельных служебных запусков используется в [UE Commandlets](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UCommandlet).

## Сборка и протокол

### SaveGame (2026.11.05.0)

Команды `savegame.save`, `savegame.load`, `savegame.validate`, `savegame.list`,
`savegame.delete` вызывают настоящий SaveGame API. Запрос: `root` (обязательный абсолютный
scratch-каталог), `gameId` (по умолчанию `commandlet`), `slot` (по умолчанию `manual-1`).
`savegame.save` также принимает `scene` — обычный JSON-документ сцены. Сохранение и загрузка
возвращают `state`, список — `slots`, удаление — `deleted`. Ошибка даёт `ok:false`, `error`
и exit 2. Используется каталог встроенных типов; классы пользовательской DLL проверяются
в проектном host с его настоящим каталогом. GPU для этих пяти команд не нужен.

Регрессия: `Editor/tests/SaveGameCommandletTests.ps1`. Проверяет разные процессы,
кириллицу, список/удаление, повреждённый файл, traversal и отказ записи с сохранением слота.

### Общие команды

Из корня репозитория:

```powershell
& Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -EngineCommandlet
$tool = 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'
& $tool --list
& $tool --help
'{"origin":[0,0,5],"direction":[0,0,-1],"distance":10}' | Set-Content Engine/build/ray.json -Encoding utf8
& $tool trace.raycast Engine/build/ray.json Engine/build/ray-result.json
```

Вход — UTF-8 JSON object в файле. Результат — UTF-8 JSON в указанном файле (`ok` и данные операции). stdout может содержать служебные логи Engine: для автоматизации читать файл результата. Exit 0 — вызов выполнен, 1 — C++ build отклонён, 2 — некорректный запрос/операция/неизвестная команда, 3 — результат невозможно записать. Ошибка отдельного API, например отклонение свойства, также возвращается в `status`: это позволяет проверять негативные контракты. Родительская папка результата должна существовать.

## Команды

Все координаты — числа. Векторы — массивы из трёх чисел, quaternion — `[x,y,z,w]`.

| Команда | Вход / значения по умолчанию | Наблюдаемый результат |
|---|---|---|
| `trace.raycast` | `origin:[0,0,5]`, `direction:[0,0,-1]`, `distance:10`, `position:[0,0,0]` | Настоящий Scene::Raycast по треугольнику (-1,-1,0),(1,-1,0),(0,1,0); hit, distance, position, normal |
| `camera.screen-ray` | `origin`, `direction`, `width:200`, `height:100`, `x/y`: центр, `fov:60`, `orthographic:false`, `orthoSize:10` | ScreenPointToRay через реальные матрицы камеры; status, origin, direction. near=0.1, far=1000 |
| `scene.property` | `type:"Gameplay.Camera"`, необязательные `path:"/fov"`, `value:75` | Создание объекта в SceneEditSession, schema, status изменения и сериализованные data; поддерживает и другие зарегистрированные builtin-типы |
| `session.lifecycle` | `scenes:2`, `frames:3`, `dt:0.02`, `tickEnabled:true` | Настоящие GameSession/Game и тестовый счётчик GameSubsystem: ready, closing, ticks, elapsed, running после Stop |
| `game.settings` | `rendering:0` (Forward=0, Deferred=1, Custom=2), `vsync:true`, `antialiasing:0` | Изменяет и читает настройки конкретного Game; визуальный результат проверяется отдельными GPU-тестами |
| `mesh.primitive` | `shape:"cube"` (также sphere/plane/cylinder/cone/pyramid), `size:1`, `slices:16` | Штатные MeshPrimitives: vertices, triangles, minimum/maximum; CPU-геометрия без загрузки модели |
| `asset.import` | абсолютные `source` (модель) и `content` (каталог назначения) | Штатный importAsset и createModel через GameEditModule; asset, placed, material, meshes, indices, clips. **Копирует модель и материалы в content**. Требует Assimp |
| `gizmo.snap` | `delta`, `step:0.5` | SnapVector; результат delta |
| `gizmo.center` | `delta`, `position:[0,0,0]`, `rotation:[0,0,0,1]`, `snap:true`, `step:0.5`, `local:false` | Та же операция свободного drag, которую вызывает ViewportWidget; итоговая position |
| `ui.inspector` | `width:310`, `height:600`, необязательный `image` | Настоящий Inspector с выбранным кубом: достижимость всех полей Transform, геометрия и диапазон скролла, PNG |
| `ui.style` | необязательный `image` | Реальный stylesheet применяется к SpinBox/ComboBox; warnings и PNG, генерируются обе стрелки |
| `ui.edit-text` | `file`, `element`, `text` | Открывает UIEditorWindow, задаёт активный текст без editingFinished, вызывает Save; возвращает сохранённый XML. **Изменяет указанный файл** |
| `input.rebind` | `focused:true/false` | Через очередь EditorQtHost отправляет InputFocus, заменяет API и измеряет доставку фокуса новой сессии |
| `project.build` | абсолютный `project` (Project.json) | Настоящее действие Recompile/Hot Reload на offscreen EditorMainWindow, рабочий C++ SDK и GameEditModule; console, dialogs, windowEnabled, buildSucceeded, replaced. **Собирает и обновляет проект** |
| `project.reload-input` | `project`, `candidate` (другой путь DLL), `key:84`; необязательные `clickX/clickY` | Один Focus до первого Play; Play → key/click → Stop → Reload → Play → key/click. Снимки объектов before/afterKey/afterClick до и после DLL. Viewport 640×480, стандартные игровые callbacks |

При `project.build` закрытый read handle Project.json не мешает атомарной активации. Команда не использует отдельную упрощённую реализацию сборки: QAction вызывает штатный CodeTools. Сборка ограничена 180 сек. Проверки запускать на scratch-проектах. `project.reload-input` сам не сохраняет проект, но создаёт временный checkpoint через editor history; candidate должен иметь другую файловую идентичность.

UI-команды требуют поставляемые Qt DLL и `platforms/qoffscreen.dll`. Offscreen backend Windows загружает Segoe UI из Windows/Fonts для читаемого PNG. UI preview использует D3D WARP для атласа шрифта. Project-команды требуют D3D11, EngineContent, Templates и SDK рядом с exe, MSVC для build. `asset.import` требует Assimp: первая сборка `Engine/build.ps1 -Configuration Release -Target EngineLauncher -EnableAssimp $true` устанавливает пакет из закреплённого vcpkg manifest. Следующие сборки автоматически включают установленный Assimp; явный `-EnableAssimp:$false` отключает его. DLL импортёра входит в Runtime, библиотеки — в SDK.

## Регрессии жалоб

```powershell
& Editor/tests/CommandletTests.ps1
& Editor/tests/CppWorkflowTests.ps1
& Editor/tests/AssimpCommandletTests.ps1
# Generate real Blender fixtures and run import, GPU and commandlet checks:
& Engine/tests/assets/run-blend.ps1
# Для сценария исходной игры «Хлам в ведро»:
& Editor/tests/ReloadInputTests.ps1 -Project 'E:/scratch/Trash/Project.json'
```

CommandletTests: BUG001/002/003/007/008, параметры камеры, typed properties, Raycast Hit/Miss, Session lifecycle, local/world/disabled/zero Snap и ошибочные запросы. CppWorkflowTests создаёт новый проект, вызывает **launcher --enable-code** с окружением PS7, намеренно ломает C++ с кириллическим #error, проверяет Console/старую DLL, исправляет исходник и проверяет новую DLL в том же сеансе commandlet. BUG004/005/006 проверяются штатными production-путями. Новый output-каталог обязателен для CppWorkflow/ReloadInput, чтобы не перетирать чужой проект.

ReloadInputTests — отдельный сценарий жалобы BUG007 на проекте роли 4: до reload и после него должно быть 0 → 1 → 2 Trash.Projectile. Сама команда универсальна и возвращает объекты, а знание игры и ожидаемые числа находятся только в сценарии теста.

AssimpCommandletTests импортирует OBJ, FBX и анимированный GLB в каталоги с кириллицей, размещает модели и проверяет отказ повреждённому FBX без оставшегося каталога модели. `Engine/tests/assets/run.ps1` дополнительно проверяет материалы, текстуры, перенос сцены и реальные GPU-пиксели, сохраняя `imported-model.bmp`; `Engine/tests/skeletal/run.ps1` проверяет загрузку, запекание анимации и LOD.

`Engine/tests/assets/run-blend.ps1` находит установленный Blender (либо принимает `-Blender <blender.exe>`), создаёт fixtures с внешней/упакованной/отсутствующей текстурой в пути с кириллицей, запускает asset-тесты и `AssimpCommandletTests -BlendFixtures <каталог>`. Последний также принимает `-Commandlet <exe>` для проверки установленного Runtime. Для BLEND геометрия измеряется по полученному GLB, а не по исходному файлу Blender. Новый каталог `-Output` обязателен, если указан явно. Рендер сохраняет `blend-external.bmp` и `blend-packed.bmp` в перенесённом тестовом проекте.

Связанные наборы: `Engine/tests/run.ps1`, `Engine/tests/input/run.ps1`, `Engine/tests/subsystems/run.ps1`, Editor CoreTests/HostTests. Основной runner пересобирает RuntimeFixtures, чтобы Module API не устаревал. Без Assimp импортная часть AuthoringTests явно SKIP после проверки доступных свойств/текстур и корректного отказа импортёра; она не учитывается как passed. С `-EnableAssimp` выполняется полный сценарий моделей.

## Добавление проверки следующего релиза

Добавить в dispatcher параметризованный вызов настоящей операции, описать вход/побочные эффекты/зависимости здесь и добавить сценарии в tests. ID жалобы принадлежит сценарию, а не API-команде. Не переносить игровую логику в Engine и не подменять неподдерживаемую возможность тестовой заглушкой. Сохранять before/after, код завершения и проверяемый эффект, а для отображения — PNG и осмотр.

Диагностика компиляции использует [MSBuild UTF-8 file logger](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference?view=visualstudio#switches-for-loggers), затем UTF-8 PowerShell stdout и QString::fromUtf8 с буферизацией неполных строк. Так не приходится угадывать OEM/ANSI кодировку на границе Qt.

# Проверки без интерактивного редактора

Создание/сборка проектов также доступны через [EngineLauncher CLI](LAUNCHER-CLI.md). Его интеграционные регрессии: `Game/Launcher/Tests/CliTests.ps1` (PowerShell 7).

`EngineCommandlets.exe` — локальные параметризованные вызовы существующего Engine/Editor API. Отдельная DLL редактора и процесс EngineRuntime в режиме editor не запускаются. Для UI создаются настоящие Qt-виджеты: обычно offscreen, а для материала — скрытое native Qt окно с HWND превью. Для игровых команд используется скрытый HWND и настоящее D3D11 устройство. Это не обещание работы без GPU или Windows desktop session. Аналогичный принцип отдельных служебных запусков используется в [UE Commandlets](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UCommandlet).

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

### Игровой ввод и диагностика рендера

`project.input` загружает настоящую игровую DLL из `project` (абсолютный путь к
scratch `Project.json`) и передаёт события через EditorQtHost. Каждый шаг имеет
обязательное поле **`kind`**. Пример удержания W, отпускания и потери фокуса:

```json
{"project":"E:/scratch/Project.json","steps":[
  {"kind":"control","action":"play"},
  {"kind":"focus","enabled":true},
  {"kind":"frames","count":3},
  {"kind":"key","key":87,"down":true},
  {"kind":"frames","count":30},
  {"kind":"focus","enabled":false},
  {"kind":"frames","count":10},
  {"kind":"key","key":87,"down":false}
]}
```

`control.action`: `play`, `pause`, `resume`, `stop`. `key` — значение **Engine Keys**
0..507 (W=87, LeftButton=500, RightButton=501). Для клика сначала передать
`{"kind":"cursor","x":95,"y":131,"valid":true}`: viewport 640×480.
`frames.count`: 0..600, суммарно не более 2000; шагов не более 100.
Относительное движение мыши: `{"kind":"pointer","delta":[60,0,0]}`
(x/y и колесо), через штатный viewport input API.
Результат `steps[].objects` содержит реальные имена, типы и позиции объектов
после каждого шага. При завершении вызывается Stop. Игровые обработчики могут
записывать свои SaveGame — используйте отдельный проект и каталог сохранений.

`render.diagnostics` создаёт скрытый D3D viewport без игровой DLL и отрисовывает
параметризованный InstancedMesh из кубов. Пример проверки ортографического LOD:

```json
{"width":640,"height":480,"screenHeights":[50,20],
 "instances":[{"position":[0,0,0],"scale":1,"visible":true}],
 "views":[{"position":[0,8,0],"orthographic":true,"orthoHeight":24},
          {"position":[0,24,0],"orthographic":true,"orthoHeight":24}]}
```

Камера по умолчанию смотрит вниз `[0,-1,0]`, up `[0,0,1]`, FOV=60.
Дополнительно: `lodCount` 1..4, `distances` (по умолчанию `[9,17]`),
`drawDistance:60`, `fade:0`, `rendering:0`, `panel:true`. `views` — до 32,
`instances` — до 10000. `image` — новый абсолютный BMP-путь с существующим
родителем; существующий файл не перезаписывается. Результат `frames[]` содержит
снимки завершённого кадра, причины отсечения, LOD-корзины и строки самой панели.
Это CPU-счётчики подачи команд, не GPU timing. Регрессия:
`Editor/tests/RenderDiagnosticsCommandletTests.ps1`.

### Граф материала через настоящее окно Qt

`material.graph` принимает `project`, `material` (путь `/Game/...mat`), `steps`
и необязательный PNG `image`. Открывает MaterialEditorWindow с полной темой,
живым Engine host и превью; использует скрытое окно Qt windows backend, требует D3D.
Offscreen Qt не предоставляет HWND для native preview; команда проверяет его
и возвращает `nativePreviewWindow:true`. PNG снимает Qt canvas/форму и не
захватывает пиксели native swapchain превью.

```json
{"project":"E:/scratch/Project.json","material":"/Game/Materials/Stone.mat",
 "steps":[{"action":"enable"},{"action":"preset","index":1},
          {"action":"apply"},{"action":"revert"}]}
```

Доступны `enable`, `disable`, `preset` (индекс штатного пресета), `apply`,
`ai` (`text` — ответ с JSON графа), `undo`, `redo`, `remove` (`ids`), `save`,
`revert`. Вызов автоматически нажимает Yes при явно запрошенном Revert.
`steps[].accepted`, `hasGraph`, `dirty`, `graph`, `hlsl` отражают состояние
настоящего редактора; `dialogs` сохраняет тексты его сообщений.
Для воспроизведения отказа записи у шага можно указать абсолютный `lockedFile`:
команда держит read-only handle без разрешения записи/удаления до конца шага.
Выбирайте файл, который изменяет именно этот шаг: для сохранения только графа
это существующий `.mat.graph.json` sidecar. Блокировка неизменяемого HLSL
не доказывает отказ записи графа.
`save` ждёт завершения асинхронной операции; при отказе возвращает
`accepted:false`. Сам выполненный сценарий может вернуть exit 0, даже если
операция была ожидаемо отвергнута — проверяйте состояния и `dialogs`.
**Операции меняют файлы указанного материала.** Для тестов используйте копию
проекта. Для отката несохранённых изменений явно добавляйте `revert`.

### Операции редактора над сценой

`project.scene` использует ту же очередь EditorQtHost, историю и batch API,
что команды редактора. Вход: `project` и до 100 `steps`. Каждый шаг имеет
`action`; объект выбирается по уникальному `target` (имя). Доступны:

- `create`: `type` и `name`; `delete`, `duplicate`, `rename` (`name`).
- `model`: `path` к уже импортированной `/Game/Models/...` модели и `name`; штатное размещение с её слотами.
- `select`: `target` или `targets:["A","B"]` для группы.
- `move`: `target`, `delta:[2,0,0]`; применяет настоящий групповой nudge.
- `parent`: `target`, `parent` (пустая строка снимает родителя).
- `property`: `target`, JSON-pointer `path`, JSON `value`.
- `material`: `target`, `path` к материалу.
- `save`/`load`: абсолютный `path` сцены; `new`, `undo`, `redo`.

Каждый объект результата содержит также `material` (общий override) и `projectData`,
включая `materialSlots`. Это позволяет сравнить назначения до/после изменения и Undo/Redo.

Результат `steps[]` содержит `status`, `error`, реальные `objects` с ID/parent/
локальной позицией, `selected`, `canUndo`, `canRedo`. Неуспех отдельной операции
сохраняется в её `status`, проверяйте его даже при exit 0 сценария.
Изменения существуют в открытой сессии до явно запрошенного `save`.
В restricted Windows host установите writable `TEMP`/`TMP`, как для hot reload.
`SceneCommandletTests.ps1 -Project <scratch Project.json>` проверяет 16 операций:
группу, nudge, историю, иерархию, duplicate, Save As/New/Load и удаление.

Все координаты — числа. Векторы — массивы из трёх чисел, quaternion — `[x,y,z,w]`.

| Команда | Вход / значения по умолчанию | Наблюдаемый результат |
|---|---|---|
| `trace.raycast` | `origin:[0,0,5]`, `direction:[0,0,-1]`, `distance:10`, `position:[0,0,0]` | Настоящий Scene::Raycast по треугольнику (-1,-1,0),(1,-1,0),(0,1,0); hit, distance, position, normal |
| `camera.screen-ray` | `origin`, `direction`, `width:200`, `height:100`, `x/y`: центр, `fov:60`, `orthographic:false`, `orthoSize:10` | ScreenPointToRay через реальные матрицы камеры; status, origin, direction. near=0.1, far=1000 |
| `scene.property` | `type:"Gameplay.Camera"`, необязательные `path:"/fov"`, `value:75` | Создание объекта в SceneEditSession, schema, status изменения и сериализованные data; поддерживает и другие зарегистрированные builtin-типы |
| `session.lifecycle` | `scenes:2`, `frames:3`, `dt:0.02`, `tickEnabled:true` | Настоящие GameSession/Game и тестовый счётчик GameSubsystem: ready, closing, ticks, elapsed, running после Stop |
| `game.settings` | `rendering:0` (Forward=0, Deferred=1, Custom=2), `vsync:true`, `antialiasing:0` | Изменяет и читает настройки конкретного Game; визуальный результат проверяется отдельными GPU-тестами |
| `mesh.primitive` | `shape:"cube"` (также sphere/plane/cylinder/cone/pyramid), `size:1`, `slices:16` | Штатные MeshPrimitives: vertices, triangles, minimum/maximum; CPU-геометрия без загрузки модели |
| `asset.import` | абсолютные `source` (модель) и `content` (каталог назначения) | Штатный importAsset и createModel через GameEditModule; asset, placed, material (общий override), materialSlots (source ID → .mat), meshes, indices, clips. **Копирует модель и материалы в content**. Требует Assimp |
| `gizmo.snap` | `delta`, `step:0.5` | SnapVector; результат delta |
| `gizmo.center` | `delta`, `position:[0,0,0]`, `rotation:[0,0,0,1]`, `snap:true`, `step:0.5`, `local:false` | Та же операция свободного drag, которую вызывает ViewportWidget; итоговая position |
| `ui.inspector` | `width:310`, `height:600`, необязательный `image` | Настоящий Inspector с выбранным кубом: достижимость всех полей Transform, геометрия и диапазон скролла, PNG |
| `ui.style` | необязательный `image` | Реальный stylesheet применяется к SpinBox/ComboBox; warnings и PNG, генерируются обе стрелки |
| `ui.edit-text` | `file`, `element`, `text` | Открывает UIEditorWindow, задаёт активный текст без editingFinished, вызывает Save; возвращает сохранённый XML. **Изменяет указанный файл** |
| `input.rebind` | `focused:true/false` | Через очередь EditorQtHost отправляет InputFocus, заменяет API и измеряет доставку фокуса новой сессии |
| `project.build` | абсолютный `project` (Project.json) | Настоящее действие Recompile/Hot Reload на offscreen EditorMainWindow, рабочий C++ SDK и GameEditModule; console, dialogs, windowEnabled, buildSucceeded, replaced. **Собирает и обновляет проект** |
| `project.reload-input` | `project`, `candidate` (другой путь DLL), `key:84`; необязательные `clickX/clickY` | Один Focus до первого Play; Play → key/click → Stop → Reload → Play → key/click. Снимки объектов before/afterKey/afterClick до и после DLL. Viewport 640×480, стандартные игровые callbacks |

При `project.build` закрытый read handle Project.json не мешает атомарной активации. Команда не использует отдельную упрощённую реализацию сборки: QAction вызывает штатный CodeTools. Сборка ограничена 180 сек. Проверки запускать на scratch-проектах. `project.reload-input` сам не сохраняет проект, но создаёт временный checkpoint через editor history; candidate должен иметь другую файловую идентичность.

Для restricted Windows host укажите `TEMP` и `TMP` на существующий scratch-каталог,
если системный временный каталог запрещает запись checkpoint (Windows error 5).
`ReloadInputTests.ps1` делает это автоматически и восстанавливает окружение.

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

С 2026.11.05.6 этот runner также создаёт Suzanne (встроенную модель Blender), вариант с двумя
материалами и четырёхцветную UV-плоскость. `ImportedModelRenderTests.cpp` проверяет реальные пиксели
BaseMap/текстуры объекта, тень на принимающей поверхности в Custom/Forward/Deferred, глубину каскадов,
masked/castShadows, текущую позу костей и статический кэш/yaw. Сохраняются `uv-*.bmp` и `suzanne-*.bmp`.
Без Blender-fixtures эта интеграционная часть пропускается; обычные FBX/GLB-тесты остаются доступными.
С 2026.11.05.7 вариант с двумя материалами проверяет автоматические submesh-назначения,
разные blend/shadow states, замену одного слота, reload, сцену и SaveGame. Полный Blender/PBR
не переносится. Через `project.scene` используйте `action: "property"`, `path: "/materialSlots/<ID>"`,
`value: "/Game/Materials/Example.mat"`; это штатная история редактора, включая Undo/Redo.
Общее назначение `action: "material"` остаётся override всех слотов.

`Editor/tests/MaterialSlotsCommandletTests.ps1 -Project <scratch/Project.json> -Model <model.blend> -Output <new-directory>`
проверяет импорт нескольких слотов, назначение, Undo/Redo, атомарный отказ неверного пути,
дублирование, сохранение/загрузку и общий override. Передавайте отдельный тестовый проект:
runner добавляет импортированные ресурсы. Модель должна содержать минимум два материала;
внешние зависимости должны лежать рядом с ней, либо используйте BLEND с упакованными текстурами.
Для другого runtime предусмотрен `-Commandlet <EngineCommandlets.exe>`.

Связанные наборы: `Engine/tests/run.ps1`, `Engine/tests/input/run.ps1`, `Engine/tests/subsystems/run.ps1`, Editor CoreTests/HostTests. Основной runner пересобирает RuntimeFixtures, чтобы Module API не устаревал. Без Assimp импортная часть AuthoringTests явно SKIP после проверки доступных свойств/текстур и корректного отказа импортёра; она не учитывается как passed. С `-EnableAssimp` выполняется полный сценарий моделей.

## Добавление проверки следующего релиза

Добавить в dispatcher параметризованный вызов настоящей операции, описать вход/побочные эффекты/зависимости здесь и добавить сценарии в tests. ID жалобы принадлежит сценарию, а не API-команде. Не переносить игровую логику в Engine и не подменять неподдерживаемую возможность тестовой заглушкой. Сохранять before/after, код завершения и проверяемый эффект, а для отображения — PNG и осмотр.

Диагностика компиляции использует [MSBuild UTF-8 file logger](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference?view=visualstudio#switches-for-loggers), затем UTF-8 PowerShell stdout и QString::fromUtf8 с буферизацией неполных строк. Так не приходится угадывать OEM/ANSI кодировку на границе Qt.

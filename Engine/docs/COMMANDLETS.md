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

### Импорт с настройками редактора

`project.import` принимает абсолютные `project` (scratch Project.json) и `source`
(существующая модель), необязательный `scale` (0.0001–10000, по умолчанию 1).
Вызывает настоящий `Assets.Import` в EditorMainWindow, обработчик завершения
с записью `.importsettings.json` и `UseAsset`. Неверный scale даёт `accepted:false`
без импорта. Ошибка самого импорта даёт ненулевой exit и `ok:false`.
Выход: `asset`, `sidecarExists`, `sidecar`, `dialogs`, снимки `before`, `spawned`,
`after` с масштабом объектов. `reimportSource` выполняет настоящий переимпорт и
добавляет ещё один объект; `moveTo` (новый /Game путь, то же имя модели) переносит
папку модели и размещает третий объект. Снимки `afterReimport`/`afterMove` позволяют
проверить сохранение старых объектов и масштаб новых. Команда изменяет scratch
проект. Нужны Qt, D3D и Assimp. `image` сохраняет Qt-раскладку (не доказательство
D3D-картинки), `width`/`height` задают её размер.

Вместо `scale` можно передать `dialog`: `presets` — последовательность имён
`Preset1`, `Preset01`, `Preset001`, необязательное числовое `value`,
`action:"accept"|"cancel"`, необязательный PNG `image`. Это настоящий модальный
ImportSettingsDialog с полной темой: команда нажимает кнопки, читает SpinBox,
ставит фокус и принимает/отменяет диалог. `dialog.presets` результата содержит
наблюдаемые значения после каждого нажатия; Cancel возвращает `accepted:false`
без импорта. PNG показывает диалог, а не игровой viewport.

```powershell
python Editor/tests/ImportSettingsCommandletTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --launcher Game/build/x64/Release/EngineLauncher.exe --output Engine/build/import-settings-new
```

### Движение точечных теней

`render.point-shadows` создаёт приёмник, одну лампу и отбрасыватель на настоящем
D3D11 устройстве. `caster`: `mesh`, `skeletal` или `crowd`; последние два по
умолчанию используют куб с движущейся костью (travel=6, duration=1 s), либо
импортируют абсолютный `model` с анимацией. `manualTexture:true` проверяет
низкоуровневый VAT API без известных bounds. `replaceLodOffset:[x,y,z]` заменяет
LOD0 после bake для проверки сброса устаревшего envelope. `rendering`: 0 Forward / 1 Deferred;
`sceneLighting:true` проверяет сцену без солнца. По умолчанию `globalShadows:true`;
в программном режиме без сценового света `false` отключает все тени.
`scale`, `radius`, `intensity`, `camera`, `target`, `width`, `height` задают сцену.

`frames` содержит 1..64 состояния: `time` (секунды первой анимации), `position`
отбрасывателя, `light` (позиция лампы), `shadows` и необязательный `image`
(новый абсолютный BMP, родитель уже существует). `probes` — до 64 мировых
точек на запрос или кадр. Выход: adapter, число активных shadow lights,
SHA-256 и число занятых texels каждой из шести граней, проецированная глубина
проб, raw-depth `occluded`, `directionalShadows` (каскады активны). `occluded` не моделирует PCF/bias
цветового прохода; видимость проверяется отдельно по BMP. Файлы не перезаписываются.

```powershell
python Editor/tests/PointShadowMotionTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/point-motion --model Game/Battle/Content/Models/CesiumMan.glb
```

Скрипт требует новый output-каталог; сохраняет запросы, результаты и кадры.
Он сравнивает анимационные карты с rigid-reference, проверяет возвращение
идентичных кадров и различие shadow-on/off в Forward/Deferred.

Для нескольких источников задайте `lights` (1..4 объектов): `position`, `color`,
`intensity`, `radius`, `enabled`, `shadows`. Порядок регистрации сохраняется.
В каждом кадре необязательный `lights` содержит столько же объектов; указанные
поля обновляются, остальные сохраняются. Это позволяет проверить и третьего
кандидата при лимите двух теневых карт. Старые `light`/`shadows` работают в
одноламповом режиме. Выход `cubes` содержит каждый активный куб с полем `slot`;
верхние `faces`, `probes`, `light` сохранены как данные первого куба.
`lights` в результате кадра показывает позицию и shader slot каждой лампы
(−1 — тени нет). Пробы проверяются отдельно относительно каждого куба.

Для crowd доступны `lodDistances`, `lodScreenHeights`, `lodFade` (0..0.5),
`lodScales` (масштабы диагностических копий исходной геометрии по уровням).
`material:true` назначает стандартный материал, иначе сохраняется legacy path.
`materialAsset` — абсолютный путь `.mat`, загружаемого штатным `Game::LoadMaterial`.
Он имеет приоритет над `material:true`. Кадр может содержать `materialParams`:
объект имя → число или массив 1–4 чисел; значения применяются перед отрисовкой.
Неизвестный параметр или ошибка загрузки/компиляции дают диагностическую ошибку.
Результат каждого кадра с `materialAsset` содержит `material.errors`, `gbuffer`
и `castsShadow`. `Editor/tests/MaterialRenderTests.py` использует этот контракт
для проверки сочетаний материалов по BMP-пикселям и глубине обеих теневых карт.
Кадр может менять `camera`/`target`; поля `lod.perLod`, `distance`, `screenSize`
и `fade` показывают реальный выбор цветового прохода. Два ненулевых buckets
означают cross-fade, а не два разных экземпляра. Обычный skeletal-компонент
LOD не поддерживает; он используется как контроль анимации и слотов.

```powershell
python Editor/tests/PointShadowSlotsLodTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/two-lights --model Game/Battle/Content/Models/CesiumMan.glb
```

### Прозрачные слои и движение камеры

`render.transparency` создаёт реальные D3D11-плоскости и рендерит их обычными проходами
движка. JSON: `rendering` (0 Forward, 1 Deferred), `width`/`height` (по умолчанию 320×256),
`orthographic` (высота 8) и `groups` (1–16 групп). Группа содержит абсолютный путь `.mat`
в `material`, `kind` (`mesh`, `copies`, `instances`, `skeletal`, `crowd`), размеры плоскости
`width`/`height` и 1–512 `layers`: `position:[x,y,z]`, `color:[r,g,b]`, `alpha:0..1`.
У слоя есть `rotation:[pitch,yaw,roll]` в градусах (instances/crowd поддерживают только yaw).
`geometry:"plane"|"cross"` выбирает плоскость либо две пересекающиеся плоскости в одном
мешe; у второй UV.x смещён на 2 для назначения отдельного цвета через Surface.
`copies` использует CreateInstance; `crowd` — настоящую VAT-текстуру анимированной плоскости.
Для batch доступны `lods:1..3` (одинаковая геометрия для проверки неизменности alpha),
`lodDistances`, `lodScreenHeights`, `fade:0..0.5`, `impostor`.

`frames` (1–128): `camera`, `target`, `time`, новый абсолютный BMP `image`; необязательный
`positions` заменяет позиции всех layers в порядке групп. `fadeWeight:0..1` для
orthographic подбирает screen-height threshold, позволяющий проверить точные границы
Bayer-маски. Выход содержит adapter, BMP, per-LOD counts, impostor level и draw calls.
`width`/`height` внутри frame изменяют размер viewport (100..1920 × 100..1080).
Редактор не требуется; нужны Qt, GPU и hidden HWND. Файлы не перезаписываются.

```powershell
python Editor/tests/TransparencyTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/transparency-new
python Editor/tests/TransparencyIntersectionTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/intersections-new
```

Runner проверяет source-over/additive по 256 пикселям каждого кадра; ожидаемый цвет
вычисляется независимо от renderer. Проверяет pan/orbit/dolly, смену порядка движущихся
слоёв, равную глубину, opaque occlusion, разные батчи/LOD, рост stream и cross-fade.
Intersection runner независимо пересекает лучи камеры с повёрнутыми поверхностями и
проверяет цвет по геометрической глубине: оба порядка регистрации, orbit с обеих сторон,
самопересечения, текущую анимацию, additive/opaque/нулевую alpha, resize, 16 глубин и
дальний остаток. Пограничные пиксели силуэта/совпадающей глубины исключаются из численного
сравнения; их BMP остаются для визуального осмотра.

### Частицы и отражённый масштаб меша

`render.particles` использует настоящий ParticleSystemComponent и D3D11:
`depthCollision`, `surface` (bool), `surfacePosition`/`surfaceScale`, `position`,
`baseVelocity`, `acceleration`, `radialSpeed`, `spread`, `lifetime`, `size`,
`brightness`, `bias`, `bounce`, `friction`, `sorting`, `particleCount` (64..4096),
`camera`/`target`, `width`/`height`, `rendering` (0/1). `frames` содержит `steps`
(1..600, суммарно до 2000), `dt` (0..0.1) и новый абсолютный BMP `image`.
Перед симуляцией прогревается depth buffer; time scale и particle rate равны 1.
Выход: GPU adapter, изображения/хеши, счётчики последнего завершённого кадра.
Это не чтение внутренних координат частиц и не счётчик столкновений: сравнивайте
контролируемые on/off кадры до и после контакта. ParticleCommandletTests.py
проверяет поверхность/её отсутствие, оба renderer и реальные цветные пиксели.

`render.mesh`: абсолютный `material` (.mat), `shape` (plane/cube/sphere), `size`,
`mirrorUv`, `camera`/`target`/`up`, `orthographic`/`orthoHeight`, `rendering`, размер
viewport и `frames`. Кадр содержит `scale` (включая отражение), `rotation`,
`position`, новый абсолютный BMP `image`. Возвращаются центральный RGB-пиксель,
хеш и реальный tangent basis меша. Для plane по умолчанию камера сверху,
up=[0,0,1]; для косых ракурсов задавайте согласованный up.
`saveRoundTrip:true` вызывает настоящий SaveGame Capture/Restore; необязательный
`saveFile` сохраняет байты в новый файл. После восстановления transient Material
явно назначается снова (`transientMaterialRebound:true`): это проверка геометрии
и тангентов, а не доказательство сохранения ссылок материала.

`asset.import` дополнительно принимает `kind:"model"|"texture"|"audio"`.
Для texture/audio выполняется настоящий importAsset с соответствующим Kind,
возвращаются путь /Game, размер и SHA256 скопированных байтов; `placed:false`.
Поведение model по умолчанию сохранено.

`mesh.simplify` выполняет публичные SimplifyMesh/ValidateMesh для процедурной
сферы: `radius`, `slices` (3..128), `stacks` (2..128), `offset`, `ratio` (0..1),
`error` (0..1), `allowSloppy`. Возвращает source/reduced validation с проблемами,
предупреждениями, количеством вершин/треугольников, ошибкой и silhouetteDrift.
Это CPU-геометрия; отрисовка LOD проверяется отдельными GPU-командами.

### Тангенты меша

`mesh.tangents` вызывает `GenerateTangents` без GPU. Вход: `vertices` — массив
объектов `{position:[x,y,z], normal:[x,y,z], uv:[u,v]}`, `indices` — тройки
индексов треугольников. Выход `tangents` содержит `[x,y,z,w]` для каждой вершины;
`w=0` означает вырожденные UV и запасной путь шейдера. Максимум 100000 вершин и
300000 индексов, нечисловые значения и индексы вне массива отклоняются с exit 2.
Проверки: `Editor/tests/TangentCommandletTests.ps1`. GPU-передачу атрибута и
зеркальный масштаб проверяет `MaterialMirroredNormalMap` в EngineTests;
успешный CPU-командлет сам по себе не доказывает работу normal map на GPU.

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
`{"kind":"cursor","x":95,"y":131,"valid":true}`: начальный viewport 640×480.
`{"kind":"resize","width":1280,"height":720}` меняет скрытый HWND и передаёт
штатный ViewportResize через EditorQtHost (width64..1920, height64..1080).
Cursor использует новый размер. Выполните хотя бы один игровой кадр после resize;
`steps[].camera` содержит фактический размер/проекцию, `viewportRequested` — запрос.
Размер настоящего backbuffer дополнительно проверяется по сохранённому D3D-кадру.
`frames.count`: 0..600, суммарно не более 2000; шагов не более 100.
Относительное движение мыши: `{"kind":"pointer","delta":[60,0,0]}`
(x/y и колесо), через штатный viewport input API.
Результат `steps[].objects` содержит реальные имена, типы и позиции объектов
после каждого шага. При завершении вызывается Stop. Игровые обработчики могут
записывать свои SaveGame — используйте отдельный проект и каталог сохранений.

`{"kind":"diagnostics","request":{"layers":255,"controller":1,"agentRadius":true}}`
вызывает необязательный `Engine.Scene.AiDiagnostics` v1 на том же потоке сцены.
Ответ находится в `steps[].diagnostics`: source, units, radius, field, path,
cell и число линий/треугольников. Это данные настоящей диагностики, не снимок UI.
Сначала запросите units без controller; идентификаторы действуют только в текущей
сессии. `layers:0` очищает overlay. В Edit используется preview, в Play/Pause —
живой мир. Поля запроса и маски описаны в `tests/ai/diagnostics/README.md`.

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
Для самого D3D preview задайте `previewOnly:true`: commandlet минимизирует основной
viewport через существующий edit API, а отдельный material preview продолжает
рисоваться. Тогда `PAINENGINE_D3D_SCREENSHOT` и `PAINENGINE_D3D_SCREENSHOT_FRAME`
могут снять только preview; число кадров должно быть достаточным для выбранного
порога. Проверяйте BMP по возвращённому `nativePreviewSize` и видимому материалу.
Для сравнения сохранённых A/B материалов используйте отдельные процессы: шаги
редактирования и кадр capture имеют независимые моменты времени.

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

`Editor/tests/AuthoringCommandletTests.py --tool <exe> --launcher <launcher>
--output <new-directory>` проверяет реальный мастер классов и создание файлов,
пресеты/Cancel импорта, стартовую сцену и recovery между процессами. Создаёт
отдельный проект и включает C++; требует MSVC/PowerShell, Qt, GPU и Assimp.

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
- `startup`: требует верхнеуровневый `editorWindow:true` и ранее выполненный
  `save`/`load`; нажимает настоящее действие Set Startup Scene и возвращает
  manifest. Изменяет `startupScene` в scratch Project.json.
- `write-recovery`/`recover`: требуют верхнеуровневый `recovery:true`.
  Первый сохраняет несохранённую сцену штатным host; второй загружает последний
  оставленный checkpoint. Проверяются в разных процессах: живой lock не предлагается
  как кандидат. Командлет завершается без RequestExit, поэтому checkpoint остаётся.
  Выход содержит `recoveryRecords` и доступные `recoveryCandidates`.

Каждый объект результата содержит также `material` (общий override) и `projectData`,
включая `materialSlots`. Это позволяет сравнить назначения до/после изменения и Undo/Redo.

Результат `steps[]` содержит `status`, `error`, реальные `objects` с ID/parent/
локальной позицией, `selected`, `canUndo`, `canRedo`. Неуспех отдельной операции
сохраняется в её `status`, проверяйте его даже при exit 0 сценария.
Изменения существуют в открытой сессии до явно запрошенного `save`.
С 2026.11.07.0 хост командлета передаёт расширение PointLightV2: одиночное
дублирование лампы сохраняет `castShadows`, включая последующие Save/New/Load.
Регрессия: `Editor/tests/PointLightCloneCommandletTests.ps1`.
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
| `ui.class-wizard` | абсолютный `projectRoot`, `steps` с `name`, `kind` (Component/Character/Game/Object), необязательные `base`/`header`; PNG `image` | Настоящий CodeClassDialog, состояние кнопки, ошибка и аргументы генератора после каждого ввода. Файлы не генерирует |
| `project.class-wizard` | `project`, `name`, `kind`, необязательные `base`/`header` | Настоящее действие Add C++ Class, модальный диалог и производственный ProjectTools AddClass. Требует включённый C++; возвращает wizardAccepted, wizardError, arguments, generated (.h/.cpp), Console и dialogs. **Создаёт исходники**; сборка отдельным project.build |
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

`ai.editor` принимает `project` (изолированный `Project.json`), необязательный
`machine` (`/Game/...fsm`) и `steps` (не более 120 операций). Это настоящее Qt-окно
AI и runtime extension из Game.dll. `before` и каждый `steps[].snapshot` содержат
граф, ошибки по ID нод, файл/SHA256, объекты сцены, состояние Play и AI
states/memory/history. Код успеха самого runner не заменяет проверку
`controlStatus`, `status`, `accepted` и результата сценария.

- Файлы: `create-file` (`directory`, `name`), `open`/`reopen` (`machine`), `close`,
  `save`, `check`. `lockedFile` удерживает существующий файл от записи на время
  шага для проверки ошибки и последующего retry.
- Граф: `add` (`type`, `x`, `y`, `properties`), `state` (`node`, `properties`),
  `connect`/`link` (`from`, `to`, `properties`), `remove` (`nodes`), `variables`
  (`values`, `renames`), `settings` (`value`), `undo`, `redo`, `arrange`.
- Настоящие диалоги по двойному щелчку: `dialog-state` (`node`) и `dialog-link`
  (`from`, `to`). `edits` — упорядоченный массив `{name,value}` по objectName
  поля; `{blockList,add}` добавляет блок, `{blockList,row}` выбирает его.
  `cancel:true` нажимает Cancel, иначе OK. `dialogImage` сохраняет новый PNG.
- Сцена/карточка: `select`, `property`, `clone` с `target` (уникальное имя),
  `save-scene`/`load-scene` с `path`, `scene-undo`, `scene-redo`, `button` (`name`),
  `field` (`scope:"main"`, `name`, `value`), `scroll` (`name` поля инспектора).
- Runtime: `play`, `pause`, `resume`, `stop`, `frames` (`count`: 1..2000,
  `dt`: 0..0.1, строго больше нуля), `key` (`key`: Engine Keys, `down`). Между
  нажатием и отпусканием нужен игровой кадр. Focus устанавливается при Play.
  `wait` (`milliseconds`: 1..5000) прокачивает реальные Qt-таймеры без шага игры,
  например для обновления карточки с частотой раз в секунду.
- `image` в шаге сохраняет новый абсолютный PNG окна графа или `scope:"main"`.
  Кадр Qt не содержит D3D viewport и не доказывает визуальное поведение игры.

`editor.scene` использует те же steps с настоящим native Windows viewport.
Перед picking выполните `frames` с `count:1`, чтобы появилась projection matrix.
`hierarchy-click` (`target`, optional `control`) посылает настоящий клик по дереву,
`viewport-click` (`x`,`y` в диапазоне 0..1, optional `control`) — по viewport.
`ui-command` вызывает зарегистрированную команду, например `Gizmo.FocusSelected`.
Поле Inspector задаётся через `field`/`scope:"main"`: `Transform0..2` — position,
`Transform3..5` — Euler degrees, `Transform6..8` — scale. Отправляется editingFinished.
Snapshot содержит selectedIds и матрицы камеры; objects — локальный quaternion.
`coordinateSpaceLocal` читает настоящий режим viewport; переключение доступно через
`ui-command`/`id:"Gizmo.ToggleCoordinateSpace"`. `resize-window` меняет размер главного
окна (`width`:640..1920, `height`:480..1080); фактический размер viewport и projection
проверяются по snapshot после следующего кадра. `viewport-drag` посылает press/move/release
в native viewport: нормализованные `fromX/fromY/toX/toY`, `moves`:1..32 (default8),
optional `control`. `button` выбирает `left` (по умолчанию), `right` (camera look)
или `middle`. Попадание в конкретную ось gizmo определяется текущей камерой и выбранным
объектом; сам по себе exit0 не означает, что перетаскивание изменило объект.
В native режиме после UI-операций в Edit отрисовывается кадр с dt=0.
Рамочное выделение сцены (marquee) в текущем viewport отсутствует; его нельзя
считать проверенной доступной возможностью. Ctrl-click поддерживается отдельно.

  UI/HUD сценарии используют настоящие окна и действия редактора:
  `ui-create` / `ui-open` (`path`: абсолютный `.ui` внутри Content), `ui-close`;
  `ui-add` (`type`: Panel/Label/Button/Bar) вызывает toolbar action;
  `ui-select` (`target`: имя элемента) выбирает строку UIHierarchy;
  `ui-field` (`name`, `value`) редактирует Inspector, либо `index` для combo;
  `ui-action` (`name`: UISave/UIDelete) вызывает соответствующее действие.
  Поля: UIName, UIText, UIRect0..3 (X/Y/Width/Height), UIAnchor (0..8),
  UIVisible; UIPreviewSize имеет индексы0..3. `scope:"ui"` сохраняет Qt image
  UI-редактора. Snapshot содержит uiMarkup, uiSavedMarkup, uiSelected, uiPath.
  `hud-settings` открывает настоящий Project HUD dialog: необязательный `choice`
  — Content-relative `.ui` или пустая строка для None; `cancel:true` нажимает
  Cancel вместо OK, `dialogImage` сохраняет снимок до нажатия. Результат содержит
  beforeChoice/selectedChoice/cancelled; snapshot — hudDescriptorExists/hudChoice.
  Для проверки сохранения и отмены сравнивайте фактический descriptor и открывайте
  диалог снова; для игрового эффекта требуется отдельный Play/Runtime и D3D-кадр.

Новые SDK можно собирать локально с установленным MSVC через
`Engine/build.ps1 -Configuration Release -Target All -EnableAssimp $true -LocalSdk`.
Такой SDK помечается `localOnly:true`; обычная сборка по-прежнему требует v142
и проверяет совместимость распространяемого SDK. Локальный режим не является
проверкой совместимости с отсутствующими на машине компиляторами.

`Engine/tests/ai/run.ps1 -Configuration Release` компилирует сохранённые в
репозитории примеры и запускает все AI-тесты. `-RefreshGuide` отдельно запускает
генерацию из исходного `AI-CPP-GUIDE.md`, который не включён в текущий upstream.
Без этого файла регенерация строго завершается ошибкой; обычный запуск не
объявляет соответствие отсутствующему руководству проверенным.

`Editor/tests/MeshDiagnosticsTests.py --tool <EngineCommandlets.exe> --output <new-directory>`
проверяет Forward/Deferred normal map при отражении/неравномерном масштабе,
SaveGame, реальные texture/audio imports и упрощение сетки с отрицательным контролем.
`ParticleCommandletTests.py` с теми же параметрами сравнивает столкновения частиц
с полом и без пола в обоих рендерах. `AuthoringCommandletTests.py` дополнительно
принимает `--launcher <EngineLauncher.exe>` и проверяет настоящие Qt-формы,
генерацию/отказ повторного класса, пресеты/Cancel, startup и recovery между процессами.
`MaterialGraphCommandletTests.ps1` также проверяет предел 121 уникальной текстуры,
отказ 122-й без изменения графа и истории, уникальность регистров и резерв t32.
`CommandletIntegerValidationTests.py --tool <exe> --output <new-directory>`
проверяет отказ дробных/огромных width/count/steps/slices/rendering до приведения
к int; остальные диагностические наборы проверяют допустимые целые значения.

Добавить в dispatcher параметризованный вызов настоящей операции, описать вход/побочные эффекты/зависимости здесь и добавить сценарии в tests. ID жалобы принадлежит сценарию, а не API-команде. Не переносить игровую логику в Engine и не подменять неподдерживаемую возможность тестовой заглушкой. Сохранять before/after, код завершения и проверяемый эффект, а для отображения — PNG и осмотр.

Диагностика компиляции использует [MSBuild UTF-8 file logger](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference?view=visualstudio#switches-for-loggers), затем UTF-8 PowerShell stdout и QString::fromUtf8 с буферизацией неполных строк. Так не приходится угадывать OEM/ANSI кодировку на границе Qt.

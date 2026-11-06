# Повторный прогон .1 — журнал дефектов

Итог: BUG-R2-001…007 исправлены, регрессии и независимые повторы прошли.
Ошибки игрового кода и окружения хранятся
также в `calm-city/issues.md`, `curious-city/issues.md`, `tired-bomber/issues.md`,
`ai-trash/issues.md`; они не считаются автоматически дефектами Engine.

## BUG-R2-001 — screen-size LOD зависит от расстояния ортографической камеры

- Категория: Engine/render, подтверждено root и разработчиком бомбардировки.
- До: один куб, viewport 640×480, orthoHeight=24, пороги 50/20 px;
  Y=8 → LOD0, Y=24 → LOD1, хотя размер проекции не меняется.
- Ожидание: LOD1 в обеих позициях; изменение orthoHeight до 8 даёт LOD0.
- Воспроизводитель: `EngineCommandlets render.diagnostics`,
  `fixes/ortho-before-request.json`, `fixes/ortho-before-result.json`.
- До правки регрессия падает: `fixes/ortho-regression-before.log`.
- Причина: формула perspective FOV/distance применялась к orthographic.
- Исправление: коэффициент масштаба из текущей projection matrix; деление на
  расстояние применяется только к perspective. Bounding-sphere approximation
  и существующие расстояния/отсечение сохранены.
- После: 9 консольных сценариев PASS; 82 проверки embedded/standalone PASS.
  Повтор в отдельной игровой сборке SDK .2 — PASS: Y8/Y24 дают LOD1,
  zoom orthoHeight8 даёт LOD0. Уникальные BMP и лог находятся в
  `tired-bomber/evidence/fixed-screenheight-*` и `fixed-lod-final.stdout.log`.

## Ошибки оболочки проверок (исправлены, не дефекты Engine)

- Первый вариант теста Custom неверно ожидал, что базовый `Game::Draw` ничего
  не рисует. Базовая реализация рисует сцену; пустой пользовательский Draw
  отдельно проверяется RenderDiagnosticsTests. Ожидание оболочки исправлено.
- Автоподтверждение Qt через `accept`/`done` не выбирало стандартную кнопку Yes.
  Команда теперь нажимает саму кнопку; проверка отката сравнивает graph/HLSL.
- Первый screenshot нового material commandlet был без полной EditorTheme.
  Оболочка теперь применяет тему и вызывает реальные preview frames;
  повторное изображение осмотрено. QWidget capture не подтверждает содержимое
  нативной D3D preview-поверхности (в PNG она чёрная).
- Offscreen backend не загрузил символы Undo/Redo из системного font fallback;
  commandlet загружает также Segoe UI Symbol. Финальный `graph-final.png`
  осмотрен: стрелки видны. Это отличие окружения оболочки от обычного редактора.
- Qt offscreen не предоставляет действительный native HWND для material preview.
  Команда графа теперь использует скрытое Qt windows окно и проверяет HWND;
  остальные UI commandlets остаются offscreen. Повтор создал оба настоящих D3D
  устройства (основной viewport и превью) и прошёл проверки Save/Revert.
  Полный EditorUiTests также должен использовать windows backend; неудачный
  запуск на offscreen — ошибка настройки проверки, не дефект редактора.
- В первой сборке project.scene неверно названо поле `parent` вместо `parentId`;
  C2039 сохранена в `fixes/scene-commandlet-build-error.log`, исправлена,
  последующая сборка и 16 операций на двух игровых проектах прошли.

## BUG-R2-002 — отключённый CharacterController после Load реагирует на look

- Категория: Engine/SaveGame/input; подтверждено разработчиком бомбардировки.
- До: Save с `enabled:false`, другой процесс Load, RMB + pointer delta;
  Character стоит на месте, но yaw меняется с -1.5708 до -3.6708 и камера движется.
- Причина: сериализованное `enabled_` не синхронизировалось с `inputScope_`
  вновь созданного контроллера; look callbacks оставались активны.
- Исправление: при Read восстанавливается локальный input scope и очищаются
  накопленные оси; общий input не сбрасывается во время подготовки кандидата.
- Регрессия в SaveGameTests проверяет disabled и enabled, движение и yaw.
  До — FAIL (`fixes/controller-before.log`), после — 78 checks + отдельный
  restart PASS (`fixes/controller-after.log`). Повтор в отдельной игровой .2
  сборке — PASS: disabled не меняет yaw/позиции; после явного SetEnabled(true)
  тот же ввод снова перемещает игрока и вращает камеру.
- Первая сборка нового теста не имела include CharacterController.h:
  ошибка тестового кода C2027 сохранена в `controller-test-build-error.log`,
  исправлена до воспроизведения и не считается ошибкой движка.

## BUG-R2-003 — новый project.input отклоняет кнопки мыши Engine

- Категория: новая console tooling; пойман CuriousCity в этом круге.
- До: `key:500` отклонён ограничением 0..255. Документация ошибочно называла
  Windows LeftButton=1; в Engine Keys LeftButton=500, RightButton=501.
- Измерение в настоящей игре: курсор валиден, Game context включён,
  физическая LeftButton всё время false, Build UI не вызывается.
- Исправление: диапазон совпадает с viewport API 0..507, документация исправлена;
  добавлен `pointer` для относительного движения мыши. Повтор клика PASS:
  CuriousCity строит дом и списывает 100, HUD показывает 400/1 house.
  Регрессия `HudInputCommandletTests.ps1` подтверждает создание ровно одного
  компонента указанного игрового типа, лог `fixes/hud-input-after.log`.

## Окружение — checkpoint hot reload запрещён sandbox

AITrash ReloadInput сначала не смог создать сценовый checkpoint в системном
AppContainer Temp, Windows error 5. Оболочка скрывала первичную причину общим
`candidate did not replace session`; теперь она возвращает сообщение команды.
TEMP/TMP на scratch разрешили запись; самостоятельный повтор агента прошёл
все шесть проверок T/HUD до и после замены DLL. Runner теперь задаёт такой
TEMP/TMP автоматически, сохраняя прежнее окружение. Engine reload не менялся.

## BUG-R2-004 — InstancedMesh без материала рисует копии в одной позиции

- Категория: Engine/render; выявлен при дополнительном сравнении кадров бомбардировщиком.
- До: 3 видимых экземпляра с X=-3/0/3, CPU submission=3, но GPU показывает один
  куб в центре. Диагностическая панель сначала закрывала область опыта; после
  `panel:false` наложение подтвердилось.
- Причина: пустые shader names выбирали общий RotatedFigure, который не читает
  instance buffer и SV_InstanceID. Значения Position/Scale/Color/fade игнорировались.
- Исправление .3: fallback для InstancedMesh выбирает InstancedMesh.hlsl.
  Явные материалы/шейдеры сохраняют приоритет; старые SaveGame с пустыми именами
  также получают правильный fallback без смены формата.
- Проверка: `RenderDiagnosticsCommandletTests` читает пиксели трёх ожидаемых
  силуэтов и двух промежутков. До FAIL (`instance-placement-before.log/.bmp`),
  после PASS (`instance-placement-after.log/.bmp`), изображение осмотрено.
- Независимый игровой ретест .3 — PASS: три раздельных RGB экземпляра,
  `tired-bomber/evidence/api-0.3-instance-fallback-3colors.bmp`; кадр осмотрен
  также координатором. Промежуточный .2 frozen сохранён.

## BUG-R2-005 — F7/F8 недоступны через общий/embedded ввод

- Категория: Engine/diagnostics input. Подтверждён тестом в embedded viewport.
- Причина: ProcessHotkeys выходил при отсутствии InputDevicePtr, а новые F7/F8
  читали только legacy Raw Input, хотя editor viewport передаёт Input::System.
- Исправление .3: новые горячие клавиши читают общий physical key state перед
  legacy guard; удержание не повторяет переключение, F8 циклически выбирает mesh.
- До: `diagnostics-hotkeys-before.log` FAIL. После:
  `diagnostics-hotkeys-after.log`, 110 проверок embedded/standalone PASS,
  включая F7 down/hold/release и цикл F8 по двум компонентам.
- Ручные клавиши desktop не имитируются; проверяется настоящий общий input путь.
- Независимый игровой ретест .3 — PASS в standalone и embedded Play:
  `tired-bomber/evidence/api-0.3-hotkeys-runtime.stdout.log` и
  `api-0.3-project-input-f7f8.result.json`; F7 переключает панель,
  F8 выбирает второй компонент.

## BUG-R2-006 — обычные primitive CreateInstance искажают GPU-изображение

- Категория: Engine/render; выявлен дополнительным A/B опытом CalmCity.
- До (.3): bare CubeComponent с тремя CreateInstance на X=-3/0/3 и явно
  одинаковой ортографической камерой даёт искажённую/перекрывающуюся поверхность.
  Контроль MeshComponent с теми же копиями рисует три раздельные фигуры.
- Вариант CubeComponent с материалом также не переносил копии в visual mesh.
  Регрессия проверяет добавление экземпляров уже после первого кадра владельца.
- Причина: legacy primitive instanced constant buffer не совпадал с обычным
  fallback shader; существующая конверсия primitive в material mesh не
  включалась для default copies и не синхронизировала их для legacy геометрии.
- Исправление .4: используется существующий mesh/material proxy, копии
  синхронизируются с ним. Явные legacy shader pairs сохраняют прежний путь.
  Новый публичный API и новый формат сохранения не потребовались.
- До: `fixes/component-copies-before.log`, Cube bare/material FAIL, Mesh PASS.
  После: `component-copies-after.log`, все три варианта PASS, всего 124
  RenderDiagnostics checks. До/после BMP сохранены и осмотрены координатором.
  Независимый .4 игровой повтор — PASS: CalmCity fresh CLI C++ project,
  отдельные 30-frame процессы, bare Cube и Mesh control показывают три
  раздельных силуэта; кадры `calm-city/evidence/r4/instance-*-topdown-r4.bmp`.
- Первая оболочка чтения BMP предполагала 32-bit top-down изображение, а
  RequestFrameDump пишет 24-bit bottom-up. Ошибка теста сохранена в
  `component-copies-harness-format-error.log`; чтение исправлено до проверки
  дефекта и не учитывается как баг движка.

## BUG-R2-007 — заданная до подключения Game текстура Mesh не загружается

- Категория: Engine/assets/render; выявлен CuriousCity при повторном открытии
  сцены после переимпорта текстуры. WestHome хранит `/Game/Images/Renamed.bmp`,
  но HasTexture=false/SRV=null через 20 кадров. Красный и синий файл дают
  одинаковую картинку. Повторный setter после подключения Game сразу работает.
- Причина: CreateBuffers у MeshComponent, InstancedMeshComponent и FBXComponent
  пропускал базовый LoadPendingTexture. Примитивы SceneProject используют Mesh,
  поэтому проблема проявляется при обычной загрузке сцены.
- Исправление .5: отложенная текстура загружается при создании ресурсов всех
  трёх типов. Повторный setter и обходной код в игре не требуются.
- До: `fixes/pending-texture-before.log`, loaded=false у всех трёх типов,
  GPU-пиксели Mesh/InstancedMesh не имеют цвета заданной текстуры.
- После: `pending-texture-after.log`, loaded=true у трёх типов, реальные
  зелёные пиксели Mesh/InstancedMesh. Asset workflow: 224 checks PASS.
  `pending-texture-before.bmp` и `pending-texture-after.bmp` сохранены;
  итоговое изображение осмотрено. FBX в новой узкой регрессии проверяет
  создание SRV; изображение импортированных моделей проверяет остальная suite.
- Независимый .5 игровой повтор CuriousCity — PASS: пересобрана отдельная
  копия без обходного TrySetTexture. HasTexture/SRV=true с первого PreUpdate;
  50-frame процессы red/blue завершились успешно. Различаются ровно 11 582
  пикселя в области дома x618..740/y202..297, остальной кадр идентичен.
  Измерение и кадры проверены также координатором; материалы в
  `curious-city/evidence/final-probes/texture-final5-*`.
- Ошибка первой оболочки: не задан белый базовый Color, поэтому стандартный
  синий цвет умножался на зелёную текстуру и проверка цвета была неверной.
  Исправлено до финального before/after; лог `pending-texture-harness-color-error.log`.

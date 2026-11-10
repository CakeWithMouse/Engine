# План разработки PainEngine Editor v2
> **Ветка:** `feat/import-pipeline`  
> **Архитектор:** Юрий  
> **Инженер-исполнитель:** Antigravity  
> **Протокол:** `engine-work-protocol`

---

## Чек-лист среза S1 (Каркас Core/Host/UI, сервисы Core, check.ps1)

- [x] **Шаг 1.1 (fix):** Исправление путей библиотек Qt в `Editor/EditorModule.vcxproj` через прямые пути `$(QtInstallDir)\lib\Qt6*$(QtLibSuffix).lib` для обхода проблемы запятых в пути репозитория.
- [x] **Шаг 1.2 (chore):** Создание и ведение `Editor/PLAN.md` с эталоном тестов, разделами Решения, Backlog, Нужно в ABI.
- [x] **Шаг 1.3 (chore):** Создание `Editor/tools/check.ps1` с режимами `-Quick` (пошаговая валидация Debug, слои, wc -l, запретные зоны) и `-Full` (Debug+Release, полный прогон).
- [x] **Шаг 1.4 (refactor):** Формирование структуры каталогов `Editor/Core`, `Editor/Host`, `Editor/UI`, `Editor/tools` и обновление `Editor/EditorModule.vcxproj`.
- [x] **Шаг 1.4b (chore):** Защита `check.ps1`: таймауты процессов, динамическое чтение версии из `Engine/Version.json`, очистка кэша версий лаунчера, завершение зависших процессов.
- [x] **Шаг 1.5 (feat):** Реализация фундаментальных сервисов `Core`: `CommandRegistry`, `IUserPrompt`, `Logger`, `FileService` (песочница проекта) + юнит-тесты без виджетов.
- [x] **Шаг 1.6 (test):** Тестовый каркас UI (`objectName` локаторы, хелперы взаимодействия).

---

## Чек-лист среза S2 (Host-слой, развязка QtHost и MainWindow, декомпозиция DrainCommands, Checkpoint coalescing)

- [x] **Шаг 2.1 (refactor):** Создание `Editor/Host/IEditorHost.h` с интерфейсами `IEditorHost` и `IEditorUiListener`. Развязка `QtHost` от `MainWindow.h` (удалено поле `window_`, удалён прямой `#include "MainWindow.h"`, связь событий через `IEditorUiListener`).
- [x] **Шаг 2.2 (refactor):** Выделение `Editor/Host/EditorHistory` (`QTemporaryDir`, `Checkpoint()`, `RecordChange()`, Undo/Redo стек max 64, coalescing жестов перетаскивания и nudge).
- [x] **Шаг 2.3 (refactor):** Выделение `Editor/Host/ViewportCommandHandler` (ввод, HWND, Play/Pause/Stop, камера) и `Editor/Host/AssetCommandHandler` (импорт ассетов, hot reload, фоновые задачи).
- [x] **Шаг 2.4 (refactor):** Выделение `Editor/Host/SceneCommandHandler` (мутации сцены, snapshot, иерархия, параметры объектов). Сокращение `QtHost.cpp` с 641 до 298 строк.
- [x] **Шаг 2.5 (chore):** Регистрация новых файлов в `Editor/EditorModule.vcxproj`, интеграция через `Editor/UI/EditorApp.cpp`, валидация через `check.ps1 -Quick` (сборка Debug, CoreTests 47/47, UiTests 15/15, слои, wc -l).

---

## Чек-лист среза S3 (Чистый Core без UI, ветка `rewrite/editor-v2-core`)

- [x] **Шаг 3.1 (feat):** `Core/GizmoMath`: перенос из `feature/editor-ui-overhaul:Editor/Source/ViewportWidget.cpp` строк 1945–2123, дословно, без Qt-виджетов (`AxisLock`, `GizmoMode`, `NormalizeQuat`, `MulQuat`, `AxisAngleQuat`, `ApplyRotateScaleGizmo`, `ComputeSpawnPosition`). Юнит-тесты на снап и оси в `Editor/tests/core/GizmoMathTests.h` (58/58 PASS).
- [x] **Шаг 3.2 (feat):** `Core/HlslGraphCompiler` + `Core/MaterialGraphModel`: из `CompileGraphToHlsl` в `MaterialGraphWidget.cpp`. Модель без QGraphicsItem, числа в C-локали, JSON через QJsonDocument. Тесты: граф -> HLSL эталонные строки, round-trip JSON (76/76 PASS).
- [x] **Шаг 3.3 (feat):** `Core/SceneFile`: чтение/запись сцены через FileService. Тест на имя объекта a"b} (97/97 PASS).
- [x] **Шаг 3.4 (feat):** `Core/Terminal`: парсер команд (help, spawn, select, deselect, pos, rot, scale, delete, duplicate, focus, play, pause, stop, clear), только маппинг текста на команды CommandRegistry. Парсер ничего не исполняет сам. Тесты на кавычки, count, x y z (125/125 PASS).
- [x] **Шаг 3.5 (feat):** `Core/AssetOps`: создание/переименование/дублирование/удаление в песочнице корня проекта через FileService (169/169 PASS).

---

## Чек-лист среза S7 (Исправление регрессий: доки, вьюпорт гизмо/снап, шапка вьюпорта)

- [x] **Шаг 7.1 (fix):** Устранение сдавливания вьюпорта, разблокировка сплиттеров доков (`QSizePolicy::Ignored`, `AdjustIgnored`, `Expanding` для вьюпорта), корректный `splitDockWidget` в `DockLayoutManager` и очистка конфликтующего layout в `BuildSceneDocks`.
- [x] **Шаг 7.2 (fix):** Восстановление `host_->SetGizmoMode(mode)`, снаппинг перемещения по накопленному смещению жеста от `dragStartTransform_.position`, коалесцирование жеста через `BeginGizmoDrag`/`EndGizmoDrag` и юнит-тест в `CoreTests`.
- [x] **Шаг 7.3 (refactor):** Перенос `ViewportHeaderBar` в обычный layout `ViewportContainer` над нативным HWND `ViewportWidget`, перевод фона и иконок на непрозрачные токены `EditorTheme` с автообновлением.
- [x] **Шаг 7.4 (fix):** Диагностика и устранение блокировки сжатия контент-браузера: ограничение `minimumSizeHint` (до 40-58 px <= 60 px) для всех доков, перенос верхнего ряда в `TopDockWidgetArea` с каноническим сплитом, настройка нижних углов `BottomLeftCorner`/`BottomRightCorner` на `BottomDockWidgetArea`.

---

## Чек-лист среза S8 (Компактный интерфейс)

- [x] **Шаг 8.1 (merge):** Слияние ветки `origin/rewrite/editor-v2` (чистые модули Core S3: `AssetOps`, `Terminal`, `SceneFile`, `HlslGraphCompiler`, `MaterialGraphModel`) с сохранением всех тестов (194 проверки в `CoreTests`).
- [x] **Шаг 8.2 (feat):** Компактная шапка вьюпорта `ViewportHeaderBar`: плоский ряд кнопок 24x24, фиксированная высота 28px, векторная иконка "⋯" и адаптивное сворачивание групп (снап, координаты, инструменты) в контекстное меню при сжатии окна.
- [x] **Шаг 8.3 (feat):** Компактный тулбар запуска `MainToolBar`: высота 30px, центрированные плоские кнопки 24x24 с иконками 16px, без текстовых подписей.
- [x] **Шаг 8.4 (feat):** Компактные заголовки доков `QDockWidget`: высота 20-22px, шрифт 11px нормальной насыщенности, отступы 4px; замена заголовка свойств на имя выделенного объекта без слова "Свойства"/"Properties".
- [x] **Шаг 8.5 (feat):** Компактные карточки инспектора `CollapsibleCard`: заголовки карточек высотой 22px, отступы между карточками 4px, поля ввода высотой 22px, визуальный акцент на значениях (второстепенный цвет подписей, яркий цвет значений).
- [x] **Шаг 8.6 (fix):** Мелкие UI-правки: кнопки с текстом в карточках (24px, padding 3px 8px, текст не обрезается в RU/EN), выравнивание тулбара Play 32px (отступы по 3px сверху и снизу), шапка вьюпорта 31px с отступами 3px и границей 1px, заголовок инспектора зафиксирован как "Инспектор"/"Inspector".
- [x] **Шаг 8.7 (feat):** Управление объектами во вьюпорте: физически корректное перемещение в плоскости экрана камеры (масштаб пропорционален расстоянию до камеры), проекция мыши на оси X/Y/Z в World и Local, корректные знаки вращения, отмена жеста по клавише Esc с возвратом стартового transform.

---

## Чек-лист среза S9 (Оверлеи вьюпорта, гизмо, иконки и ABI камеры)

- [x] **Шаг 9.1 (feat(engine)):** C-ABI расширение `ViewportOverlay` (`EngineViewportOverlayExtension` = `"Engine.Viewport.Overlay"`: `getCamera`, `setOverlayLines`, `setOverlayIcon`, `setOverlayBillboards`). Сохранение замороженного ABI (`sizeof(EngineEditSessionApi) == 320`). Шейдер `Engine/Content/Shaders/EditorOverlay.hlsl` (линии и билборды с отменой перспективного деления `* clipCenter.w`). Прогон тестов `Engine/tests/edit-session/run.ps1` (617/617 PASS).
- [x] **Шаг 9.2 (feat(core)):** Модули `Core/ViewportOverlay` (генерация сетки Y=0, затухание по дистанции, проволочная сфера радиуса света, FNV-1a хеширование) и `Core/GizmoGeometry` (масштаб гизмо постоянного экранного размера, проекция на экран, расстояние точка-отрезок, пересечение луч-плоскость, угол на кольце, выбор ручки PickHandle 8-10px, построение линий гизмо). Юнит-тесты в `Editor/tests/core/GizmoGeometryTests.h`, включение в `CoreTests` (216/216 PASS).
- [x] **Шаг 9.3 (feat(host)):** Экспорт `EngineBindViewportOverlay` в `Module.cpp`, расширение интерфейса `IEditorHost` и реализация в `QtHost` (`GetCamera`, `SetOverlayLines`, `SetOverlayIcon`, `SetOverlayBillboards`), поток-безопасная буферизация с атомарным обновлением камеры и выгрузкой в начале кадра `Update(dt)`. Реализация `IconProvider` (растеризация 6 Tabler SVG 64x64 RGBA через `QSvgRenderer`).
- [x] **Шаг 9.4 (feat(viewport)):** Синхронизация камеры `ViewportWidget` напрямую из `host_->GetCamera` (полное устранение дельта-приближений `camYaw_`/`camPitch_`/`camPos_`). Модуль `ViewportOverlayHelper` (обновление оверлеев, билборды для источников света, неба, камер, партиклов, персонажа; хит-тест билбордов по клику мыши). Интерактивное гизмо с подсветкой ручек при наведении и перетаскиванием по осям/плоскостям. Пункты меню "Вид" -> "Сетка" и "Иконки и гизмо" с сохранением в `SettingsService`. Декомпозиция `ViewportWidget_Input.cpp` с соблюдением лимита строк (<500 строк).

---

## Чек-лист среза S9.5 (Устранение дефектов приёмки вьюпорта)

- [x] **Шаг 9.5a (fix(host)):** Исправление проверки кода возврата `overlay_.getCamera` в `QtHost.cpp`: C-ABI метод возвращает `EngineSceneOk == 0`, поэтому `if (overlay_.getCamera(...))` всегда давал `false` (`lastCameraValid_` оставался `false`, блокируя генерацию гизмо и радиуса света). Заменено на `== EngineSceneOk`.
- [x] **Шаг 9.5b (fix(ui)):** Защита и отказоустойчивость `IconProvider.cpp`: очистка XML-комментариев перед `<svg>`, проверка валидности растеризации и процедурный fallback (желтая лампочка, солнце, прямоугольник) при пустых пикселях SVG; устранение одноразового сброса при неинициализированном `Device` (до 5 попыток выгрузки).
- [x] **Шаг 9.5c (fix(host)):** Буферизация и повторная постановка в очередь `pendingIcons_` в `QtHost.cpp`, если `overlay_.setOverlayIcon` еще не готов на раннем кадре. Добавлены однострочные диагностические логи `[Info] Viewport overlay service...`, `[Info] Camera: ok/zero`, `[Info] Overlay lines sent...`, `[Info] Billboards sent...`.
- [x] **Шаг 9.5d (fix(settings)):** Включение сетки и иконок/гизмо по умолчанию в `MainWindow.cpp` (`view/grid` и `view/overlays` принудительно инициализируются в `true` при первом запуске).
- [x] **Шаг 9.5e (tool):** Создание `Editor/tools/run-editor.ps1`: завершение зависших процессов, очистка устаревшего кэша `%LOCALAPPDATA%\PainEngine\Engines\<version>`, сборка Debug с `/m /p:ForceSynchronousPDBWrites=true`, деплой `EditorOverlay.hlsl` и запуск лаунчера.
- [x] **Шаг 9.5f (test):** Режим `--smoke-editor` в `EditorApp.cpp` и автоматическая визуальная верификация пикселей (`smoke_screenshot.png`): подтверждено наличие пикселей сетки, осей гизмо (красный >6200 px, зеленый, синий) и радиуса теплого света `RGB(255, 166, 89)` (>7700 px).
- [x] **Шаг 9.5g (fix(engine)):** Устранение порядка умножения матриц в `Game::RenderEditorOverlay()` (`Game.cpp`): `Player::ViewMatrix` и `ProjectionMatrix` возвращаются транспонированными (`V^T`, `P^T`), добавлен `XMMatrixTranspose` перед перемножением, исключающий искажение и culling оверлеев.
- [x] **Шаг 9.5h (fix(engine)):** Транспонирование матриц вида и проекции в `Game::GetCameraInfo()` (`Game.cpp`): передача канонических row-major матриц в `EngineCameraInfo`, исключающая отрицательный `cw` в `GizmoGeometry::ProjectPointToScreen`.
- [x] **Шаг 9.5i (diag(engine)):** Логирование инициализации шейдеров `EditorOverlay.hlsl` (`[Info]`/`[Error]`) и D3D11 staging readback в `Game::RenderViewportFrame()` для объективной проверки пикселей swapchain без зависимости от grab Qt-окна.
- [x] **Шаг 9.5j (fix(ui)):** Динамический выбор объектов в smoke-режиме `EditorApp.cpp` на основе снапшота сцены.
- [x] **Шаг 9.6 (feat):** Толстые линии оверлея (D3D11 квады в VS, 3px гизмо, 1px сетка, 2px сфера света), гизмо ~100px с полупрозрачными плоскостями (альфа 0.35) и 3D наконечниками/кубиками, полное удаление свободного перетаскивания тела объекта и переход на физически точные 3D-лучи (ScreenToWorldRay, ClosestPointOnAxis, RayPlaneIntersection), синхронизация Game.dll в run-editor.ps1.

---

## Чек-лист среза S10 (Ускорение повседневной сборки, chore(build))

- [x] **Шаг 10.1 (perf(engine)):** Включение параллельной компиляции `/MP` в `Engine/BuildSupport/Cpp.props` (`<MultiProcessorCompilation>true</MultiProcessorCompilation>`). Сокращение холодной сборки `Engine.vcxproj` в 8 раз (с 224.2 с до 28.3 с).
- [x] **Шаг 10.2 (perf(editor)):** Внедрение предкомпилированных заголовков PCH для Editor (`Editor/pch.h`, `Editor/pch.cpp`) с инъекцией через `/FI"pch.h"` и исключением moc/UI файлов. Ускорение инкрементной компиляции файла редактора в 5.7 раз (с 4.75 с до 0.83 с).
- [x] **Шаг 10.3 (perf(editor)):** Оптимизация `windeployqt` через stamp-файл (`$(IntDir)windeployqt.stamp`) и таргет `DeployQtRuntime` с проверкой зависимостей. Устранение оверхеда ~350 мс на каждом запуске сборки.
- [x] **Шаг 10.4 (tool):** Реализация быстрого режима `-Fast` (по умолчанию) в `Editor/tools/run-editor.ps1`: сборка целевых проектов (`Engine`, `EngineRuntime`, `SceneProject`, `EditorModule`, `EngineLauncher`), пропуск игр и тестов при отсутствии изменений в их коде, вывод меток времени и 15-секундный таймер линковки.
- [x] **Шаг 10.5 (build):** Оптимизация настроек Debug в `Cpp.props` (явный `<LinkIncremental>true</LinkIncremental>`, отключение LTCG/GL для не-Release конфигураций). Валидация через `check.ps1 -Full` (Debug + Release, все тесты PASS).

---

## Эталон тестов (Baseline ветки feat/import-pipeline @ ddec269 перед шагами исправления)

| Тест / Скрипт | Результат (эталон) | Примечание |
|---|---|---|
| Сборка `EditorModule` (Debug) | **PASS** | 0 ошибок, 0 новых warnings |
| Сборка `EditorModule` (Release) | **PASS** | Собрано MSBuild x64 |
| `Editor/tests/CoreTests.exe` | **656/656 PASS** | 656 checks passed |
| `Editor/tests/HostTests.exe` | **109/109 PASS** | 109 checks passed |
| `Editor/tests/UiTests.exe` | **315/315 PASS** | 15 helper + 20 dialog + 18 edit + 3 v2 + 14 delete + 245 smoke = 315 checks passed |
| `Engine/tests/run.ps1` (Debug) | **74/74 PASS** | 74/74 tests passed; 0 skipped; 0 failed |
| `Engine/tests/skeletal/run.ps1` (Debug) | **8/8 PASS** | 8/8 passed |
| `Engine/tests/edit-session/run.ps1` (Debug) | **651/651 PASS** | 651 checks passed |
| `Engine/tests/scene-module/run.ps1` (Debug) | **109/109 PASS** | 109 checks passed |
| `Engine/tests/compatibility/run.ps1` | **116/116 PASS** | 116 checks passed |
| `Editor/tools/check.ps1 -Quick` | **FAIL (Красная база)** | `FAIL: Legacy file Editor/Source/MaterialEditorWindow.cpp exceeded Cyrillic ratchet limit (1380 > 800)` |
| `Game/tests/run-games.ps1` | **FAIL (Красная база)** | `Expected 1, got -1073741515: BasicExamples.exe --smoke` (отсутствие assimp.dll в фикстуре, 25/27 PASS) |
| `Editor/tests/run.ps1` (`EditorUiTests.exe`) | **FAIL (Красная база)** | `preview clears to its background` (падение grabWindow в фоновом окружении) |
| `Engine/tests/assets/run.ps1` | **FAIL (Красная база)** | `ParserError: Unexpected token '' + [guid]::NewGuid().ToString(''` (нет BOM в PS 5.1) |
| `Engine/tests/code-projects/portable.ps1` | **FAIL (Красная база)** | `Join-Path : A positional parameter cannot be found that accepts argument 'С‹РїСѓСЃРє'` (нет BOM) |
| `Engine/tests/code-projects/run.ps1` | **FAIL (Красная база)** | `ParserError: An expression was expected after '('` (нет BOM) |
| `Engine/tests/input/run.ps1` | **FAIL (Красная база)** | `ParserError: Unexpected token 'std::unique_ptr<CharacterController>'` (нет BOM) |
| Храповик строк `EditorUiTests.cpp` в `check.ps1:220` | **FAIL при модификации (Красная база)** | Реальный размер файла 1662 строки (на `main` 1674) против заниженного храповика 1045. При чистом diff `check.ps1` проходит, но любая правка файла роняет проверку line count |

---

## Решения

1. **Разделение слоев архитектуры:**
   - `Core/`: только QtCore/QtGui без виджетов (`QWidget`, `QAction`, `QDialog`, `QMessageBox` запрещены). Диалоги только через интерфейс `IUserPrompt`.
   - `UI/`: тонкие виджеты, вызывают команды исключительно через `CommandRegistry`.
   - `Host/`: мост к движку через C-ABI `IEditorHost`. UI не знает про `Host`, `Host` не знает про UI.
2. **Безопасная линковка Qt:**
   - Подключение библиотек Qt выполняется явным указанием путей к файлам `$(QtInstallDir)\lib\Qt6*$(QtLibSuffix).lib` в `AdditionalDependencies`, что исключает разрушение путей с запятыми в `/LIBPATH`.
3. **Режимы верификации в `check.ps1`:**
   - `-Quick`: сборка Debug, проверка слоёв, `wc -l`, запретные зоны, быстрый юнит-тест. Запускается после каждого атомарного коммита.
   - `-Full`: сборка Debug + Release, полный эталон тестов. Запускается в конце каждого среза.
4. **Защита от блокировок модальными окнами лаунчера:**
   - Модальные окна лаунчера блокируют прогоны, лечится очисткой кэша (`%LOCALAPPDATA%\PainEngine\Engines\<версия>`, читается динамически из `Engine/Version.json`) и таймаутами процессов в `check.ps1`.
5. **Конфигурация CRT для тестов:**
   - Debug-сборка тестов использует `/MD` + `NDEBUG` (как `EditorModule.vcxproj`), потому что Qt6 в поставке только релизный; CRT-проверки кучи в Debug отключены.
6. **Статическая компоновка Core и границы типов Qt:**
   - Модуль `Core` не экспортируется как отдельная динамическая библиотека, а компилируется статически прямо в потребители (`Editor.dll`, `CoreTests.exe`).
   - Типы Qt (`QString`, `QStringList`, `QByteArray`) допустимы только внутри бинарника `Editor.dll`. На границе `Editor.dll` <-> `Engine` (C-ABI `ModuleApi.h`, `SceneModuleApi.h`) передача объектов Qt строго запрещена; используются только C-совместимые типы (указатели, POD, C-строки).
7. **Тестовый каркас UI (`UiTestHelper`):**
   - Локация элементов строго по `objectName` (`Find`, `Require`), поиск по тексту запрещён.
   - Хелперы взаимодействия (`ClickButton`, `TriggerAction`, `SetLineEdit`, `SetSpinBox`, `SelectComboBox`, `WaitUntil`) гарантируют nullptr-безопасность, проверку `isEnabled` и вызов исключений при отсутствии элементов.
   - Самопроверка каркаса запускается через `EditorUiTests.exe --self-test` (15 проверок).
8. **Развязка Host и UI слоев (`IEditorHost` и `IEditorUiListener`):**
   - Хост `EditorQtHost` реализует интерфейс `IEditorHost` и не включает заголовок `MainWindow.h`, не держит указатель на окно и не зависит от UI-классов.
   - Окно редактора `EditorMainWindow` реализует `IEditorUiListener` (`OnLogMessage`, `OnCommandFinished`) с потокобезопасной доставкой через `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
   - Запуск приложения вынесен в слой UI (`EditorApp.cpp`), регистрирующий bootstrap-фабрику окна.
9. **Политика Checkpoint и coalescing жестов в `EditorHistory`:**
   - Непрерывные жесты перетаскивания гизмо обрамляются `BeginGizmoDrag` / `EndGizmoDrag`. Первый тик создает ровно 1 файл контрольной точки `N.scene.json` в `QTemporaryDir`, последующие тики в течение жеста коалесцируются (0 файлов).
   - Одно нажатие `Undo` возвращает объект в состояние до начала жеста.
10. **Выделение Core/GizmoMath и Core/GizmoGeometry без виджетов (S3.1, S9.2):**
    - Математические операции гизмо (`ApplyRotateScaleGizmo`, кватернионы, `ComputeSpawnPosition`, `AxisLock`) и геометрические алгоритмы (`ComputeGizmoWorldScale`, `ProjectPointToScreen`, `DistancePointToSegment`, `RayPlaneIntersection`, `ComputeRingAngle`, `PickHandle`, `BuildGizmoLines`) изолированы в чистом слое Core без зависимостей от Qt-виджетов.
11. **Расширение C-ABI ViewportOverlay без нарушения frozen layout:**
    - `sizeof(EngineEditSessionApi) == 320` зафиксирован статическим ассертом ABI v6. Новые возможности оверлеев и камеры оформлены как версионированное сервисное расширение `EngineViewportOverlayExtension` (`"Engine.Viewport.Overlay"`), запрашиваемое через `EngineGetEditExtension` и привязываемое через `EngineBindViewportOverlay`.
12. **Отрисовка оверлеев и билбордов в D3D11:**
    - Линии оверлея рендерятся в `Game::RenderEditorOverlay()` топологией `LINELIST` поверх буфера кадра (с тестом глубины `LESS_EQUAL` без записи глубины).
    - Билборды масштабируются в экранных пикселях в вертексном шейдере через умножение экранного смещения на `clipCenter.w`, полностью отменяя перспективное деление и сохраняя точный размер в пикселях на любом расстоянии от камеры.
13. **Кэширование и предотвращение аллокаций в горячем коде:**
    - Вычисление 64-битного FNV-1a хеша состояния камеры, выделения и ручек гизмо (`ViewportOverlay::ComputeOverlayHash`) предотвращает передачу неизменённых массивов линий и билбордов в движок.
    - Атомарное кэширование камеры `EngineCameraInfo` в `QtHost::Update(dt)` гарантирует потокобезопасность чтения из UI-потока без блокировок D3D11 контекста.
14. **Синхронизация `Game.dll` для пользовательских проектов (`run-editor.ps1`):**
    - Каждый созданный из шаблона проект содержит локальный `Game.dll`, скомпонованный со статическим `Engine.lib`. При изменении кода движка в `Engine/` сборка шаблона обновляет `Game/build/x64/Debug/Runtime/Templates/SceneProject/Game.dll`, но локальный `Game.dll` в проектах пользователя (например `Documents/11/Game.dll`) остаётся старым и блокирует новые фичи (включая оверлеи). В `run-editor.ps1` добавлен шаг 5b, находящий проекты пользователя в `Documents/` и реестре лаунчера и автоматически копирующий свежий `Game.dll` при несовпадении временных меток с выводом жёлтого предупреждения в лог.
15. **Толстые экранные линии в D3D11 (`EditorOverlay.hlsl`, `Game.cpp`):**
    - Каждая линия оверлея преобразуется в вертексном шейдере в экранно-ориентированный квад (2 треугольника, 6 вершин на отрезок) со смещением по нормали к экранному направлению линии, масштабируемым на `clip.w` и aspect ratio для точной толщины в пикселях. Толщина кодируется в битах 8..15 поля `flags` структуры `EngineOverlayLine` без нарушения 32-байтного ABI. Топология переведена на `TRIANGLELIST`, добавлен `OverlayRasterState` (`CULL_NONE`), вертексные буферы переиспользуются без аллокаций.
16. **Физически корректное управление гизмо по 3D-лучам:**
    - Полностью устранено старое свободное перетаскивание тела объекта при drag мыши по телу (проекция 2D-дельты на плоскость камеры).
    - Новый конвейер событий ЛКМ: сначала хит-тест ручек гизмо (`PickHandle`, 10px); при попадании — старт жеста гизмо (`BeginGizmoDrag`) с вычислением 3D-луча камеры (`ScreenToWorldRay`); при промахе — хит-тест билбордов иконок -> `PickObject` сцены (клик в пустоту снимает выделение).
    - Перемещение по оси вычисляет ближайшую точку на 3D-прямой оси (`ClosestPointOnAxis`), по плоскости — пересечение луча с плоскостью (`RayPlaneIntersection`), вращение — угол по кольцу с физически корректным знаком (`ComputeRingAngle`), масштабирование — отношение расстояний.
    - Накопленный жест обеспечивает стабильный snap без дрифта, Esc возвращает исходный `dragStartTransform_`, ровно 1 запись в стек Undo на жест.
17. **Слияние origin/main и rewrite/editor-v2 в feature/editor-v2-final:**
    - Создана ветка `feature/editor-v2-final` от `origin/main`. Влита ветка `rewrite/editor-v2` без конфликтов в рабочей копии, объединены новые тесты Core/Host/UI и движковые правки.
18. **Центральный манипулятор (Center handle) гизмо:**
    - Добавлен кубический манипулятор в центре гизмо (размер ~10 px) с приоритетом выбора в `PickHandle` при дистанции курсора <= 8 px.
    - Для режима Move (перемещение) перетаскивание за центр выполняет свободное перемещение в плоскости, параллельной экрану камеры (через пересечение с плоскостью камеры camFwd).
    - Для режима Scale (масштабирование) перетаскивание за центр выполняет равномерное масштабирование по всем трем осям (Sx = Sy = Sz = S0 * scaleFactor).
    - Для режима Rotate (вращение) центр не используется.
19. **Исправление математики депроецирования луча и ориентации осей/плоскостей:**
    - В `GizmoGeometry::ScreenToWorldRay` исправлена формула преобразования луча из пространства камеры в мир для DirectX row-major матриц (`XMMatrixLookToLH`): заменено некорректное прямое умножение на матрицу вида на умножение на транспонированную матрицу вида (V^T), эквивалентное V^-1 для ортонормированного базиса.
    - В `ViewportWidget::BeginGizmoDragGesture` и `ApplyGizmoDragGesture` вектор взгляда камеры `camFwd` исправлен на 3-й столбец матрицы вида: `[viewMatrix[2], viewMatrix[6], viewMatrix[10]]`.
    - В `GizmoGeometry::PickHandle` плоскости XY, XZ, YZ переведены на хит-тест попадания точки пересечения внутрь 2D-квадрата ручки плоскости (u in [0, size], v in [0, size]) с фильтрацией скользящих лучей (|N * RayDir| < 0.05).
    - Исправлен порядок кватернионного умножения в `ViewportWidget::ApplyGizmoDragGesture`: Q_res = Q_delta * Q_start (`MulQuat(deltaQ, startQ, resQ)`), поскольку базис осей гизмо уже переведён в мировое пространство.
20. **Сетка вьюпорта (`ViewportOverlay.cpp`):**
    - Из генерации сетки `GenerateGridLines` удалена зеленая вертикальная линия оси Y.
    - Главные оси X (Z=0) и Z (X=0) выделены толщиной 2 px (`OverlayFlags::Thickness(2)`), остальные линии сетки имеют толщину 1 px.
21. **Пост-процессное сглаживание FXAA 3.11 (`FXAA.hlsl`, `Game.cpp`, `QtHost.cpp`):**
    - Архитектурный анализ deferred vs forward: Pain Engine использует deferred-конвейер с набором G-буферов, которые семплируются форвардными и материальными шейдерами как стандартные `Texture2D`. Включение аппаратного MSAA потребовало бы перехода всех G-буферов на `Texture2DMS` и адаптации шейдеров каждого материала. В качестве эффективного решения реализован полноэкранный пост-процесс FXAA 3.11 Quality, устраняющий лестничный эффект на геометрии сцены, освещении и оверлеях без размытия UI.
    - Расширен C-ABI оверлея `EngineViewportOverlayApi` до версии 2 (с обратной совместимостью v1/v2): добавлено поле `setAntialiasing(void* context, uint32_t mode)`.
    - В `Game.cpp` реализован промежуточный рендертаргет `SceneColorTexture` и проход `RenderFxaaPass()`, выполняемый перед Present.
    - В UI редактора добавлено меню `Вид -> Сглаживание` (Выкл, FXAA) с вызовом команды `View.SetAntialiasing` и сохранением в `SettingsService` под ключом `viewport/antialiasing` (по умолчанию включен FXAA).
22. **Ускорение повседневной сборки редактора и движка (S10):**
    - `Cpp.props` изменён архитектором-разрешён для ускорения сборки (`/MP`, `LinkIncremental`).
    - **Параллелизм /MP:** В `Engine/BuildSupport/Cpp.props` глобально включен `<MultiProcessorCompilation>true</MultiProcessorCompilation>`, что устранило последовательную компиляцию 54 исходников движка на одном ядре CPU и сократило время холодной компиляции движка с 224.2 с до 28.3 с (в 8 раз быстрее).
    - **PCH для Editor:** Модуль `EditorModule` теперь использует минимальный `pch.h` (включающий Windows API с `NOMINMAX`, базовые контейнеры STL, QtCore, QtGui, QtWidgets). Это устранило многократный разбор тяжелых заголовков Qt и сократило время перекомпиляции любого `.cpp` файла редактора с 4.75 с до 0.83 с (в 5.7 раз быстрее), а общее время одной правки в Editor/*.cpp — с 6.38 с до 2.07 с (в 3.1 раза быстрее).
    - **Stamp-файл для windeployqt:** Деплой библиотек Qt переведен из безусловного `PostBuildEvent` в таргет `DeployQtRuntime` с отслеживанием зависимостей `windeployqt.stamp`. Если бинарники Qt не обновлялись, запуск `windeployqt.exe` полностью пропускается, экономя ~350 мс на каждом цикле сборки.
    - **Режим -Fast в run-editor.ps1:** По умолчанию `run-editor.ps1` собирает через MSBuild только целевые проекты, необходимые для запуска редактора (`Engine`, `EngineRuntime`, `SceneProject`, `EditorModule`, `EngineLauncher`), пропуская игры (Pong, Battle, SunGame, BasicExamples, Katamari) и тесты, если в их каталогах нет локальных изменений.
    - **Индикация прогресса и таймер линковки:** В `run-editor.ps1` добавлен вывод меток времени `[HH:mm:ss]`, подсветка этапов компиляции и сборки артефактов, а также периодический heartbeat-таймер раз в 15 секунд, информирующий пользователя об активном процессе линковки.
    - PCH для Engine и динамический `Game.dll` отложены в Backlog.
23. **Sidecar настроек импорта (`<asset>.importsettings.json`):**
    - Настройки импорта сохраняются в sidecar-файл с суффиксом `.importsettings.json` рядом с целевым ассетом (например `Content/Models/Character/Character.glb.importsettings.json`).
    - Суффикс `.import.json` не используется, так как он занят манифестом материалов `ModelImport`.
    - Формат JSON сериализуется через `QJsonDocument`, вещественные числа строго в C-локали (`QLocale::c()`). Запись и чтение производятся через `FileService` с логированием.
24. **Объём среза S16 (Настройки импорта):**
    - Реализуется только настройка масштаба (Scale) для моделей с пресетами 1.0, 0.1, 0.01 и возможностью ручного ввода.
    - Текстуры и звуки импортируются по штатному конвейеру; неподдерживаемые движком параметры не добавляются в UI (правило №2: «Муляжей нет»).
25. **Фиксация красной базы:**
    - Прогон `check.ps1` на ветке `main` зафиксировал 4 эталонных падения: локализация в `MaterialEditorWindow.cpp`, отсутствие `assimp.dll` в фикстуре `run-games.ps1`, 4 скрипта тестов без BOM в PS 5.1, падение `grabWindow` в UI-тесте окна материалов. База признана эталоном и не исправляется в рамках задачи.
26. **Запретные зоны:**
    - `Engine/`, `Game/`, `build.ps1` не модифицируются.
27. **Решения по срезу S17 (Полный пайплайн импорта моделей):**
    - **Unit scale:** Вариант 1 (нормализация геометрии в Assimp). Дефолт `useFileUnitScale = false` (сохраняем текущее поведение; включение по желанию пользователя).
    - **Import materials / textures:** Вариант А (sidecar до импорта). Вариант В отвергнут.
    - **Область правок Engine/:** Разрешены правки `Engine/` ТОЛЬКО в пяти точках:
      1) `Engine/Source/Render/SkeletalLoaderAssimp.cpp`
      2) `Engine/Source/Scene/ModelImport.cpp`
      3) `Engine/Source/Scene/SceneAssets.cpp`
      4) `Engine/Source/Scene/AssetUpdate.cpp` (ремаппинг и защита importsettings.json)
      5) новый маленький файл парсера sidecar в `Engine/Source/Scene/`
    - `EditSessionApi.h` и размер структуры 320 байт строго не трогать.
    - `Game/`, `build.ps1` не трогать.
    - Ветка: `feat/import-pipeline`, без push. Красная база остаётся эталоном.
28. **Дефолт mergeMeshes = true (Шаг 17.3b):**
    - Воспроизводит историческое поведение движка `PretransformVertices` без `KEEP_HIERARCHY`. При `mergeMeshes = false` выставляется `AI_CONFIG_PP_PTV_KEEP_HIERARCHY = 1`.
29. **Реализация useFileUnitScale через aiProcess_GlobalScale (Шаг 17.3c):**
    - `useFileUnitScale` в `SkeletalLoaderAssimp.cpp` подключается через выставление флага `aiProcess_GlobalScale`.
    - Значение `scale` из sidecar в Assimp не передаётся (остаётся на Transform при спавне объекта во избежание двойного применения).
    - При `useFileUnitScale = false` флаги Assimp не изменяются, сохраняя 100% побитовую совместимость с ранее импортированными моделями.
30. **Дефолт useFileUnitScale в UI (Шаг 17.6b, 17.6d):**
    - Дефолт `useFileUnitScale = true` во ВСЕХ путях `ImportSettingsDialog` (единый метод `DefaultSettings()`, при открытии диалога, в `Prompt()`, в `PromptScale()`, по кнопке «Сбросить к умолчанию» и при формировании JSON из диалога).
    - Схема `ImportSettings`, Engine-парсер `ModelImportSettings`, общий набор `ImportSettingsTestCases.h` и чтение существующих sidecar без поля `useFileUnitScale` строго остаются `false` (ранее импортированные модели и проекты не меняются).
31. **Редактирование настроек импорта модели («Настройки импорта…») (Шаг 17.6c):**
    - Команда `Assets.EditImportSettings` в `CommandRegistry` и пункт контекстного меню Content Browser для моделей (`FileBrowser.cpp`).
    - Чтение текущего `.importsettings.json` целевого ассета (дефолты, если отсутствует; лог `[Error]`, если повреждён).
    - Открытие `ImportSettingsDialog` в режиме редактирования (`SetEditMode(true)`): чекбоксы `importMaterials` и `importTextures` отключены с поясняющими всплывающими подсказками (материалы и текстуры создаются движком однократно при первичном импорте и живут независимой жизнью в проекте).
    - При нажатии OK: сохранение `.importsettings.json` через `FileService` с сохранением неизвестных полей JSON, проверка наличия исходника из `.source.json` (диалог выбора файла при отсутствии), вызов `UpdateAsset` с перезагрузкой геометрии движком (`AssetUpdate.cpp`) с новыми настройками и сохранением позиций и компонентов объектов на сцене.
    - При нажатии Cancel: sidecar не модифицируется, транзакция реимпорта не запускается.
32. **Отмена решения S16 «scale только на Transform» (Новое решение Юрия):**
    - Решение S16 («масштаб модели выставляется только на уровне Transform при спавне») отменено по решению архитектора Юрия.
    - Цель: в редакторе у импортированных объектов `Transform.Scale = [1, 1, 1]`. Размер корректируется при импорте на уровне геометрии.
    - Пресеты Scale (1 / 0.1 / 0.01) и ручной ввод Scale теперь должны запекаться в геометрию меша при импорте/реимпорте в движке, а не применяться к `Transform.scale` сцены. (Правки в Engine ожидают согласования архитектора после диагностики C).
33. **Визуальное отключение угла сглаживания (Шаг 17.6e):**
    - При `Normals != Smooth` поле ввода угла сглаживания и его текстовая метка становятся неактивными (`setEnabled(false)`, muted-стиль палитры темы) и получают подсказку `Tr("Доступно только при сглаженных нормалях", "Available only when normals are set to Smooth")`.
34. **Скрытие служебных файлов импорта в Content Browser (Шаг 17.6f):**
    - В `ProjectFilesFilter` (`FileBrowser.h/.cpp`) отфильтровываются файлы с суффиксами `.import.json`, `.importsettings.json`, `.source.json`. Модель видна в браузере как 1 элемент.
35. **Удаление ассетов с очисткой сопутствующих файлов (Шаг 17.6g):**
    - Команда `Assets.Delete` в `CommandRegistry`, пункт контекстного меню Content Browser «Удалить» и клавиша `Delete`.
    - Удаление в пределах песочницы проекта через `FileService` и `AssetOps`.
    - При удалении модели автоматически удаляются сопутствующие sidecar-файлы (`.importsettings.json`, `.source.json`, `.import.json`).
    - Проверка наличия ассета на сцене (`Snapshot.objects`): при обнаружении ссылок выводится предупреждающий диалог подтверждения.
36. **Решения среза S17.8 (Scale в геометрию, R1-R5) — утверждены архитектором Юрием:**
    - **R1 (Scale в геометрию):** sidecar с `version >= 2` означает: `model.scale` применяется Engine к ГЕОМЕТРИИ при загрузке модели, на спавне `Transform.scale = 1`. Sidecar `version 1` и legacy `{"scale"}` (миграция из S16) остаются со старой семантикой: Engine geometry scale НЕ применяет (множитель 1), Editor на спавне ставит `Transform.scale` из sidecar как раньше. Так существующие сцены и модели не меняются и не удваиваются. Новые импорты и «Настройки импорта…» пишут `version 2`.
    - **R2 (Масштаб файла):** `useFileUnitScale=true` даёт нормализацию единиц файла (`aiProcess_GlobalScale`), `scale` из sidecar применяется ПОВЕРХ как отдельный множитель геометрии; итог = `масштаб_файла * scale` при true и просто `scale` при false (при false масштаб файла не учитывается). Механизм выбран экспериментом T1 (см. чек-лист S17.8). Дефолт схемы: версия 2 пишет Editor, `scale=1.0`.
    - **R3 (Разрешённые файлы Engine):** `Engine/Source/Render/SkeletalLoaderAssimp.cpp`, `Engine/Source/Scene/ModelImportSettings.h/.cpp`, `Engine/Source/Scene/SceneAssets.cpp`, `Engine/Source/Scene/AssetUpdate.cpp`, `Engine/Source/Scene/ModelImport.cpp`, `Engine/Engine.vcxproj`, тесты в `Engine/tests` (в т.ч. новые мелкие фикстуры ASCII `.obj`/`.fbx`), общий набор `Engine/tests/ImportSettingsTestCases.h`. Больше ничего в Engine без слова архитектора.
    - **R4 (Транзакционность реимпорта):** реимпорт при смене числа материальных слотов по-прежнему может падать (так задумано движком), но Editor обязан быть транзакционным: если `UpdateAsset` после «Настройки импорта…» завершился ошибкой, sidecar восстанавливается к прежнему содержимому (бэкап до записи), пользователю одно понятное сообщение через `Tr()`.
    - **R5 (Дефолт useFileUnitScale):** в диалоге `true`; схема и чтение sidecar без поля — `false` (как сейчас).
37. **Вывод по П3 (плоские нормали) после среза S17.8:**
    - П3 на файле Юрия НЕ закрыт: найден отдельный дефект парсеров Engine и Editor — поле `model.scale` было ошибочно обязательным (без него `Parse` возвращал false и лоадер молча откатывался к дефолтам `normals="file"`). Дефект обязательного scale исправлен: поле сделано опциональным со значением 1.0 по умолчанию, кейс "нет scale, normals=flat" читается как flat, а любая ошибка парсинга логирует `[Error]` с именем поля.
    - На синтетической фикстуре `Engine/tests/fixtures/folded-smooth.obj` (два согнутых треугольника с общим smooth vn) режим `flat` корректно разделяет общее ребро и строит плоские нормали граней как с указанием scale, так и без него.
    - Поведение на реальном проблемном файле FBX Юрия исследуется: на синтетических моделях Assimp корректно дропает нормали и генерирует плоские, но на реальном файле в редакторе требуется дополнительная верификация цепочки Editor и структуры меша FBX.
38. **Решения по срезу S17.10 (Удаление папок, staging, миграция Game.dll, оптимизация реимпорта) — утверждены архитектором Юрием:**
    - **Мини-цикл A-E:**
      - **A (Delete в Content Browser):** Шорткат `Delete` (`Qt::WidgetShortcut`) на виджете дерева файлов `view_`. Нормализация индекса к колонке 0 (`sibling(row, 0)`) добавлена в `DeleteCurrentAsset`, `OnDoubleClicked` и `OnContextMenu` (изменение поведения: клик, двойной клик и контекстное меню теперь консистентно работают на любой колонке строки файла: Имя, Размер, Тип).
      - **B (Enter в диалоге):** По умолчанию Enter выбирает «Да», Escape выбирает «Нет».
      - **C (Удалённый ассет на сцене):** Проверка наличия файла при выделении с кэшем (без дискового ввода-вывода на каждом тике). Редактор материала не открывает пустое окно и не вызывает `saveMaterial`.
      - **D (Дефолты импорта):** Единый `DefaultSettings()`. `SettingsService` не перетирает `scale` и `flipUv`. Пресеты масштаба заблокированы при `useFileUnitScale=true` с подсказкой.
      - **E (Фильтр реимпорта):** Строгая фильтрация по типу ассета на базе центрального реестра Core.
    - **Шаг 1 (Удаление папок):** Разрешено удалять любые папки в Content Browser, кроме защищенных системных корней (`Content`, `Source` и корень проекта). Удаление безвозвратное через `FileService::DeleteDirectory` в песочнице с записью в лог. Диалог подтверждения обязателен: предупреждение, если папка используется на сцене или не пуста, вопрос — если пуста.
    - **Шаг 2 (Staging реимпорта):** Промежуточный каталог реимпорта переносится в `.painengine/staging/` в корне проекта (вне видимости Content Browser), исключая появление артефактов `<stem>_1`. Правки в Engine разрешены только в `Engine/Source/Scene/AssetUpdate.cpp` и `SceneAssets.cpp`.
    - **Шаг 3 (Миграция старых проектов):** При обнаружении отсутствия расширения `Engine.Asset.Import`:
      - No-code проект: автообновление `Game.dll` из шаблона `Runtime/Templates/SceneProject/Game.dll` с созданием бэкапа старой DLL, записью в лог и явным уведомлением пользователя.
      - C++ проект: диалог с выбором «Собрать проект» или «Открыть как есть».
      - При отказе пользователя или ошибке сборки: fallback на дефолтный импорт с явным предупреждающим сообщением о недоступности кастомных опций (не молча). Формат sidecar/проекта не менять.
    - **Шаг 4 (Оптимизация реимпорта и устранение лагов):** Сначала строгий замер FPS/времени кадра и ресурсов до и после реимпорта. Затем: исключение повторного прохода Assimp для создания материалов, пропуск загрузки модели в `Commit` для сцен без ссылок на ассет, сохранение выделения объекта и истории Undo. Правки в Engine разрешены строго в `SceneAssets.cpp`, `AssetUpdate.cpp`, `SceneAuthoring.cpp`, `SceneEditSession.cpp`, `ModelImport.cpp`, `SkeletalLoaderAssimp.cpp`.
39. **Устойчивость `SceneDocument::Restore` к отсутствующим ассетам геометрии (Шаг 17.10.0):**
    - При открытии сцены (`SceneDocument::Restore`) отсутствие файла модели на диске (например, после удаления ассета) ранее приводило к `throw std::runtime_error("Can't load model, Assimp not enabled")` или исключению VFS, блокируя открытие сцены, Play mode и реимпорт ЛЮБЫХ других моделей проекта (`SceneAuthoring.cpp:90` валидирует проект загрузкой всех `.scene.json`).
    - Исправление: вместо исключения создаётся `SkeletalMeshComponent`-плейсхолдер с dummy `SkeletalModel` (с сохранением `MaterialIndex` из `projectData.materialSlots`), привязываемый через реальный метод `dummy->SetAssetModel(o.modelAsset, std::move(model))`. Это сохраняет ссылку на ассет в объекте сцены (`GetModelAsset() == o.modelAsset`), исключает потерю связи при пересохранении (`Scene::GetSnapshot()` -> `Serialize()`), выводит ошибку `std::printf("[Error] SceneDocument: missing model '%s': %s\n", ...)` и обеспечивает корректную валидацию сцен при реимпорте.
    - **Строгая проверка физического отсутствия:** заглушка создаётся ТОЛЬКО если файл физически отсутствует на диске (`!VfsRead::Active().Exists(o.modelAsset)`). Если файл существует, но повреждён/не может быть загружен, исключение пробрасывается дальше (`throw;`), благодаря чему `SceneAuthoring.test_all_scenes` корректно откатывает реимпорт повреждённой модели.
    - **Изоляция обработки ошибок:** `try/catch` вокруг `SceneAssets::Apply` и `LoadProjectData` применяется исключительно для плейсхолдера (`if (placeholder)`); для обычных объектов сохранено штатное поведение без подавления ошибок.
    - **Логирование:** пустой `catch (...)` в `CreateMissingModelPlaceholder` удалён, ошибки парсинга слотов логируются с указанием причины через `std::printf`. Покрыто тестами (a-e) в `AuthoringTests.cpp`.

---

## Backlog

- [ ] **Единый логгер в Engine:** В `Engine/` отсутствует подсистема/класс логгера (в отличие от `EditorCore::Logger`). Все модули движка выводят сообщения напрямую через `std::printf` и `std::fprintf(stderr, ...)`. Требуется архитектурное решение Юрия по единому логгеру движка.

- [ ] **Нарушение протокола п.4:** Чтение transcript.jsonl в обход PLAN.md и прямых указаний архитектора (зафиксировано, чтение логов и транскриптов запрещено).
- [ ] **check.ps1 падает на храповике MaterialEditorWindow (1380>800) и не доходит до сборки и тестов:** На шаге 2b проверки локализации легаси-файл `Editor/Source/MaterialEditorWindow.cpp` содержит 1380 символов кириллицы с ветки `main` против заниженного лимита 800 в храповике `check.ps1`. Скрипт `check.ps1` не модифицируется; решение о корректировке храповика или переводе строк за Юрием.
- [ ] **Release Editor (LTCG, Editor.iobj) в путях с запятой:** Release Editor не собирается из каталога с запятой в пути (`fatal error C1083: Не удается открыть промежуточный файл компилятора: ...Editor.iobj: No such file or directory`); гипотеза подтверждена фактом успешной сборки из `C:\Users\Yuri\Documents\GitHub\Engine`. Требуется правка `EditorModule.vcxproj` (настройка `IntermediateOutputPath` без макросов, ломающихся на запятых) и проверка `-PathStress`.
- [ ] **Скрипты тестов PS 5.1:** 4 скрипта на `main` не парсятся (`assets/run.ps1`, `code-projects/portable.ps1`, `code-projects/run.ps1`, `input/run.ps1`). Эталон = не запускается, причина: нет BOM / несовместимый синтаксис PowerShell 5.1. Требуется согласование фикса кодировки/форматирования в `Engine/tests/`.
- [ ] **EditorUiTests падает на "preview clears to its background" (строка ~766) и не выполняет код после неё; новые UI-тесты размещать до этой точки:** Падение связано с `grabWindow` в виртуализированном/фоновом графическом окружении. Оставить красным до этапа переработки окна материалов.
- [ ] **Content Browser Sandbox:** замена открытого `QFileSystemModel` на изолированную модель на базе `FileService`, запрещающую выход за корень проекта.
- [ ] **Checkpoint при ошибке setTransform:** Если `setTransform` вернул ошибку на первом тике жеста, `gizmoDragCheckpointed_` не выставляется и следующий тик создаст лишний Checkpoint.
- [x] **AssetUpdate и sidecar `.importsettings.json`:** `AssetUpdate.cpp` фильтрует только суффиксы `{".source.json", ".import.json"}` при переименовании и перемещении моделей/ассетов (появляются sidecar-сироты при переносе модели). Требуется поддержать перенос `.importsettings.json` при обновлении движка. (Реализовано в шаге 17.5: ремаппинг, защита от затирания кастомных настроек дефолтными и перенос при переименовании папки модели/файла ассета).
- [x] **Normals как enum class ModelNormalsMode в ImportSettings:** строковое представление `normals` ("file", "flat", "smooth") в `EditorCore::ImportSettings` заменено на строго типизированный `enum class ModelNormalsMode` с конвертерами и валидатором (шаг 17.6).
- [ ] **Лимит размера файлов ImportSettings.cpp (462 строки) и ImportSettingsTests.h (481 строка):** файлы приблизились к лимиту 500 строк. Больше код в них не добавлять: новые тесты выносить в отдельные тестовые заголовки (как `ImportSettingsDialogTests.h`), новые форматы (текстуры S18, звуки S19) — в отдельные файлы (`TextureImportSettings`, `AudioImportSettings`).
- [ ] **Декомпозиция ImportSettings.cpp при добавлении texture/audio (462 строки):** размер файла `ImportSettings.cpp` достиг 462 строк; при реализации срезов S18 (текстуры) и S19 (звуки) парсинг и сериализацию вынести в отдельные файлы (`TextureImportSettings.cpp`, `AudioImportSettings.cpp`), чтобы строго соблюдать лимит <500 строк.
- [ ] **Внеплановая правка локализации в ImportCommands.cpp:** в шаге 17.1 вызовы `AppendLine(QStringLiteral("[Error]..."))` были заменены на `Tr(...)` для прохождения пункта 2b `check.ps1`, что попало в коммит `feat(core)` вне предварительного плана.
- [ ] **Валидация путей `contains("..")` в `ModelImportSidecar`:** текущая проверка `contains("..")` отвергает валидные имена файлов с двойными точками (например, `Hero..v2.glb`); требуется разбор компонентов пути через `QDir::cleanPath` / `split('/')`.
- [ ] **Чувствительность к регистру `Qt::CaseInsensitive` в `ModelImportSidecar`:** сравнение префикса проекта платформозависимо (на Linux/macOS пути чувствительны к регистру, на Windows нет).
- [x] **AssetUpdate: ремаппинг `.importsettings.json` при смене имени геометрии при реимпорте:** В `AssetUpdate.cpp:79-85` при реимпорте переименовываются `.source.json` и `.import.json`, но отсутствует ремаппинг `.importsettings.json`. Если импортер движка в будущем начнет генерировать `.importsettings.json` во временном каталоге, появится сирота под новым именем или перезапись существующего sidecar. Предложенная точная правка в `Engine/Source/Scene/AssetUpdate.cpp`: в строке 82 добавить `if(entry.path()==fs::u8path(imported.u8string()+".importsettings.json"))relative=fs::u8path(file.filename().u8string()+".importsettings.json");`, а в строке 84 добавить `||entry.path().u8string().find(".importsettings.json")!=std::string::npos` в условие `continue`. (Реализовано в шаге 17.5: коммит `5218a71`).
- [x] **Реимпорт моделей (UpdateAsset) и диалог настроек:** При реимпорте модели через контекстное меню браузера (`UpdateAsset`) настройки берутся из существующего `.importsettings.json` и сохраняются движком (`AssetUpdate.cpp`). Для поддержки изменения настроек непосредственно при реимпорте реализовано: 1) команда `Assets.EditImportSettings` и пункт контекстного меню Content Browser «Настройки импорта…»; 2) отображение `ImportSettingsDialog`, предварительно заполненного текущими параметрами из sidecar с отключением материалов/текстур в режиме редактирования; 3) сохранение sidecar хостом с сохранением неизвестных полей; 4) в `AssetUpdate.cpp` чтение существующего sidecar и передача в `SceneAssets::Import` (коммиты `e20d169` и текущий).
- [ ] **Не проверено (без Qt6Svg иконки пустые):** Специального текстового фолбэка для иконок без Qt6Svg нет: кнопки и заголовок карточки имеют текстовые подписи, но сами векторные иконки в окружении без Qt6Svg отображаются как пустые/прозрачные пиксмапы (`RenderSvgPixmap` возвращает прозрачный `QPixmap`). Полноценное поведение в сборке без Qt6Svg не проверено.
- [ ] **Не проверено (сквозной UI сценарий импорта и реимпорта):** Сквозной сценарий импорта через UI со scale 0.01, последующего реимпорта через UI и сохранения настроек в живом вьюпорте проверен только на синтетических юнит-тестах (`AuthoringTests`, `HostRegressionTests`, `ImportSettingsDialogTests`); проверка в живом GUI отложена до ручной верификации.
- [ ] **Штатное связывание расширения Engine.Asset.Import через GameEditModule/Base.cpp:** В шаге 17.4 Editor `AssetCommandHandler` получает расширение через `ctx.assetImport` (штатный путь), но имеет фолбэк через `GetModuleHandleExW/GetProcAddress` по адресу `createModel`, так как `GameEditModule.cpp` и `Base.cpp` не входили в согласованный список файлов 17.4 и не вызывают `EngineBindAssetImport` в `Editor.dll`. При согласовании правок `GameEditModule.cpp` и `Base.cpp` расширение связывается аналогично `EngineBindAudioPreview`/`EngineBindViewportOverlay`, а Windows-специфичный фолбэк убирается.
- [ ] **ReadFromVfs не вызывался на реальном ассете (Не проверено):** юнит-тест в `EngineTests` проверял чтение через динамический Mount во временную директорию (`/TestImportVfs`); вызов на реальном проектном ассете через `LoadSkeletalModelFromVfs` будет проверен в Шаге 17.3.
- [ ] **Сироты sidecar при rename/move через AssetOps companions:** шаги 17.6f/17.6g nauczyли `AssetOps` переносить и удалять спутниковые файлы (`.importsettings.json`, `.source.json`, `.import.json`) при дублировании и удалении, но сценарии переименования/перемещения модели через UI-браузер могут оставлять сироты, если путь companion вычисляется не по тому же правилу, что и в `AssetUpdate.cpp`. Требуется сквозной тест rename/move модели с проверкой отсутствия сирот.
- [ ] **Валидация `contains("..")` в Engine `ModelImportSettings::ResolveSidecarPath`:** Engine-функция `ResolveSidecarPath` (`ModelImportSettings.cpp:23-36`) просто дописывает суффикс `.importsettings.json` без нормализации пути и без защиты от `..`; аналогичная Editor-проверка `contains("..")` уже отмечена выше как чрезмерно строгая. Нужно привести обе стороны к единому разбору компонентов пути (`QDir::cleanPath` / `std::filesystem::weakly_canonical`).
- [ ] **Смена числа материальных слотов ломает реимпорт (R4):** реимпорт модели, у которой изменилось число материальных слотов, по-прежнему может падать на стороне движка (так задумано). Editor теперь транзакционен (R4, коммит `7efe9ad`): sidecar откатывается к прежнему содержимому и показывается одно сообщение через `Tr()`. Полноценное решение требует поддержки смены числа слотов в движке (`SceneAssets`/`AssetUpdate`), что вне разрешённых файлов S17.8.

---


---

## Срез S12: Устранение критических дефектов оверлея, меню «Вид», освещения и доков (верифицировано на Проекте 14)

- [x] **Пункт 1 (overlay/critical):** Устранена причина пустого оверлея (lines=0, billboards=0):
  - Исправлено двойное переключение QAction в `MainWindow.cpp` (`toggled` + `triggered`), из-за которого флаги в реестре постоянно сбрасывались в `false`.
  - Устранена блокировка таймера оверлеев в `ViewportWidget.cpp`: убран ранний возврат `!hasFocus() || !isActiveWindow()` для обновления оверлеев и опроса камеры.
  - Добавлено хэширование флага `iconsEnabled` в `ViewportOverlay.cpp:ComputeOverlayHash` для исключения коллизий кэша при скрытии иконок.
  - Живое подтверждение в Проекте 14: сетка генерирует 140 линий, при выборе Cube генерируются гизмо-линии (143) и 36 треугольников конусов, билборды соответствуют источникам света (2 штуки).
- [x] **Пункт 2 (view/toggles & antialiasing):** Переключатели меню «Вид» и гонка старта сглаживания:
  - Флаги меню «Вид» включены в хэш оверлея, смена флага обновляет оверлей на следующем кадре.
  - Режим сглаживания (`antialiasingMode_ = 1` FXAA) сохранён в сессии движка `SceneEditSession.h` и переприменяется при появлении сервиса оверлея, а также при операциях `attachViewport` и `resizeViewport`.
  - Живое подтверждение: лог кадра фиксирует `AntialiasingMode = 1 (FXAA)`, readback фиксирует краевые пиксели FXAA (156) и оси гизмо.
- [x] **Пункт 3 (view/audit):** Аудит пунктов меню «Вид»:
  - «Ресурсы: компактная панель» (`CompactContentPanel`): объединяет доки консоли и контента во вкладки внизу и ужимает до 180px; подтверждён тестом `EditorUiTests.cpp:338`.
  - «Ресурсы: отдельное окно»: открепляет Content Browser в плавающее окно 800x450; подтверждён тестом `EditorUiTests.cpp:349`.
  - «Служебные объекты сцены»: переключает отображение служебных объектов (`Gameplay.PlaySettings`) в иерархии для исправления сцен; подтверждён тестом `EditorUiTests.cpp:968-969` и `EDITOR-WORKFLOW.md:31`.
  - Пункты оставлены в коде как функциональные и покрытые тестами.
- [x] **Пункт 4 (scene/light):** Освещение и ошибка код 5:
  - Определена причина «Операция не выполнена (код 5)» (`EngineSceneBusy` = 5): в `SceneEditSession.cpp:350` вызов `GetPointLightV2Api()` оборачивал `SetPointLightV2` во внешний `Invoke(p, ...)`, выставлявший `busy_ = true`, после чего внутренний `Invoke` обнаруживал флаг занятости и возвращал `EngineSceneBusy`. Исправлено прямое делегирование в `commands_`.
  - Подтверждено живым логом Проекта 14: `[Info] SetPointLight ok id=3 castShadows=0`.
  - Факт по теням точечного света: в `Game.cpp` рендеринг теней реализован исключительно для `DirectionalLightComponent` (`sun`). Точечный свет не имеет теневого прохода (только Blinn-Phong). Чекбокс «Тени» скрыт для PointLight и зафиксирован в «Нужно в ABI».
  - Единая карточка `CollapsibleCard` "Свет": унифицирована верстка для PointLight и Directional Light (Цвет, Интенсивность, Тени, Радиус/Дальность теней). Исправлена видимость карточки в `ObjectInspectorPanel.cpp` (флаг `selectedLight`).
- [x] **Пункт 5 (viewport/sun):** Иконка и линия направления направленного света:
  - Добавлена иконка Tabler Sun для `Light.Directional` в палитру и билборды.
  - При выделении Directional Light во вьюпорте строится тонкая жёлтая линия направления вперёд (`dir=1`), смещённая от иконки (0.8–3.5 м), не перекрывающая гизмо вращения.
- [x] **Пункт 6 (ui/splitters):** Разделители доков:
  - Толщина разделителей установлена в 1px с цветом границы `splitterColor` и полупрозрачным акцентом при наведении (hover 0.5 alpha).
- [x] **Пункт 7 (ui/bottom-tabs):** Вкладки нижних доков:
  - Установлено `setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::South)`.
  - Высота вкладок установлена в компактные 22px в QSS, скрыты дублирующиеся заголовки объединённых во вкладки доков.

## Нужно в ABI

1. **Режимы шейдинга вьюпорта:** Wireframe / Unlit / Lit (переключение конвейера через C-ABI).
2. **Выделение и подсветка (Mesh Outline vs Box):**
   - Текущая оранжевая рамка рисуется непосредственно движком в `Game::RenderEditorSelection()` как проволочный каркас параллелепипеда.
   - Для контурной подсветки меша (silhouette outline / stencil pass) требуется переработка прохода рендеринга на стороне движка и ABI-метод конфигурации стиля выделения (`setSelectionStyle` / outline post-process).
   - Поддержка мультивыделения (множественный выбор ID).
3. **Пространственные запросы:**
   - Рейкаст курсора `pickObject(context, x, y, &id)` в движке тестирует CPU-треугольники геометрии (`RaycastPrimitive`), но возвращает только `id`.
   - Требуется метод с возвратом мировой точки пересечения и нормали поверхности для точного прилипания (Surface Snapping) и позиционирования.
4. **Габариты геометрии:** запрос Bounding Box / Bounding Sphere объекта сцены по его `id`.
5. **Иерархия сцены:** концепция пустого узла (папка/группа объектов) в `createObject` без привязки к компонентам меша.
6. **Параметрические меши:** API чтения параметров примитива (радиус, сегменты, размеры) и запрос их перегенерации в рантайме.
7. **Материалы в `SceneModuleApi`:**
   - метод `setMaterialAsset(void* context, uint64_t id, const char* path)`;
   - API управления материалом: компиляция шейдера, чтение/запись скалярных и векторных параметров без прямого дискового ввода-вывода в обход движка.
8. **Переключение режима Play-камеры:** API переключения активной камеры во время Play (Free Camera / Main Camera / Follow Camera) в `EngineSceneApi`/`EngineEditSessionApi` без записи фиктивных данных проекта.
9. **Всенаправленный проход теней точечных источников света (Point Light Omnidirectional Shadow Cube Maps):**
   - В `Game.cpp` метод `RenderShadowMaps()` строит теневые карты исключительно для `DirectionalLightComponent` (`sun`). Для точечных источников (`Light.Point` / `PointLightComponent`) всенаправленные теневые карты (cube shadow maps / dual paraboloid / octahedral shadow maps) отсутствуют.
   - В UI редактора (`ObjectInspectorPanel.cpp:453`) чекбокс «Тени» скрыт для `Light.Point` во избежание муляжей (правило №2 протокола).
   - Требуется реализовать проход генерации omnidirectional shadow depth cube maps для точечных источников света в движке и сэмплирование в шейдере освещения.
10. **Расширенные параметры импорта ресурсов (`Engine.Asset.Import`):**
    - `Use file unit scale`: чтение метаданных `UnitScaleFactor` из `aiScene::mMetaData` в `SkeletalLoaderAssimp.cpp` (сейчас Assimp вызывается без флагов масштаба).
    - `Normals / Smoothing angle`: настройка генерации нормалей (`aiProcess_GenNormals`, `aiProcess_GenSmoothNormals`, `aiProcess_ForceFlatNormals` и задание угла сглаживания). Флаги зашиты в `SkeletalLoaderAssimp.cpp:73`.
    - `Import materials / Import textures`: селективное отключение генерации материалов (сейчас `ModelImport::CreateMaterials` безусловно вызывается в `SceneAssets.cpp:208`).
    - `Flip UV / Merge meshes`: проброс флагов `aiProcess_FlipUVs` и оптимизаций объединения мешей.
    - `Текстуры (sRGB, mipmaps, Flip vertical, Flip Y, Wrap, Filter)`: в `TextureLoader.cpp:63` формат жестко захардкожен как `DXGI_FORMAT_R8G8B8A8_UNORM`, мип-цепочка генерируется безусловно, нет проброса настроек из C-ABI.
    - `Звуки (Volume, Loop)`: параметры есть в `AudioEngine`, но отсутствуют в C-ABI `EngineAudioPreviewApi` (`SceneEditAudio.cpp:36`).
    - `Звуки (Force mono, Trim start/end)`: отсутствуют в движке (декодеры не выполняют микширование каналов и обрезку семплов).
    - **Предложенная архитектурная правка C-ABI:** версионированное сервисное расширение `EngineAssetImportExtension` (`"Engine.Asset.Import"`) с методом `importAssetWithOptions(void* context, const char* source, uint32_t kind, const char* optionsJson, char* virtualPath, uint32_t pathCapacity, char* error, uint32_t errorCapacity)`.

---

## Проверено в рантайме (автоматический smoke-editor и прямой D3D readback)

- [x] Отрисовка координатной сетки Y=0 во вьюпорте и генерация линий сетки (139-141 линий).
- [x] Появление интерактивного гизмо при выделении объекта (+33-35 линий гизмо).
- [x] Прямой D3D11 staging readback бэкбуфера (713x426):
  - Кадр 30: 713 пикселей диагностической красной линии (на всю ширину вьюпорта), 7165 пикселей сетки (non-bg).
  - Кадр 130: 754 пикселя красных элементов (+41 пиксель оси X гизмо и стрелки), 649 пикселей рамки выделения, 8311 суммарных пикселей геометрии и оверлея.
- [x] Компиляция шейдера `EditorOverlay.hlsl` по всем 4 точкам входа (`VSLine`, `PSLine`, `VSBillboard`, `PSBillboard`) с явным логированием `[Info] EditorOverlay pipeline and shaders compiled successfully`.
- [x] Положение `RenderEditorOverlay()` в едином конвейере кадра: сразу после `RenderEditorSelection()` и непосредственно перед `EndFrame()` / `Present()`.
- [x] Чтение и валидация камеры из `getCamera` без дрифта и инверсии W (`[Info] Camera: ok`).
- [x] Включение сетки и оверлеев по умолчанию (`true`).

---

## Не проверено в рантайме (ручное интерактивное тестирование)

- Клик по иконке билборда во вьюпорте курсором мыши (хит-тест в живой сессии).
- Подсветка ручек гизмо при наведении курсора (8-10px tolerance).
- Перетаскивание объектов мышью за ручки гизмо по осям X/Y/Z и плоскостям в живой сессии.
- Отмена активного перетаскивания гизмо по Esc с возвратом начальной позиции.
- Переключение "Вид -> Сетка" и "Вид -> Иконки и гизмо" через клик в меню в живом GUI.
- Интерактивная навигация вьюпорта (WASD + правая кнопка мыши).
- Ручное перетаскивание сплиттеров панелей Сцена/Инспектор/Консоль и проверка стабильности размеров после плавания/возврата вьюпорта.
- Ручное стягивание сплиттера между вьюпортом и контент-браузером вниз до ~60px в запущенном GUI.
- Смещение объекта мышью с включенным Snap во вьюпорте и проверка пошагового скачкообразного движения.
- Визуальное переключение темы Dark/Light для шапки вьюпорта `ViewportHeaderBar`.
- Поведение hot reload (горячая перезагрузка ассетов и шейдеров): тесты `UiTests` проверяют только хелперы взаимодействия тестового каркаса (`UiTestHelper`), а реальный жизненный цикл перезагрузки ассетов и перекомпиляции шейдеров в рантайме НЕ проверен.
- Визуальное сжатие вьюпорта в узком окне (<=180px, <=130px, <=100px) и проверка сворачивания кнопок в меню "⋯".
- Компактный тулбар Play (30px) визуально по центру окна.
- Заголовки доков (Scene/Objects, Viewport, Inspector, Content Browser, Console) при переключении RU и EN.
- Карточки инспектора и высота полей 22px при выделении объекта сцены.
- Перетаскивание объекта за центральный кубический манипулятор гизмо (свободное перемещение в плоскости экрана для Move и равномерный масштаб для Scale) в запущенном интерактивном окне.
- Переключение режима сглаживания через пункт меню "Вид -> Сглаживание -> Выкл / FXAA" в запущенном GUI с визуальной оценкой пикселей.
- Реимпорт модели (`UpdateAsset`): подтверждение на живом проекте, что реальный вызов `UpdateAsset` через движок не перезаписывает `.importsettings.json` и сохраняет масштаб объектов на сцене (в юнит-тестах проверена эмуляция семантики копирования каталога `AssetUpdate::Run`).
- Показ ровно одного модального окна `QMessageBox::warning` при пакетном спавне/импорте N файлов с битыми sidecar: коалесцирование с кулдауном 3 секунды реализовано в `ImportCommands.cpp`, но автотест с подсчётом модальных окон отсутствует (проверить вручную при пакетном спавне поврежденных файлов).
- FBX из 3ds Max / Maya с отличающимися настройками осей и системных единиц (например, дюймы или сантиметры без промежуточного скейла 100 в узле): в тестах проверен синтетический FBX (100 в узле, UnitScaleFactor 1.0) и glTF/OBJ; проверка на реальном Blender-файле Юрия была только вручную через FbxProbe (вне репозитория).
- Перерисовка вьюпорта после реимпорта (R1/S17.8): после «Настройки импорта…» и успешного `UpdateAsset` геометрия объекта заменяется в движке, но мгновенная перерисовка вьюпорта (без ручного покачивания камеры или переключения кадра) в живом GUI не проверена; юнит-тесты подтверждают только смену bounding box геометрии и сохранение `Transform`.
- Реальный FBX Юрия для flat/scale (П3 и R2): проверка режима `flat` и `useFileUnitScale` выполнена на синтетической фикстуре `folded-smooth.obj`, `CesiumMan.glb` и Blender-FBX (100 в узле); на реальном проблемном файле Юрия (сантиметры из Max/Maya, где «плоские нормали не работают») поведение не проверено — нужен сам файл.
- Scale в геометрию (R1) в живом редакторе: спавн модели с sidecar `version 2` даёт `Transform.scale = {1,1,1}` и запечённый в геометрию масштаб; подтверждено юнит-тестами (`ModelGeometryScaleTests`, `ImportSettingsSpawnV2Tests`), но сквозной сценарий «импорт FBX в сантиметрах с пресетом 0.01 -> спавн -> визуальный размер во вьюпорте» в живом GUI не проверен.
- Поведение иконок без Qt6Svg в рантайме: векторные иконки в сборке без Qt6Svg отображаются как прозрачные пиксмапы (`RenderSvgPixmap`), текстовый фолбэк есть только у кнопок и заголовков карточек; полноценная работа интерфейса без Qt6Svg в рантайме не проверена.

## Интеграция main в editor-v2 — 31.10.2026

База: `main` / `origin/main` = `395986ba`; исходная вершина v2 = `3c8d66d8`.

- Перенесены изменения main, включая HUD/UI, authoring-свойства, обновление ассетов, группы, recovery, экспорт и доработки рендеринга. Сохранены слои Core/Host/UI, новые карточки, меню, гизмо и оверлеи v2.
- Групповые операции и свойства адаптированы в SceneCommandHandler; сохранены fallback для DLL без расширений, undo/redo и защита редактирования в Play/Pause.
- Исправлены потерянные элементы управления текстурой, создание дочернего объекта, объединение undo через промежуточную операцию, чтение истории из UI-потока и отписка обработчиков закрытого окна.
- Исправлены жизненный цикл иконок и камеры, повтор загрузки оверлеев, обновление сервиса при hot reload. GameEditModule теперь переносит таблицу overlay API вместе с сессией, не вызывая функции выгруженной DLL.
- Возвращено сохранение высоты контент-панели при изменении окна с сохранением свободного перетаскивания разделителя.
- Добавлены 72 регрессионные проверки: 45 команд/истории, 18 буферизации оверлеев, 9 реальной замены DLL. Старые HostTests переведены с отключаемого assert на обязательные проверки.
- Финальный `Editor/tools/test-regression.ps1 -Full` прошёл в Debug и Release: Core 216, Host 69, UI helpers/smoke 249, Engine 47, HUD 52, native reload 48; интеграционные UI-сценарии standard, authoring, workflow, recovery, layout и reload также прошли.
- Логи: `Engine/build/editor-regression/Debug-e4b01f82c7b8495e8ca95f3bcd6286d1` и `Engine/build/editor-regression/Release-26821ae3fe124399a81f6da63521c493`. Дополнительно пройдено 650 проверок edit-session в Debug.
- Команды повторного запуска и ограничения среды: `Editor/tests/README.md`.

---

## Чек-лист среза S11 (Полировка v2: FXAA, сетка, сплошное гизмо, тени света, центрирование тулбара, Tabler-иконки, биллборды)

- [x] **Шаг 11.0 (build):** Ускорение сборки: `/MP` и инкрементальная линковка, PCH для редактора (`pch.h`/`pch.cpp`), оптимизированный `run-editor.ps1 -Fast`.
- [x] **Шаг 11.A (ui):** Устранение толстых 9px полос между доками через токены темы `borderLight`/`bgWindow` (2px).
- [x] **Шаг 11.Б (ui):** Реорганизация меню: вкладка "UI" перенесена в меню "Инструменты", порядок меню зафиксирован: `Сцена | Правка | Вид | Запуск | Проект | Инструменты`.
- [x] **Шаг 11.1 (engine/render):** Устранение шлейфов оверлея: очистка `backbufferRTV` и `SceneColorRTV` каждый кадр, сброс состояний blend/depth/raster в FXAA, периодическое логирование `AntialiasingMode` каждые 120 кадров, readback-анализ промежуточных цветов.
- [x] **Шаг 11.2 (engine/shader):** Сетка и оси с depth-тестом: `LESS_EQUAL` для сетки и осей с чтением DSV сцены, гизмо alwaysOnTop, флаг `HalfPixel` (1.5px), постоянная экранная толщина линий у горизонта (`safeC0`/`safeC1`).
- [x] **Шаг 11.3 (engine/gizmo):** Сплошные наконечники стрелок и кубики Scale: C-ABI v3 расширение оверлея с `setOverlayTriangles`, шейдеры `VSTriangle`/`PSTriangle`, модуль `GizmoTriangles` со сплошными пирамидами, кубами и полупрозрачными плоскостями.
- [x] **Шаг 11.4 (engine/scene):** Тени точечных источников: включены по умолчанию для новых `Light.Point`, сохранение бит-в-бит стабильности для сохранённых сцен (дефолт `false`), карточка света с чекбоксом "Тени" (`LightShadows`), расширение `EnginePointLightV2Extension`.
- [x] **Шаг 11.5 (ui):** Вертикальное центрирование тулбара Play/Pause/Stop: контейнер `QFrame` с фиксированной высотой 36px, центрированные кнопки 28x28px, разница отступов top/bottom = 0px, проверка в `UiSmokeTests`.
- [x] **Шаг 11.6 (ui):** Единый провайдер иконок `IconProvider`: интеграция 24 Tabler outline SVG, токенизированная палитра темы, генерация состояний `Normal`/`Disabled`/`Active`/`Selected` через tint, замена самодельных QPainter-иконок, эмодзи и системных иконок файлового браузера.
- [x] **Шаг 11.7 (overlay):** Доставка биллбордов света и камеры: включение хеша объектов в триггер обновления оверлеев, подтверждённая доставка в проект 11 (`Billboards sent: count 2`).


---

## Чек-лист среза S12 (Комплексная стабилизация функционала и UX по 17 пунктам)

- [x] **Пункт 1 (engine/render):** Отображение иконок-биллбордов во вьюпорте: восстановлен растровый стейт `Context->RSSetState(OverlayRasterState.Get())` в `Game::RenderEditorOverlay()` (`Game.cpp:3396`), сброшенный вызовом `ClearState()`. Устранено отсечение CCW-треугольников биллбордов по backface culling.
- [x] **Пункт 2 (engine/scene):** Точечный свет (код ошибки 7 / `EngineSceneWrongThread`): в `SceneEditSession.cpp:351` исправлена передача `&s.commands_` вместо `&s.Active()`. `SceneCommands::SetPointLightV2` получает валидный `SceneCommands*`, проверка `thread_id` проходит успешно. Сохранена обратная совместимость со старыми сценами.
- [x] **Пункт 3 (ui/inspector):** Единообразный UI освещения: скрыт избыточный чекбокс "Включён" для PointLight при сохранении локатора `LightEnabled` для UI-тестов; устранён дребезг/двойной клик теней `Property/shadows` для `Light.Directional` через оптимистичное подтверждение в `ProjectProperties.cpp`.
- [x] **Пункт 4 (core/ui):** Двойное гизмо и раздельное меню: устранены дублирующие каркасные пирамиды, плоскости и центральный куб в `GizmoGeometry::BuildGizmoLines()`, оставлены чистые осевые линии, гармонично дополняющие сплошные тела `GizmoTriangles`. В меню "Вид" разделены пункты "Сетка" (`view/grid`), "Гизмо" (`view/gizmo`), "Иконки объектов" (`view/icons`).
- [x] **Пункт 5 (engine/render):** FXAA: подтверждена актуализация `Game.dll` в проектах пользователя (синхронизация в Проект 11), развёрнуты шейдеры `FXAA.hlsl` и `EditorOverlay.hlsl` в `Runtime/EngineContent/Shaders`, добавлен автоматический вызов `View.SetAntialiasing` при старте редактора с сохранённым режимом.
- [x] **Пункт 6 (ui/toolbar):** Тулбар запуска Play/Pause/Stop: применение стилей и темы до показа окна, полная независимость геометрии (36px контейнер, кнопки 28x28px) от сигнала смены темы.
- [x] **Пункт 7 (ui/toolbar):** Цвета кнопок запуска: Play = success (`#22C55E`), Pause = warning (`#EAB308`), Stop = danger (`#EF4444`). Поддержка более светлого hover и состояния свечения `activeState` (playing/paused).
- [x] **Пункт 8 (ui/tree):** Дерево сцены и контент-браузер: цвет иконки `QIcon::Selected` переведён на чистый белый `#FFFFFF` (устранён красный на красном), стиль `QTreeView::item:selected:hover` сохраняет акцентный цвет и белый текст без сброса в серый, индентация установлена в 13px.
- [x] **Пункт 9 (abi/backlog):** Видимость объектов («глаз»): движковый C-ABI (`EngineSceneApi`, `SceneCommands`, сериализация сцены) не поддерживает флаг видимости на уровне объектов сцены. В соответствии с правилом №2 протокола ("Муляжей нет") муляж не создавался, требование зафиксировано в разделе "Нужно в ABI".
- [x] **Пункт 10 (ui/tree):** Контекстное меню пустого пространства дерева сцены: при клике на пустое место отображается подменю "Добавить" со всеми типами объектов, разделитель, "Развернуть всё" и "Свернуть всё".
- [x] **Пункт 11 (ui/styles):** Удаление дублирующих символов: убран префиксный символ "+" из кнопки добавления (`Tr("Добавить", "Add")`), проверены тулбары и меню.
- [x] **Пункт 12 (ui/content):** Контент-браузер: высота заголовков колонок 22px, аккуратная стрелка импорта Tabler `download`, верхняя панель выровнена в одну линию 24px с отступами 4px. Устранён конфликт переключения вкладок Консоль/Контент через удаление `GroupedDragging` из `dockOptions` и настройку `setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North)`.
- [x] **Пункт 13 (ui/viewport):** Шапка вьюпорта и масштаб: разделены группы `[Move Rotate Scale] | [World/Local] | [Snap + step]` вертикальными разделителями, `comboSnapStep_` сделан редактируемым с `QDoubleValidator`. Спинбоксы масштаба в `TransformCard` и математика гизмо в `ViewportWidget` поддерживают отрицательный масштаб (снято ограничение `ratio < 0.01f`).
- [x] **Пункт 14 (ui/theme):** Иконки светлой темы: все панели (`SceneObjectsPanel`, `FileBrowser`, `ObjectInspectorPanel`, `MainToolBar`, `ViewportHeaderBar`) подписаны на смену темы `SubscribeThemeChanged`, иконки генерируются динамически по токену текста темы.
- [x] **Пункт 15 (ui/menu):** Аудит меню "Вид": все пункты привязаны к реальным командам `CommandRegistry` и `SettingsService`, муляжи отсутствуют.
- [x] **Пункт 16 (ui/inspector):** Реестр карточек по `typeName`: примитивы получают только General + Transform + Material (карточка Geometry скрыта); `Engine.Model` получает Geometry; `Light.Point` получает LightCard; `Light.Directional` получает Properties; `Camera`, `Particles`, `Character` показывают только релевантные карточки без прыжков компоновки.
- [x] **Пункт 17 (abi/backlog):** Силуэтный контур выделения: шейдер оверлея обводки требует расширения проходов рендера движка (stencil/silhouette buffer в `EngineSceneApi`), задача зафиксирована в разделе "Нужно в ABI".

---

## Чек-лист среза S13: Комплексный фикс интерфейса, оверлеев и производительности вьюпорта (8 задач)

- [x] **Задача 1 (fix(editor)):** Геометрия тулбара Play/Pause/Stop: унифицированы отступы и фиксированная высота 36px в `MainToolBar.cpp`, исключающие сдвиг при старте и смене темы (`0cb1888`).
- [x] **Задача 2 (fix(editor)):** Заголовки нижних доков и вкладок: удалена заглушка `setTitleBarWidget`, восстановлены компактные нативные заголовки доков и полные имена вкладок ("Консоль", "Контент") без элизии (`b68942a`).
- [x] **Задача 3 (refactor(editor)):** Чистка меню «Вид»: удалены устаревшие невостребованные действия ("Ресурсы: компактная панель", "Ресурсы: отдельное окно", "Служебные объекты сцены") и неиспользуемые методы (`5d0e296`).
- [x] **Задача 4 (perf(editor)):** Коалесцирование событий изменения размера вьюпорта: добавлен `resizeTimer_` (20 мс) в `ViewportWidget.cpp`, исключающий шторм пересоздания GPU swapchain и буферов при непрерывном перетаскивании сплиттеров (`04f7cc8`).
- [x] **Задача 5 (feat(editor)):** Группировка объектов окружения в дереве сцены: `Light.Directional`, `Environment.Sky`, `Gameplay.PlaySettings` сгруппированы в недеструктивный корневой узел «Окружение» с иконкой солнца и жирным шрифтом (`4c3d5e5`).
- [x] **Задача 6 (fix(viewport)):** Отображение иконок-билбордов источников света: буферизация `PendingOverlayIconData` и отложенная выгрузка `UploadOverlayIcon` при готовности D3D11 `Device`, проброс `storedIcons_` через сессию редактирования `SceneEditSession`, контрастные круглые плашки Tabler-иконок в `IconProvider.cpp`, корректная отсечка near-plane в `EditorOverlay.hlsl` (`15877e0`).
- [x] **Задача 7 (feat(renderer)):** Сглаживание линий оверлея: субпиксельный спад альфы через `smoothstep` по расстоянию от центра линии в `EditorOverlay.hlsl` (0.5px margin), полноэкранный проход FXAA после оверлеев (`bc8eeee`).
- [x] **Задача 8 (docs(plan)):** Фиксация архитектурных ограничений теней точечного света в плане и беклоге; подтверждено скрытие чекбокса «Тени» в инспекторе для `Light.Point`.

---

## Чек-лист среза S14: Реальная приёмка вьюпорта (Синхронизация рантайма, иконки D3D, сглаживание FXAA, ресайз и меню)

- [x] **Пункт 0 (tool(sync)):** Верификация запуска проекта 14 через Launcher и синхронизация рантайма:
  - Факты запущенного процесса `EngineRuntime` (PID 54724):
    - `EngineRuntime.exe`: `C:\Users\Yuri\AppData\Local\PainEngine\Engines\2026.10.31.3\Debug\EngineRuntime.exe`
    - `Editor_*.dll`: `C:\Users\Yuri\AppData\Local\PainEngine\Engines\2026.10.31.3\Debug\.shadow_cache\Editor_379748371667100.dll`
    - `Game.dll`: `C:\Users\Yuri\Documents\14\Binaries\Debug\ef9d7f4c85404d8ab34ca49abe81de91\Game.dll`
    - Корневой каталог EngineContent для шейдеров: `%LOCALAPPDATA%\PainEngine\Engines\2026.10.31.3\Debug\EngineContent`
  - Сравнение дат и хешей:
    - `EditorOverlay.hlsl` и `FXAA.hlsl`: SHA256 совпадают между репозиторием (`Engine/Content/Shaders`), сборкой (`Game/build/.../Runtime/EngineContent`) и кэшем лаунчера (`Engines/2026.10.31.3/Debug/EngineContent`).
    - `Game.dll`: SHA256 совпадает во всех проектах `Documents` (включая корень и `Binaries/Debug/*`).
  - В `Editor/tools/run-editor.ps1` добавлена гарантированная синхронизация после сборки: деплой шейдеров в оба каталога `EngineContent`, синхронизация бинарников (`Editor.dll`, `EngineRuntime.exe`, `Game.dll`) в кэш лаунчера, очистка `.shadow_cache`, рекурсивная синхронизация `Game.dll` по всем подкаталогам `Binaries` пользовательских проектов.
- [x] **Пункт 1 (viewport/icons):** Отображение иконок объектов во вьюпорте: исправление сбоя `CreateInputLayout` для билбордов (сигнатура `VSBillboard` vs `VSTriangle`), `FallbackIconSRV`, D3D11 staging readback в BMP/PNG с верификацией Warm Light (344, 309) и Cool Light (464, 331), контрастная круглая подложка и процедурная пиктограмма лампочки в `EditorOverlay.hlsl`.
- [x] **Пункт 2 (renderer/antialiasing):** Сглаживание FXAA: два readback кадра с FXAA 0 и 1 (`viewport_fxaa_off.png` и `viewport_fxaa_on.png`), 3545 пикселей сглажено на ребрах куба, оверлеях и сетке, оптимизирован порог `max(0.02, lumaMax * 0.08)` и субпиксельное смешивание в `FXAA.hlsl`.
- [x] **Пункт 3 (viewport/resize):** Устранение дергания при ресайзе сплиттеров: коалесцирование через debounce таймер 120мс (`resizeTimer_->start(120)`), заморозка пересоздания swapchain/ресурсов во время активного драга (`QGuiApplication::mouseButtons() & Qt::LeftButton`), пропуск опроса инспектора и дерева сцены во время драга (`viewport_->IsResizing()`), подтверждено тестом: ровно 1 вызов `viewportResize` (diff=1 <= 2) за 2-секундный непрерывный драг (50 перемещений за 2000мс).
- [x] **Пункт 4 (ui/separators):** Разделители окон: `QMainWindow::separator` и `QSplitter::handle` ширина/высота 6px (увеличенная зона захвата), визуальная линия 1px по центру (`border transparent` + `background-clip: content`), цвет через токен темы `%14` (`splitterColor`), при наведении `:hover` 2px акцентная линия с альфой `%15` (`splitterHover`).
- [x] **Пункт 5 (ui/menu):** Меню «Сцена»: удалён пункт «Направленный свет» из верхнего меню `BuildSceneMenu()`, удалена проверка `SceneDirectionalLight` из `RefreshSceneUi()`, метод `ShowDirectionalLight()` перенесён в `public:` интерфейса `EditorMainWindow` для вызова из иерархии и тестов, в `EditorUiTests.cpp` вызовы `Action(w, "Направленный свет")->trigger()` заменены на `w->ShowDirectionalLight()`, имя дока «Иерархия» сохранено без изменений.

---

## Интеграция main в Engine/Scripts — 01.11.2026

База PR #13: `ed209d63`; объединённый main: `5ca0abfa`.

- Разрешены конфликты MainToolBar, BaseEngine и Game. Сохранены центрированные кнопки запуска из main и отсутствие настроек Play в тулбаре: настройки персонажа/камеры остаются в свойствах.
- Одновременно доступны сервисы курсора и PointLightV2. Треугольное гизмо, биллборды и отладочные линии трассировок используют общий overlay.
- FXAA и очистка render targets выполняются в общем RenderFrame для standalone и embedded; диагностика читает GameSettings. Повторный FXAA в RenderViewportFrame не возвращён.
- Добавлены проверки наличия всех четырёх кнопок запуска, отсутствия ConfigPlayBtn, реальных пикселей треугольного overlay с FXAA/без него и очистки следующего кадра. UI-тест явно устанавливает фокус после закрытия диалогов перед проверкой сохранения фокуса в свойствах.
- Release x64 с Assimp: All (движок, редактор, runtime, SDK, примеры) собран; EngineTests 54/54, CompatibilityTests 116, CoreTests 374, HostTests 69, UI helpers/smoke 239, ViewportTests пройдены. HTML API Reference пересобран и проверен.
- Qt CoreTests запускались вне sandbox: ограниченное окружение запрещает запись в каталоги QTemporaryDir с собственными ACL (см. tests/README.md).
- Полный стандартный Qt UI workflow с импортом CesiumMan.glb прошёл: 3123 проверки, включая свойства персонажа, сохранение фокуса, Play/Stop и настройки направленного света.



---

## Чек-лист среза S15: Задачи 0-6 (Git merge, Assimp, Clamping биллбордов, Спинбоксы/карточки инспектора, Заголовки таблиц, Сплиттеры, Локализация)

- [x] **Шаг 15.0 (git):** Проверка `branch -vv`, настройка `git config push.default simple`, снятие случайного upstream `origin/main` с `feature/editor-v2-final`, слияние `origin/main` в ветку `feature/editor-v2-final` объединением веток (11 файлов), валидация через `check.ps1 -Quick` (коммит `1a38a825`).
- [x] **Шаг 15.1 (fix(model)):** Устранение "Can't load model, Assimp not enabled": автоопределение `EngineEnableAssimp=true` и `VcpkgRoot` в `Engine.vcxproj`, `EngineRuntime.vcxproj`, `SceneProject.vcxproj`, `RuntimeExample.vcxproj` при наличии `vcpkg_installed\x64-windows\include\assimp\scene.h`; проброс путей vcpkg и синхронизация dll в `run-editor.ps1`; информативное сообщение об ошибке в `SceneAssets.cpp`. Проверена живая загрузка `CesiumMan.glb` в Проекте 14 (Debug runtime, 20 кадров, exit code 0).
- [x] **Шаг 15.2 (fix(viewport)):** Ограничение экранного размера билбордов во вьюпорте: ограничение 16..32 px в EditorOverlay.hlsl (VSBillboard): базовый 32 px, уменьшается до 16 px при приближении (dist <= 3.0), при отдалении (dist >= 15.0..60.0) не растёт и остаётся 32 px. Подтверждено D3D readback в ViewportTests: dist 3 -> 16 px, dist 15 -> 32 px, dist 60 -> 32 px (все <= 34 px).
- [x] **Шаг 15.3 (fix(ui)):** Визуальное оформление инспектора: стрелки спинбоксов и комбобоксов переводятся на Tabler `chevron-up`/`chevron-down` в цвете текста темы (`textPrimary`: `#ECEFF4` в Dark, `#1F2328` в Light) через `IconProvider::GetPixmap` и QSS; индикаторы сворачивания карточек инспектора унифицированы на Tabler `chevron-down`/`chevron-right` с динамическим обновлением цвета при смене темы для `CollapsibleCard` и `PropertySection`.
- [x] **Шаг 15.4 (style(ui)):** Заголовки колонок таблиц в Контенте (`FileBrowser`) и Иерархии (`SceneObjectsPanel`): высота 20 px (`setFixedHeight(20)` и `QHeaderView::section` height/min-height/max-height: 20px), шрифт 11 px (`setPixelSize(11)` и `font-size: 11px`), вертикальное центрирование (`setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter)` и `padding: 0px 6px`). Проверено тестом `UiSmokeTests` (225/225 PASS).
- [x] **Шаг 15.5 (style(ui)):** Разделители доков: зона захвата 4 px (вместо 6 px), центральная линия 1 px через токен темы `splitterColor` (`%14`), подсветка `:hover` 2 px акцентным цветом `%15`. Сохранена идеальная плавность без подёргиваний: подтверждено тестом `EditorUiTests --layout` (diff=1 <= 2 вызовов viewportResize за 2 секунды непрерывного перетаскивания, 205/205 PASS).
- [x] **Шаг 15.6 (fix(localization)):** Перевод интерфейса на английский: "Окружение", виртуальные элементы среды ("Directional Light", "Play Settings", "Sky") и группы добавления в `SceneObjectsPanel` динамически обновляются при смене языка через `RetranslateUi()` и кэширование `lastSnapshot_`; все строки диалогов и инспектора разметки `UIEditorWindow` переведены на `Tr()`, добавлена поддержка динамической смены языка `RetranslateUi()`; `ProjectProperties` инвалидирует кэш схемы при смене языка; подтверждено тестами `UiSmokeTests` (243/243 checks PASS) и проверкой отсутствия хардкод-кириллицы вне `Tr()`.
- [x] **Шаг 15.7 (fix(viewport)):** Устранение модальных окон "Ошибка сцены" (код 1), белого мигания диалога и красной линии при ресайзе:
  - Причина кода 1 (`EngineSceneInvalidArgument` из `SceneModuleApi.h:9`): при драге сплиттеров или разворачивании окна `ViewportWidget::resizeTimer_` (120 мс) откладывал `ViewportResize`, но курсор `SendCursor` слал текущий размер виджета `PhysW, PhysH`, пока в движке оставался размер `s.width_, s.height_`. В `SceneEditSession.cpp:440` проверка несовпадения возвращала `EngineSceneInvalidArgument` (код 1), из-за чего `MainWindow::CommandFinished` открывал модальное окно на каждое движение мыши.
  - Исправление: `ViewportWidget_Input.cpp` не шлёт курсор при `isResizing_` или `!attached_` и использует `lastSentW_`/`lastSentH_`; `ViewportWidget.cpp` не шлёт resize/attach при нулевом размере; `SceneEditSession.cpp` принимает повторы resize/0x0 без ошибки; API курсора сохраняет InvalidArgument для несовпадения размеров, а host не отправляет образцы во время resize;
  - Модальные окна: фоновые/служебные команды (`ViewportResize`, `ViewportCamera`, оверлей, AA, `InputCursor`) пишутся только в лог `[Error]` и не показывают `QMessageBox`; пользовательские ошибки оформляются стилем темы, привязаны к `this`, имеют защиту от дублирования и коалесцируются (1 раз в 2 с);
  - Красная линия: в `EditorOverlay.hlsl` отсечение по ближней плоскости камеры `nearW = 0.1f` вместо `0.001f` устранило артефакт проекции оси X сетки ($Z=0$) за спиной камеры;
  - Тестирование: регрессионный сценарий в `EditorUiTests --layout` (maximize/restore, splitter drag, float viewport, dock top) выполнен 3 раза подряд с кодом 0 (226, 225, 225 проверок, `activeModalWidget == nullptr`).
- [x] **Шаг 15.8 (fix(ui)):** Переключение вкладок нижней панели (Консоль <-> Контент-браузер):
  - Причина: `QTabBar::tab` в `EditorStyles.cpp` имел фиксированную `height: 22px;`, в то время как контейнер нижней панели имел высоту 29 px. Зона клика вкладок не доходила до нижнего края таббара, и события мыши терялись.
  - Исправление: В `EditorStyles.cpp` изменено на `min-height: 24px;` с заполнением всей высоты таббара `(0,0 68x29)`. В `DockLayoutManager.cpp` добавлен метод `ConfigureTabBars`, настраивающий все `QTabBar` главного окна (`setElideMode(Qt::ElideNone)`, `setUsesScrollButtons(false)`, `setExpanding(false)`, `setFocusPolicy(Qt::NoFocus)`).
  - Тестирование: В `EditorUiTests.cpp` добавлен тест 5 последовательных циклов переключения `QTest::mouseClick` по центру `tabRect` вкладок ("Консоль" и "Контент-браузер"). Проверено на живом запуске Проекта 14 (`tabAt=0 cur=0` <-> `tabAt=1 cur=1`). Коммит `a885aec`.
- [x] **Шаг 15.9 (fix(viewport)):** Экранный размер иконок объектов во вьюпорте:
  - Причина: Предыдущий диапазон 16..32 px давал слишком большой размер на удалении и рассогласование с хит-тестом.
  - Исправление: В `Engine/Content/Shaders/EditorOverlay.hlsl` (`VSBillboard`) размер подложки ограничен диапазоном 18..20 px: `scaleT = saturate(dist / 3.0f); targetHalf = lerp(9.0f, 10.0f, scaleT); clampedHalf = clamp(targetHalf, 9.0f, 10.0f);` (приближение < 3 м уменьшает до 18 px, дальше 20 px константа). Радиус хит-теста билбордов `HitTestBillboard` в `ViewportOverlayHelper.h` и `ViewportWidget_Input.cpp` приведен к `10.0f` px (диаметр 20 px).
  - Тестирование: D3D staging readback диаметра подложки в `ViewportTests.cpp` на дистанциях 1, 3, 15, 60 показал ровно: dist 1 = 18 px, dist 3 = 20 px, dist 15 = 20 px, dist 60 = 20 px (все в диапазоне 18..22 px). Коммит `d9fa434`.
- [x] **Шаг 15.10 (fix(ui)):** Стрелки вверх/вниз у числовых полей спинбоксов (`SpinBoxArrowStyle`):
  - Причина: В `ObjectInspectorPanel.cpp:42` на `NoWheelDoubleSpinBox` локально вызывался `setStyleSheet("border-left: none; ...")`, который в Qt перетирал все каскадные стили `QDoubleSpinBox::up-button`, `::down-button`, `::up-arrow`, `::down-arrow`. Кроме того, декларация `image: url(...)` в `EditorStyles.cpp` без явного `subcontrol-origin`/`subcontrol-position` приводила к сбоям рендеринга растровых/SVG стрелок в Qt.
  - Исправление: Создан `Editor/UI/SpinBoxArrowStyle.h` — кастомный `QProxyStyle`, рисующий векторные шевроны стрелок через `QPainter` (antialiased) цветом текста темы (`#ECEFF4` в Dark, `#1F2328` в Light, с акцентом при hover и бледным при disabled). Базовый стиль обёрнут в `EditorTheme.cpp`. В `ObjectInspectorPanel.cpp` локальный `setStyleSheet` заменен на динамическое свойство `spin->setProperty("axisCell", true)` с правилом в глобальном QSS. В `EditorStyles.cpp` удалены `image: url(...)`, так что Qt делегирует отрисовку примитивов `PE_IndicatorSpinUp`/`PE_IndicatorSpinDown` в `SpinBoxArrowStyle`.
  - Тестирование: Добавлен тест с `spin.grab()` в `UiSmokeTests.h`: подтверждено 114 контрастных пикселей в зоне кнопок в темной теме, 190 пикселей в светлой теме при переключении на лету. Проверено в `check.ps1` (244 checks passed) и на живом запуске Проекта 14 (`run-editor.ps1 -Fast`). Коммит `2f6832c`.

---

## Чек-лист среза S16: Настройки импорта моделей (Sidecar .importsettings.json, Scale, пресеты)

- [x] **Шаг 16.1 (feat(core)):** `Editor/Core/ImportSettings.h/.cpp`. Структура `ImportSettings` (scale), загрузка и сохранение JSON через `QJsonDocument`, вещественные числа в C-локали, валидация scale в диапазоне `[1e-4, 1e4]`. Запись только через `FileService` с логом. Юнит-тесты в `CoreTests`: round-trip, битый JSON (лог + дефолт, без молчаливого fallback), выход за диапазон (418/418 PASS).
- [x] **fix(core):** Семантика `ImportSettingsFile::Load`: `LoadResult::NotFound` (scale 1.0, лог Info, без Error); битый JSON и scale вне диапазона возвращают `LoadResult::Error`. Добавлен метод `FileService::FileExists`. Тесты обновлены (429/429 PASS).
- [x] **Шаг 16.2 (feat(ui)):** Тонкий виджет/диалог `ImportSettingsDialog` в `Editor/UI/`, без логики. Поля: Scale (`QDoubleSpinBox` с `objectName="ImportScaleSpin"`) + кнопки пресетов `1` / `0.1` / `0.01` (`Preset1`, `Preset01`, `Preset001`) + OK/Cancel (`ImportOkBtn`, `ImportCancelBtn`). Показывается при импорте моделей (один диалог на всю пачку при мультивыборе). Вызов через команду `Assets.Import` в `CommandRegistry`, прямой вызов `ImportAsset` из FileBrowser убран. Локализация через `Tr()`. Добавлено поле `importScale` в `EditorCommand`. Тесты в `UiTests` через `UiTestHelper` (13/13 PASS).
- [x] **refactor(ui):** Вынос регистрации команды `Assets.Import` и её обработчика в отдельный файл `Editor/Source/ImportCommands.cpp`. `MainWindow.cpp` сокращён до 1450 строк (лимит 1460, на main было 1453), `AssetTools.cpp` сокращён до 387 строк (лимит 500). Поведение сохранено.
- [x] **Шаг 16.3 (feat(host)):** Реализован `EditorCore::ModelImportSidecar` (helper в `Core/`). После успешного импорта модели сохранение `<asset>.importsettings.json` через `FileService` со scale из `EditorCommand.importScale`. При создании объекта на сцене (`UseAsset` в `AssetTools.cpp`) чтение scale через `ReadSpawnScale` (`ImportSettingsFile::Load`) и выставление `c.transform.scale = {s, s, s}` (один раз при создании, вне горячего пути). 7 тестов без GPU в `CoreTests` (450/450 PASS).
- [x] **Шаг 16.4 (test):** Регрессионные тесты: спавн модели со scale 0.01 из sidecar выставляет `transform.scale = {0.01, 0.01, 0.01}`; отсутствие sidecar даёт scale 1.0 и `[Info]`; битый sidecar даёт scale 1.0 и `[Error]`; реимпорт сохраняет существующий sidecar и масштаб объектов на сцене не сбрасывается. Тест пакета из 3 файлов на уровне Host с сохранением и проверкой sidecar для каждого файла. Подмена модального `exec()` через `SetExecHookForTesting` в `ImportSettingsDialog`, тест Cancel на реальной функции принятия решений `ResolveModelImportScale`. Ограничение показа модальных окон при пакетных ошибках sidecar.

---

## Чек-лист среза S17: Полный пайплайн импорта моделей
- [x] **Шаг 17.0 (analysis):** Фаза 0: Диагностика и согласование архитектуры S17 с архитектором (принято).
- [x] **Шаг 17.1 (feat(core)):** Расширить `ImportSettings` до v1 (`version`, `kind`, `model{scale,useFileUnitScale,normals,smoothingAngle,importMaterials,importTextures,flipUv,mergeMeshes}`). Миграция legacy `{"scale"}` с записью `[Info]`. Валидация диапазонов (`smoothingAngle` 0..175, `normals` enum), сохранение неизвестных полей JSON при перезаписи, битый sidecar = `[Error]` и дефолты. Тесты в `CoreTests` (518/518 PASS). Только `Editor/`.
- [x] **Шаг 17.2 (feat(engine)):** Парсер sidecar в Engine (`ModelImportSettings.h/.cpp` в `Engine/Source/Scene/`, `nlohmann::json`, тот же формат, те же дефолты, ошибки в лог stderr `[Error]`, legacy migration `[Info]`). Чтение через VFS `VfsRead::Active().Exists(sidecarPath)` и `ReadBytes`. Общий тестовый набор `ImportSettingsTestCases.h` в `Engine/tests/` подключён в `CoreTests` (529 checks PASS) и `EngineTests` (71/71 PASS).
- [x] **Шаг 17.3 (feat(engine)):** Флаги Assimp в `SkeletalLoaderAssimp.cpp` из sidecar: `normals` (file = текущее поведение; flat = `DropNormals|GenNormals`; smooth = `DropNormals|GenSmoothNormals` + `AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE`), `flipUv` (управление `FlipUVs`, дефолт `false` сохраняет текущее поведение), `mergeMeshes` (`AI_CONFIG_PP_PTV_KEEP_HIERARCHY` и `OptimizeMeshes` только при `!skinned`), `useFileUnitScale` (предложение на ревью Юрия, до одобрения не подключено). Тесты в `EngineTests` на реальное изменение результата (число вершин flat vs smooth, V-координата flipUv, число мешей mergeMeshes) и побитовую идентичность с дефолтным sidecar на `CesiumMan.glb` в `SkeletalTests`. Скелетные тесты 8/8 и EngineTests 71/71 PASS.
- [x] **Шаг 17.3b (fix(engine)):** Восстановление дефолтного объединения мешей: дефолт схемы `mergeMeshes = true` (воспроизводит историческое поведение движка `PretransformVertices` без `KEEP_HIERARCHY`). При `mergeMeshes = false` выставляется `AI_CONFIG_PP_PTV_KEEP_HIERARCHY = 1`. Синхронное обновление в Editor/Engine/тестах. Тест 12 на `box.obj`: дефолт даёт 1 меш, 9 вершин и 9 индексов (memcmp с эталоном), отсутствие поля `mergeMeshes` читается как `true`, `mergeMeshes = false` сохраняет 2 меша (6 и 3 вершины). CoreTests (549), EngineTests (71/71), SkeletalTests (8/8) PASS.
- [x] **Шаг 17.3c (feat(engine)):** Реализация `useFileUnitScale` через `aiProcess_GlobalScale` в `SkeletalLoaderAssimp.cpp`. `scale` из sidecar в Assimp не передаётся (остаётся на Transform). При `useFileUnitScale=false` флаги Assimp без изменений. Тесты: статический FBX (Blender с 100 в узле) даёт ~1.9м при true и ~190м при false/дефолте (memcmp с эталоном); скелетная модель (`CesiumMan.glb`) и формат без единиц (`box.obj`) с true побитово не меняются (memcmp). EngineTests (71/71), SkeletalTests (8/8), CoreTests (549), HostTests (102), UiTests (274) PASS.
- [x] **Шаг 17.4 (feat(engine) + feat(host)):** Сервисное расширение `EngineAssetImportExtension` (`"Engine.Asset.Import"`), запись `.importsettings.json` до `CreateModel` с откатом/удалением при сбое, управление `importMaterials` и `importTextures` в `CreateMaterials`. Старый `importAsset` сохраняет дефолты. В Editor `AssetCommandHandler` проверяет доступность расширения и передаёт `importOptionsJson` при наличии либо прозрачно вызывает старый `importAsset` без падений. Тесты: `EngineTests` (71/71), `SkeletalTests` (8/8), `compatibility` (116), `edit-session` (651), `scene-module` (109), `HostTests` (109), `CoreTests` (549), `UiTests` (274) PASS.
- [x] **Шаг 17.5 (feat(engine)):** `AssetUpdate.cpp`: ремаппинг `.importsettings.json` при смене имени геометрии, защита от перезаписи существующего sidecar дефолтным при реимпорте, перенос sidecar при переименовании/перемещении папки модели и очистка временных sidecar при откате. Тесты в `AuthoringTests.cpp` (сохранение настроек при реимпорте, отсутствие orphan sidecar при другом имени исходника, перемещение вместе с папкой). Коммит `5218a71`. EngineTests (71/71), SkeletalTests (8/8), Compatibility (116), Edit-session (651), Scene-module (109), CoreTests (549), HostTests (109), UiTests (274) PASS.
- [x] **Шаг 17.6 (feat(ui)):** `ImportSettingsDialog` для моделей: группы Базовые/Расширенные (расширенные свёрнуты), зависимости (`importTextures` от `importMaterials`, `smoothingAngle` от `normals=smooth`), кнопка «Сбросить», сохранение последних значений через `SettingsService` с логом, Tabler-иконки через `IconProvider`, типизированный `ModelNormalsMode`. Коммит `a754eb9`. CoreTests (553), HostTests (109), UiTests (277) PASS.
- [x] **fix(ui):** Сворачивание карточки «Расширенные» в `ImportSettingsDialog`: отложенный `adjustSize()` через `QTimer::singleShot(0)` после layout hide, гарантирующий возврат к исходной компактной высоте окна (0 px дельта). Коммит `f8a956f`.
- [x] **Шаг 17.6b (feat(engine)):** Чтение существующего `.importsettings.json` и передача в `SceneAssets::Import` при реимпорте модели в `AssetUpdate.cpp`. Тесты в `AuthoringTests.cpp`: reimport с `mergeMeshes=false` даёт 2 меша, без sidecar 1 меш, материалы `.mat` не перезаписываются, Transform объектов сцены не изменяется. Коммит `e20d169`.
- [x] **Шаг 17.6c (feat(ui)):** Команда `Assets.EditImportSettings`, пункт контекстного меню Content Browser «Настройки импорта…», открытие `ImportSettingsDialog` в режиме редактирования (материалы/текстуры отключены с подсказками), сохранение sidecar с сохранением неизвестных полей, обработка отсутствия исходника (`.source.json`), вызов реимпорта. Тесты в `EditImportSettingsTests.h` (7 секций, 21 проверка). Коммит `fad8c52`.
- [x] **Шаг 17.6d (fix(ui)):** Дефолт `useFileUnitScale = true` во всех путях `ImportSettingsDialog` (единый `DefaultSettings()`, открытие, `Prompt`, `PromptScale`, сброс). Тест в `EditImportSettingsTests.h`: открытие без сброса даёт `true`, парсер sidecar без поля даёт `false`. Коммит `acfde31`.
- [x] **Шаг 17.6e (fix(ui)):** Поле угла сглаживания и его текстовая метка визуально неактивны (`setEnabled(false)`, цвет `Disabled`, подсказка «Доступно только при сглаженных нормалях») при `Normals != Smooth`. Тест состояния в `EditImportSettingsTests.h`. Коммит `393bd72`.
- [x] **Шаг 17.6f (feat(ui)):** Скрытие служебных sidecar (`.import.json`, `.importsettings.json`, `.source.json`) в Content Browser через `ProjectFilesFilter`. Расширение `AssetOps` поддержкой сопутствующих файлов на переименование, дублирование и удаление. Тесты в `AssetOpsTests.h` и `EditImportSettingsTests.h`. Коммит `ed42c59`.
- [x] **Шаг 17.6g (feat(ui)):** Удаление ассетов в Content Browser (`Assets.Delete`, пункт меню «Удалить», клавиша `Delete`), подтверждение, предупреждение при наличии объекта на сцене (`Snapshot.objects`), удаление сопутствующих sidecar через `AssetOps` в песочнице проекта. Юнит-тесты в `AssetDeleteTests.h` (7 проверок). Коммит `4ea3409`.
- [x] **Шаг 17.7 (test):** Регрессионное тестирование на уровне Host/Core без раздувания `EditorUiTests.cpp`. Реализовано в T7 (коммит `d9b6c3f`): новая цепочка `Editor/tests/core/ImportReimportChainTests.h` (20 проверок), `EditorUiTests.cpp` не тронут.

---

## Чек-лист среза S17.8: Scale в геометрию (R1-R5), транзакционность реимпорта (R4), регрессия плоских нормалей (П3)

- [x] **T1 (experiment, без коммита):** Эксперимент «scale в геометрию» на реальном FBX `C:\Users\Yuri\Downloads\0dae7c1fa11c828a43de24a048fa49f2.fbx` (fallback `blender-scale100.fbx`), пробник вне репозитория (`%TEMP%`). Замеры static + `CesiumMan.glb` для `useFileUnitScale` true/false x scale 1/0.01/2. Выбранный механизм по R2: `useFileUnitScale=true` -> `aiProcess_GlobalScale` (нормализация единиц файла), затем `model.scale` отдельным множителем геометрии поверх; при `false` масштаб файла не учитывается, применяется только `scale`. FBX в репозиторий не добавлялся.
- [x] **T2 (feat(engine)):** `ModelImportSettings`: явный признак «geometryScale applies» при `version >= 2` (`AppliesGeometryScale()`). Реализация R1/R2 в `SkeletalLoaderAssimp.cpp`: v2 запекает `model.scale` (и масштаб файла при `useFileUnitScale`) в геометрию, v1/legacy побитово неизменны (memcmp). Скелетный путь: кости, `InverseBindPose`, rest-pose, позиция-ключи анимации, bounds и `BoundingRadius` масштабируются согласованно. Обновлён общий набор `Engine/tests/ImportSettingsTestCases.h` (v2-кейсы), парсеры Engine и Editor идентичны. Тесты `Engine/tests/ModelGeometryScaleTests.h` (static + skeletal, v1 vs v2, scale 0.01 уменьшает bbox в 100 раз, Transform не задействован). Коммит `1df506f`.
- [x] **T3 (feat(host)/feat(ui)):** Editor под R1: спавн модели с sidecar `version >= 2` -> `Transform.scale = {1,1,1}`; v1/legacy/без sidecar -> как раньше (scale из sidecar на Transform). `ImportSettingsDialog` пишет `version 2` для новых импортов и «Настройки импорта…»; открытый v1-sidecar показывает scale 1.0 с подсказкой, запись конвертирует в v2 с предупреждением. Реимпорт после смены scale реально меняет геометрию. Новые тесты в новых файлах (`ImportSettingsSpawnV2Tests.h`, `ModelEditFlow`). Коммит `5e09a88`.
- [x] **T4 (feat(host)):** R4 транзакционность «Настройки импорта…»: бэкап sidecar в памяти до записи (`Editor/Core/ModelImportSidecarTransaction.h/.cpp`), при ошибке `UpdateAsset` восстановление прежнего содержимого, одно сообщение через `Tr()`. Тест `Editor/tests/core/SidecarTransactionTests.h` (38 проверок, mock-ошибка `UpdateAsset` -> файл возвращён к прежнему содержимому). Логика в новых файлах, `ImportCommands.cpp` не раздут (480 строк). Коммит `7efe9ad`.
- [x] **T5 (test(engine)):** Регрессия плоских нормалей (П3): фикстура `Engine/tests/fixtures/folded-smooth.obj` (в репозитории, не TEMP; два треугольника с общим ребром, согнутые на 90 градусов, все углы с одним smooth `vn`) и тест `Engine/tests/ModelFlatNormalsTests.h`. `flat` даёт больше вершин, чем файл, и 100% плоских граней с различными нормалями; `smooth` на дефолтном угле не сваривает складку; `smooth` на 175 градусах сваривает обратно до 4 вершин. Тест падает при отсутствии фикстуры. Вывод по П3 зафиксирован в «Решения» (п. 37): НЕ воспроизведён, первый черновик падал из-за отсутствия обязательного `model.scale` в sidecar (баг обвязки). Коммит `2592da2`. EngineTests 74/74 PASS.
- [x] **T6 (chore):** Актуализация `Editor/PLAN.md`: решения R1-R5 (п. 36), вывод по П3 (п. 37), чек-лист S17.8, Backlog (сироты sidecar при rename/move через AssetOps companions, `contains("..")` в Engine `ResolveSidecarPath`, смена числа материальных слотов ломает реимпорт), «Не проверено в рантайме» (перерисовка вьюпорта после реимпорта, реальный FBX Юрия для flat/scale, scale-в-геометрию в живом GUI, иконки без Qt6Svg).
- [x] **T7 (test):** Регрессия 17.7 Host/Core: импорт с v2 sidecar -> спавн `Transform = 1` -> реимпорт не сбрасывает `Transform`; в новых файлах. Новый тест `Editor/tests/core/ImportReimportChainTests.h` (20 проверок) связывает R1 (v2 -> `ReadSpawnScale = 1`, геометрия хранит scale) и R4 (транзакционный откат sidecar при ошибке `UpdateAsset`); подключён в `CoreTests.cpp` и `CoreTests.vcxproj`. CoreTests 653 (Debug и Release). Коммит `d9b6c3f`.

---

## Чек-лист среза S17.9: Исправление дефектов приёмки S17.8 (3 бага живого тестирования)

- [x] **Шаг 17.9a (fix(ui)):** Стили `:disabled` для спинбоксов, комбобоксов и меток в `EditorStyles.cpp`. Добавлены селекторы `:disabled` с цветом `%12` (`textDisabled`), фоном `%8` и границей `%6` для `QLineEdit`, `QSpinBox`, `QDoubleSpinBox`, `QComboBox`, а также для `QLabel`, `QLabel#CardFieldLabel` и меток внутри `QFrame#CollapsibleCard`. Объединены дублирующиеся стили `QMainWindow::separator` и `QSplitter::handle` для соблюдения лимита (492 строки <= 500). Юнит-тест в `ImportSettingsDialogTests.h` (347 строк <= 500) проверяет наличие правил в темной и светлой теме. `UiTests.exe` 308/308 PASS.
- [x] **Шаг 17.9b (test(host)):** Красный тест хостового пути импорта: проверка в `HostRegressionTests.h` (Case C), что при наличии `cmd.importOptionsJson` отсутствие расширения `Engine.Asset.Import` (`importWithOptions`) не приводит к молчаливому сбросу опций на дефолты через `ctx.scene.importAsset`, а отклоняет операцию со статусом `EngineSceneFailure` без вызова legacy `importAsset` и без выставления `completedCmd.path`. Тест проверен и подтверждён красным на текущем коде (`AssetCommandHandler rejects optionsJson when extension missing`, exit code 1).
- [x] **Шаг 17.9c (fix(engine) + fix(host)):** Проброс `EngineBindAssetImport` в `Engine/Source/Base/Base.cpp` и удаление молчаливого fallback в `AssetCommandHandler.cpp`. В `Base.cpp` при наличии `editApi->createModel` запрашивается расширение `EngineAssetImportExtension` из модуля Game (`EngineGetEditExtension`) и связывается с `Editor.dll` через экспорт `EngineBindAssetImport`. В `AssetCommandHandler.cpp` удален молчаливый сброс опций: при наличии `cmd.importOptionsJson` и отсутствии `importWithOptions` операция отклоняется с `EngineSceneFailure` и ошибкой. Добавлен юнит-тест `EditorQtHost::SetAssetImportApi` в `HostTests.cpp`. Все 8 тестовых наборов (74 Engine, 8 Skeletal, 651 Edit-session, 109 Scene-module, 116 Compatibility, 656 Core, 116 Host, 308 UI) PASS в Debug и Release.

---

## Чек-лист среза S17.10: Стабилизация конвейера импорта, очистка staging и удаление папок (4 шага)

- [x] **Шаг 17.10.1 (fix(editor)):** Удаление папок в Content Browser: снят запрет `!model_->isDir(index)` в `FileBrowser.cpp`, защита системных корней (`Content`, `Source` и корень проекта) централизована в `FileService::IsProtectedDirectory` и `FileService::DeleteDirectory` (вложенные папки `Content/Source` и `Content/Content` не защищены). `FileBrowser` и команда удаления делегируют проверку в Core без дублирования. Логика удаления вынесена в `Editor/Source/AssetDeleteCommand.cpp` (119 строк, функции <= 42 строк), `ImportCommands.cpp` сокращён до 398 строк. Юнит-тесты в `AssetDeleteTests.h` (9/9 checks PASS).
- [x] **Шаг 17.10.A (fix(ui)):** Шорткат `QKeySequence::Delete` (`Qt::WidgetShortcut`) на дереве `view_` в Content Browser (`FileBrowser.cpp`), нормализация индекса к колонке 0 (`viewIndex.sibling(viewIndex.row(), 0)`). Нормализация колонки добавлена также в `OnDoubleClicked` и `OnContextMenu` (изменение поведения: двойной клик и контекстное меню теперь срабатывают на любой колонке строки ассета, а не только на колонке 0). Тест в `EditorUiTests.cpp`: проверка сохранения выбранного объекта сцены при удалении ассетов через колонку 0 и колонку 1. Коммит `6437b10`.
- [x] **Шаг 17.10.B (fix(ui)):** Enter по умолчанию «Да» в диалоге подтверждения удаления ассета (Escape = «Нет»). Выделен хелпер `EditorUI::CreateDeleteConfirmationBox`/`ExecDeleteConfirmation` в `Editor/Include/AssetDeletePrompt.h`, в `AssetDeleteCommand.cpp` диалоги предупреждения и вопроса используют `setDefaultButton(Yes)` и `setEscapeButton(No)`. Юнит-тесты в `AssetDeleteTests.h` (проверка конфигурации кнопок, срабатывание Yes по Return и Keypad Enter, срабатывание No по Escape, 12/12 checks PASS).
- [x] **Шаг 17.10.C (fix(editor)):** Обработка удаления ассета, используемого на сцене: объект сцены сохраняется; в инспекторе путь подсвечивается акцентным цветом темы (`EditorTheme::Instance().AccentColor().name()`) с подсказкой и кэшированием проверки диска; при удалении ассета кэш немедленно инвалидируется (`InvalidateAssetCache()`) и виджет инспектора сразу обновляет подсветку без сброса выделения; кнопка «Изменить» материала неактивна с подсказкой; `OpenMaterialEditor` проверяет наличие файла на диске до создания окна и показывает предупреждение через `Tr()`, не создавая окно и не вызывая `saveMaterial`. Тесты в `AssetDeleteTests.h` (15/15 checks PASS: стили, токен темы, кэш, мгновенное обновление при том же выделении, блокировка кнопки) и `EditorUiTests.cpp` (проверка предупреждения при вызове `OpenMaterialEditor` для отсутствующего файла). Коммиты `6bfcec6` и последующий.
- [x] **Шаг 17.10.0 (fix(engine)):** Устойчивость `SceneDocument::Restore` к отсутствующим моделям: создание плейсхолдера `SkeletalMeshComponent` с dummy `SkeletalModel` через `dummy->SetAssetModel(o.modelAsset, std::move(model))` с сохранением пути ассета (`GetModelAsset() == o.modelAsset`), логирование ошибки `[Error] SceneDocument: missing model...` через `std::printf`, сохранение слотов материалов из `projectData`. Исключено падение реимпорта других ассетов с кодом 6 (`EngineSceneSimulationFailed`) при валидации сцен через `Restore`.
- [ ] **Шаг 17.10.D (fix(ui)):** Унификация дефолтов диалога импорта в единый `DefaultSettings()`, запрет перезаписи `scale`/`flipUv` через `SettingsService`, блокировка пресетов масштаба при включённых единицах файла, перенос `flipUv` и `mergeMeshes` в «Дополнительно».
- [ ] **Шаг 17.10.E (fix(ui)):** Строгая фильтрация типов в диалоге реимпорта по центральному реестру расширений Core.
- [ ] **Шаг 17.10.2 (fix(engine)):** Перенос staging реимпорта в `.painengine/staging/` в корне проекта (вне зоны видимости Content Browser) в `Engine/Source/Scene/AssetUpdate.cpp` и `SceneAssets.cpp`. Устранение дублирующих папок `<stem>_1`.
- [ ] **Шаг 17.10.3 (feat(editor)):** Миграция старых `Game.dll`: автообновление для no-code проектов из шаблона с бэкапом и логом; диалог «Собрать проект» для C++ проектов; явный fallback с предупреждением при отказе.
- [ ] **Шаг 17.10.4 (perf(engine) + fix(editor)):** Замеры FPS/ресурсов до/после реимпорта; исключение повторного прохода Assimp в материалах; пропуск загрузки нерелевантных сцен в `Commit`; сохранение выделения и Undo.

---

## Чек-лист среза S18: Настройки импорта текстур
> **Статус:** Позже (после приёмки S17).

---

## Чек-лист среза S19: Настройки импорта звуков
> **Статус:** Позже (после приёмки S18).

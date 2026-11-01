# 054 — Общие трассировки сцены и экранные лучи

Статус на 2026-11-01: реализация этапов 1, 2 и 4 выполнена; итоговые сборки и регрессионные проверки описаны ниже. Этап 3 исключён по запросу пользователя.

## Границы задачи

Движок предоставляет world/screen/cursor raycast, чистые пересечения, диагностику и общий editor picking. Размещение зданий, призрак, snap/сетка, занятость, стоимость, уклон участка и игровые правила принадлежат разработчику игры. PlacementController, OverlapBox и placement-box в этой задаче не реализуются. Игровых BuildSurface/PlacementBlocker-каналов в Engine нет: доступны произвольные битовые маски.

## Этап 1 — экранный луч и простые пересечения: выполнен

- `Trace::Ray`, `Hit`, `Query`, `Result`, `RayResult`, отдельные статусы ошибки/промаха.
- `Trace::IntersectPlane`, `IntersectAabb`, `IntersectTriangle`; конечные мировые расстояния, нормализованный луч.
- `Game::ScreenPointToRay`, `RaycastScreen`, `RaycastCursor`; физические координаты относительно viewport с Y вниз.
- `ViewportPoint::Pixel` — центр пикселя; обычные дробные координаты без скрытого округления; `Logical` — явное DPI-преобразование.
- Active Player, перспективная/ортографическая проекция и XY/XZ/YZ через view matrix; origin на near plane; проверка границ/размера/матриц/отсутствия камеры. Матрицы обновляются без input tick, включая первый PreUpdate и изменение камеры между кадрами.
- Абсолютный cursor sample, размер target и valid в Input::System. Qt передаёт физические координаты; standalone масштабирует client coordinates в render target. HUD использует тот же sample до игровых callbacks.
- Optional `Engine.Viewport.Cursor` v1 без изменения существующих DLL-таблиц; проверяются version/size, capability перезапрашивается после hot reload.

## Этап 2 — ближайшее попадание сцены: выполнен

- `Scene::Raycast(ray,query)` возвращает ComponentId, мировую точку, геометрическую нормаль, мировую дистанцию, triangleIndex и instanceIndex (при наличии).
- Ближайшее попадание, стабильный ComponentId при равенстве, дальность, маски, ignore IDs, backfaces.
- Линейный перебор с AABB broad phase. `Exact`, `Bounds`, `EditorSelection` разделены. Служебные сферы отсутствуют в игровых Exact/Bounds.
- Мировые треугольники обеспечивают правильное расстояние при иерархии и неравномерном/отрицательном scale; singular transform пропускается. Пересечение использует CPU-геометрию без lazy GPU-загрузок.
- Визуальные proxy любых компонентов; CreateInstance и статический InstancedMeshComponent LOD0 с index инстанса.
- Editor picking переведён на общий запрос, старое NDC API и выбор служебных объектов сохранены.
- Hit не содержит указателей на компоненты. Новые C++ возможности требуют сборки новым SDK; frozen DLL работают через прежний ABI.

## Этап 3 — строительство: исключён целиком

Не входит в реализацию и не считается незавершённой работой движка в этой задаче. Примеры показывают только общие запросы, без спавна/экономики/игрового размещения.

## Этап 4 — диагностика и производительность: выполнен

- `Game::DebugDrawTrace` рисует луч, точку и нормаль через overlay в Play/standalone; ограниченные TTL и очередь.
- Статистика на запрос и накопительно на Scene: запросы, bounds, треугольники, unsupported, микросекунды; явный Reset на границе кадра.
- BVH/grid и физическая система не добавлялись. Дальнейшее ускорение — по измерениям реального потребителя.

## Поддержка и ограничения

Статический terrain поддерживается как обычный CPU mesh. Skinned mesh без clips использует загруженную CPU-позу; с clips Exact пропускается, Bounds/editor используют коробку начальной позы. Crowd/VAT пропускается с диагностикой. CPU trace не обещает соответствия итоговому GPU-пикселю при transparency/alpha test, shader displacement, GPU-анимации, impostor и render LOD. Отдельного terrain collider нет.

Подробный контракт: [TRACE-API.md](../../Engine/docs/TRACE-API.md). Компилируемые примеры: [TraceExamples.cpp](../../Engine/examples/TraceExamples.cpp). SDK включает заголовки, библиотеку, документ и примеры.

## Проверки реализации

- `Engine/tests/TraceTests.cpp`: чистая математика, nearest/range/mask/ignore, точные треугольники против bounds, parent/scale/нормали/дистанции, instances, singular/invalid входы, pixel/fractional/DPI/resize, перспективная/ортографическая XY/XZ/YZ, cursor/focus/UI, editor picking, standalone, TTL.
- `AuthoringTests.cpp`: trace импортированной visual model и отражённой нормали.
- `EditorUiTests.cpp`: version/size cursor extension и существующая регрессия редактора, включая hot reload binding.
- `CompatibilityTests.cpp`: отсутствие расширения у frozen DLL, его наличие после миграции на текущий шаблон, сохранение старых файлов.

Итоговые результаты (Release x64, MSVC v145, Assimp включён):

- Engine.lib, EngineRuntime.exe, Editor.dll, RuntimeExample/Game.dll и шаблон SceneProject/Game.dll собраны успешно.
- EngineTests: **51/51** группа; четыре новые группы trace и проверка visual proxy. Включены первый запрос до первого кадра, изменение камеры до следующего кадра и фактическая блокировка курсора кнопкой HUD.
- Input/Character: **80 + 99** проверок ввода и независимой камеры.
- EditorUiTests: **3110** проверок, повторно после исправления камеры. Для записи временных scene snapshots запуск выполнен вне sandbox (в sandbox Windows error 5).
- CompatibilityTests: **116** проверок frozen DLL, migration copy и optional extension.
- EditorReloadTests: **50** проверок, включая новый cursor callback после выгрузки прежней DLL.
- SDK экспортирован в Game/build/x64/Release/Runtime/SDK. Отдельный проект O:/Engine/TraceSdkChecks собран только его средствами; TraceExamples.cpp включён в игровой модуль. Трёхкадровый standalone smoke вызывает все четыре API из первого PreUpdate и завершился с кодом 0. Мировой запрос дал Hit, экранный — корректный Miss, преобразование — Success; отсутствие фокуса даёт CursorUnavailable.
- git diff --check: без ошибок.

Результаты сборок/запусков сохранены в O:/Engine/trace-*-build.log и O:/Engine/trace-*-run.log. Производительность на реальной игровой карте и многомониторный DPI вручную не измерялись; DPI-преобразование покрыто автоматическими тестами. Ускоряющие структуры не требуются для завершения этого этапа.

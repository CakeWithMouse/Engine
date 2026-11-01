# Трассировки сцены и экранные лучи

Публичные типы: `Engine/Scene/Trace.h`. Игровые функции доступны из `Game.h`, мировые запросы — из `Scene.h`. Один C++ API работает в editor Play и standalone. Физика и правила размещения объектов в него не входят.

## Обычное использование

```cpp
#include <Engine/MainGame/BaseGameClass/Game.h>

void QueryExample(Game& game) {
    Trace::Query query;
    query.maxDistance = 500; // мировые единицы
    auto world = game.GetScene().Raycast({{0, 10, 0}, {0, -1, 0}, 500}, query);
    auto pixel = game.RaycastScreen(Trace::ViewportPoint::Pixel(120, 80), query);
    auto cursor = game.RaycastCursor(query);
    auto ray = game.ScreenPointToRay({120.25f, 80.75f}, query.maxDistance);
    if (pixel) {
        const auto& hit = *pixel.hit;
        // hit.componentId, position, normal, distance, triangleIndex, instanceIndex
        // Разрешать ID через Scene::FindComponent только в момент использования.
    }
    if (ray) game.DebugDrawTrace(ray.ray, game.GetScene().Raycast(ray.ray, query), 2.0f);
}
```

Четыре отдельные компилируемые функции: [TraceExamples.cpp](../examples/TraceExamples.cpp), включены в EngineTests. Заголовки, библиотека, этот документ и примеры поставляются в SDK.

`Result` содержит `status`, `optional<Hit>` и счётчики запроса. `bool(result)` означает попадание. `Miss` — корректный луч без попадания. `InvalidRay`, `InvalidQuery`, `InvalidViewport`, `OutsideViewport`, `InvalidMatrix`, `NoCamera`, `CursorUnavailable`, `InputBlocked` — разные причины отсутствия результата. `RayResult` успешен при `Success`. `Trace::StatusName` возвращает имя статуса.

## Координаты и камера

- `ViewportPoint{x,y}` — непрерывные **пиксельные координаты render target игрового viewport**, начало в левом верхнем углу; X вправо, Y вниз. Координаты всего окна сюда не передаются.
- `Pixel(i,j)` проходит через центр конкретного пикселя: `{i+0.5,j+0.5}`. Обычные дробные координаты сохраняются без округления и без добавления половины пикселя.
- Допустимая область: `0 <= x < width`, `0 <= y < height`. Точка `(width,height)` находится снаружи. Геометрический центр — `(width/2,height/2)`; при чётном размере это граница центральных пикселей.
- `ViewportPoint::Logical(x,y,pixelsPerUnit)` явно переводит логические UI-координаты в физические пиксели; сначала нужно вычесть UI-положение viewport. Значение масштаба получает вызывающий код от UI, а не угадывает движок.
- Qt передаёт позицию относительно самого viewport, умноженную на `devicePixelRatioF()`. Размер target округляется так же, как при создании viewport. События мыши сохраняют дробную точность; текущая позиция также обновляется таймером после resize, смены DPI и Play/resume.
- Standalone переводит позицию Win32 client в размер render target через отношения размеров по X/Y. Нативная целочисленная позиция относится к центру её пикселя; учтена DPI-виртуализация процесса.
- Используются матрицы активного `Player` и текущий размер Display. Перед запросом матрицы обновляются без обработки ввода: запрос работает и до первого кадра, и сразу после изменения камеры. Луч начинается на near plane, его направление единичное. При ортографии начала различаются, направления параллельны. XY/XZ/YZ поддерживаются направлением активной камеры, дополнительных ручных преобразований не требуется.
- `distance` измеряется от начала луча, включая экранный луч с near plane. Дальность задаётся запросом и лучом; применяется меньшая. Far clip не является дальностью трассировки. Направление мирового луча нормализуется внутри API.
- Нулевой/свёрнутый viewport и ожидающий применения resize дают `InvalidViewport`. После кадра с новым размером нужны координаты этого размера; старый образец курсора отвергается. Матрицы должны быть конечными и обратимыми. Нельзя использовать `Game` до создания камеры; это `NoCamera`.

Чистая `Trace::ScreenPointToRay` доступна для тестов/инструментов с собственными матрицами: принимает сохранённые shader-transposed матрицы в соглашении `Player`, D3D depth 0..1. Игровому коду достаточно методов `Game`.

## Геометрия и фильтры

`Scene::Raycast(ray,query)` возвращает ближайшее попадание. На равном расстоянии выбирается меньший ComponentId, затем первый треугольник/инстанс. Сцена перебирается линейно; перед треугольниками проверяется мировой AABB из локальных bounds. Треугольники преобразуются в мир, поэтому расстояние не искажается неравномерным scale. Нормаль — единичная геометрическая нормаль по порядку вершин; отражение меняет её знак. Это не интерполированная shading normal. Вырожденные/нечисловые трансформации пропускаются со счётчиком `unsupported`.

| Параметр | Поведение |
|---|---|
| `geometry = Exact` | По умолчанию: точные CPU-треугольники, без служебных сфер |
| `geometry = Bounds` | Пересечение мировой осевой коробки геометрии; `triangleIndex = NoTriangle` |
| `geometry = EditorSelection` | Треугольники + сферы объектов без геометрии; animated mesh выбирается по bounds начальной позы |
| `backfaces = Include / Cull` | Двусторонние треугольники / только грань, чья нормаль смотрит навстречу лучу; для bounds/sphere не применяется |
| `channels` | Пересечение битов с `GameComponent::traceChannels`; нулевая маска не попадает никуда |
| `ignore` | ComponentId, исключённые из запроса, включая все их инстансы |
| `maxDistance` | Конечная неотрицательная мировая дальность, граница включается |

`traceChannels` по умолчанию `DefaultChannel` (бит 0); запрос по умолчанию проверяет `AllChannels`. Значение на компоненте — runtime-настройка. Если игре нужно сохранять его или менять в свойствах, она объявляет собственное свойство и устанавливает маску при инициализации. Специфичных каналов строительства в движке нет. Видимость и каналы независимы; skybox и screen-space UI всегда исключаются. Отложенно удаляемые объекты исключаются немедленно; отложенное добавление появляется после завершения update scope.

| Источник | Поддержка |
|---|---|
| Примитивы, MeshComponent, назначенные примитивы и статические mesh-ассеты | Точные треугольники и bounds; сюда относится terrain, представленный такой геометрией |
| Визуальная модель любого GameComponent | Используется существующий visual proxy, его иерархия и model yaw; наружу возвращается ID владельца |
| GameComponent::CreateInstance | Геометрия источника в матрице каждого инстанса; ID источника + `instanceIndex` |
| Статический InstancedMeshComponent | LOD0 и мировые матрицы каждого инстанса; камера/LOD/видимость не фильтруют запрос |
| SkeletalMeshComponent без animation clips | Точные CPU-треугольники загруженной позы |
| SkeletalMeshComponent с animation clips | `Exact` пропускает; `Bounds`/editor используют AABB начальной позы, не текущей анимации |
| GPU crowd/VAT instancing | Пропускается, `unsupported` увеличивается; игровой код может использовать отдельный обычный mesh как collision-форму |
| Камеры/свет/компоненты без геометрии | Только служебный режим editor selection |

`unsupported` — диагностический счётчик пропущенных кандидатов, а не обещание проверить GPU-поверхность. Нет совпадения с окончательным видимым пикселем для прозрачности, alpha test, shader displacement, GPU-анимации, billboard/impostor и выбора render LOD. Материалы не влияют на точность CPU-геометрии. Отдельного terrain collider и физической симуляции нет.

Чистые функции `IntersectPlane`, `IntersectAabb`, `IntersectTriangle` имеют те же мировые единицы. Их попадания имеют ComponentId 0 и `NoTriangle`, поскольку не привязаны к сцене. При старте внутри AABB возвращается выходная поверхность. Параллельность и вырожденный треугольник дают `Miss`; неверные параметры — ошибку.

## Курсор, UI и DLL

`Input::CursorPosition` хранит абсолютные x/y, размер target и valid. Потеря фокуса сбрасывает valid. `RaycastCursor` требует включённый контекст Game и отсутствие pointer capture у HUD/UI; иначе возвращает `InputBlocked`. Неизвестный курсор даёт `CursorUnavailable`, известная точка вне viewport — `OutsideViewport`. Подготовка UI и установка блокировки происходят до игровых обработчиков действий. `RaycastScreen` и `ScreenPointToRay` намеренно не зависят от фокуса/capture: это явные запросы точки, пригодные и инструментам.

Новый optional extension `Engine.Viewport.Cursor`, version 1, структура `EngineViewportCursorApi`, передаёт позицию из editor host в игровую DLL. Поле `set(context,x,y,width,height,valid)` вызывается на потоке сессии. Версия и размер проверяются до записи output; при валидном образце размеры должны совпадать с viewport. Host service `EngineViewportCursorService` запрашивается заново после hot reload. Существующие таблицы `EngineEditSessionApi`, `EngineViewportInputApi`, `EngineModuleHost` и reload bindings не менялись.

Старые DLL продолжают загружаться без расширения. Для нового игрового C++ API проект пересобирается новым SDK; ABI C++ класса Game не переносится через границу старой DLL. Новая DLL со старым host без cursor extension не получает абсолютный cursor sample; явный `RaycastScreen` остаётся доступен, `RaycastCursor` не придумывает позицию. Editor picking использует общий запрос `EditorSelection`, сохраняя старое NDC-соглашение своего ABI и ID 0 при промахе.

## Отладка и измерения

`game.DebugDrawTrace(ray,result,seconds)` рисует луч, крест в точке и нормаль поверх сцены в редакторе и standalone. Время жизни — реальное монотонное время, включая паузу, максимум 3600 секунд; максимум 4096 линий. `ClearDebugTraces()` очищает, `GetDebugTraceLineCount()` возвращает число живых линий. Это ограниченная CPU-очередь существующего overlay renderer, без указателей на объекты.

`result.statistics` содержит число запросов, bounds, треугольников, unsupported и время CPU в микросекундах. `scene.GetTraceStatistics()` — накопительные значения; вызывайте `ResetTraceStatistics()` на нужной границе кадра. Некорректные экранные запросы, не дошедшие до Scene, не увеличивают её статистику. Запросы и сброс выполняются на игровом потоке, как остальные операции Scene. Ускоряющей структуры нет; её следует добавлять после замеров конкретной сцены.

## Проверки

`EngineTests` компилирует примеры и проверяет math, nearest/filter/range, отрицательный/неравномерный scale и parent, модели/proxy, instances, animated ограничения, проекции XY/XZ/YZ, пиксели/дроби/DPI, resize, курсор/UI/focus, editor picking, standalone и TTL. `EditorUiTests` проверяет подключение versioned extension и существующие UI-сценарии. `CompatibilityTests` загружает frozen DLL, выполняет миграцию копии проекта и проверяет отсутствие/наличие cursor capability без изменения старых файлов.


Проверено 2026-11-01, Release x64: EngineTests 51/51, Input/Character 80+99, EditorUiTests 3110, CompatibilityTests 116, EditorReloadTests 50. SDK-проект вне репозитория компилирует примеры и вызывает API в первом PreUpdate standalone. Числовые преобразования DPI покрыты тестами; ручной перенос окна между мониторами с разным DPI не проверялся.

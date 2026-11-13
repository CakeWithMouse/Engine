# Прозрачные слои, камера и LOD — 2026-11-07

Выпуск 2026.11.07.5, Windows x64 Release, MSVC v143, D3D11, NVIDIA GeForce RTX 4070 Laptop.
Продолжение проверки [материалов .4](../2026-11-07-materials/report.md).
Исторические правки, журналы и архивы сохранены. Новые игровые проекты не изменялись:
GPU-сцены создаются в hidden HWND, проверки проекта — в scratch-каталогах.

## Воспроизведение и измерения

Новый `render.transparency` добавлен в существующий EngineCommandlets. Он строит
обычные MeshComponent, CreateInstance, InstancedMeshComponent, skeletal и VAT-плоскости;
назначает настоящие `.mat` и вызывает штатный RenderViewportFrame. JSON принимает
слои, группы, цвета/alpha, камеру, позу и LOD; сохраняет BMP, draw calls и LOD counts.
Никакая специальная сортировка или формула смешивания в сцене-командлете не используется.

`Editor/tests/TransparencyTests.py` независимо вычисляет source-over/additive цвет
и проверяет 256 пикселей в центре каждого кадра с допуском 3/255 на округление UNORM.
Для невидимых слоёв используется ray/plane-проверка покрытия; opaque occlusion учитывается
отдельно. Полная карта функций и проверок: [fixes/coverage.csv](fixes/coverage.csv).

```powershell
Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -EnableAssimp -EngineCommandlet
python Editor/tests/TransparencyTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/transparency-new
```

После правок HLSL нужен полный `Engine/build.ps1 -Target All -Configuration Release`
для развёртывания EngineContent рядом с runtime: сборка одного commandlet не копирует
shader content. В финальном прогоне используются командлет и шейдеры самого дистрибутива.

## Подтверждённые дефекты

| ID | До | Исправление | Проверка |
|---|---|---|---|
| TR-001 | Сортировка по квадрату расстояния меняла порядок параллельных плоскостей при pan | Глубина вдоль forward камеры | pan/orbit для mesh, copies, instances, skeletal, crowd в Forward/Deferred |
| TR-002 | Копии рисовались в исходном порядке; LOD и разные батчи не перемежались с другими объектами | Общая очередь по отдельным экземплярам и LOD; совместимые соседние диапазоны объединяются | interleave, lod-order, moving-layers, opaque-occlusion, equal-depth |
| TR-003 | clip(0) оставлял обе LOD-маски на равном Bayer-пороге; полный вес интерполировался в значение чуть меньше 1 при повороте | Непересекающаяся граница масок, точный full weight, nointerpolation для веса экземпляра | Все 17 порогов, повтор 0.5, веса 0.0001/0.9999, dolly и orbit |
| TR-004 | Прозрачный mesh переходил в atlas с coverage вместо alpha, менялись непрозрачность и цвет | Translucent/Additive сохраняют последний mesh LOD; opaque/masked atlas остаётся доступным | transparent/additive impostor, прежний EngineTests atlas regression |

TR-001 пример: красный ближний и синий дальний слой alpha=0.5 должны давать примерно
RGB (128,0,76). После смещения камеры старый код давал (64,0,140), то есть менял слои местами.
TR-003 пример: часть пикселей вместо RGB (128,0,26) становилась (191,0,13) из-за двойной alpha.

Исходная матрица: **6/46 пройдено, 402 кадра** (`before/summary.json`). Это 40 отказавших
сценариев, а не 40 независимых дефектов. После исправлений основная матрица дала
**46/46, 410 кадров**; добавлены крайние веса перехода. Финальный набор расширен до
52 сценариев / 428 кадров проверками роста stream 2 → 300 → 2 и равной глубины.

## Детали исправления диапазонов

StartInstanceLocation не смещал SV_InstanceID в используемом structured-buffer пути:
первая попытка диапазонной отрисовки повторяла первый экземпляр. Вместо нового shader
ABI диапазон задаётся SRV.FirstElement; представления создаются лениво и переиспользуются.
При росте GPU-буфера кеш диапазонов очищается. Оба пути — CreateInstance и mesh/VAT LOD —
проверены на GPU. Прозрачный проход сбрасывает диапазон после draw; opaque/shadow пути
сохраняют обычные полные батчи. Устранена также лишняя удержанная ссылка на D3D device
при каждом вызове InstancedMeshComponent::Render.

В промежуточных `after`/`after-ranges` сохранены диагностические отказы реализации.
Первый runner после C++-фикса ещё использовал старый развёрнутый Instancing.hlsli;
`after-flat` и финальный packaged runner используют исправленные HLSL. Эти промежуточные
артефакты не выдаются за верификацию финального выпуска.

## Визуальные свидетельства

- [Порядок при pan до](evidence/pan-before.png) → [после](evidence/pan-after.png).
- [Двойная alpha при LOD до](evidence/fade-before.png) → [после](evidence/fade-after.png).
- Все JSON-запросы, ответы, BMP и журналы: `Engine/build/playtests/20261107-transparency/`.

## Проверки и сборка

- EngineTests: 70/70, 0 skipped, 0 failed.
- API reference: 39 страниц, 308 API entries, 97 заголовков; snapshot `cf45030d8eeb`.
- Финальные packaged GPU-наборы: transparency **52/52, 428 кадров**; materials
  **48/48, 288 кадров**; две теневые лампы/LOD **22/22, 366 кадров**; движение
  точечных теней **34/34, 282 кадра**. Всего **156 сценариев, 1364 кадра**.
- Frozen binary compatibility: **116 проверок**. Дистрибутив: **12 запусков** —
  шесть игр, два редактора, создание/компиляция C++ проекта и его game/editor.
- SHA-256 SDK Engine.lib, EngineCommandlets.exe, заголовка GameComponents.h,
  четырёх изменённых shader-файлов и документации совпадают с итоговой сборкой.
  `git diff --check` и API reference `--check` прошли.
- Архив: `Engine/build/releases/PainEngine-2026.11.07.5-win64.zip`, **102 899 760 байт**.
  SHA-256: `04b13a8a4f52aaf37c56119f88fdd1af1f111e4ee4787a0d98399826a27c5834`.
  CRC пройден; manifest проверил **982 файла**. Метаданные — `package-verification.json`.
- UI не изменялся; новый полный UI-прогон не заявляется. Материальный UI/импорт проверены
  в предыдущем выпуске, здесь повторяются подходящие GPU/runtime и release-проверки.

## Границы

Ключ сортировки — мировой origin объекта/экземпляра, не отдельные треугольники.
Пересекающиеся меши, циклические перекрытия и сильный сдвиг геометрии анимацией относительно
origin требуют разбиения контента либо другого алгоритма; OIT в этом выпуске не добавлялся.
Разные силуэты соседних LOD могут показывать dither; distance LOD без fade дискретен.
Для прозрачных экземпляров корректный порядок может увеличить draw calls и число cached
SRV; это сознательная цена сортировки. Непрозрачные массовые батчи не дробятся.
В контрольной сцене из трёх экземпляров получены 3 draw calls спереди и 1 с обратной
стороны, где нужный порядок совпал с непрерывным диапазоном; рост stream дал 2 → 299 → 2.
Это показатели конкретной fixture, не универсальный performance benchmark.
Transparent impostor не реализован: используется последний mesh LOD, поэтому дальняя
прозрачная геометрия может быть дороже. C++-проекты нужно пересобрать с новым SDK;
форматы сцены и версии Module/Edit API не менялись.

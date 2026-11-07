# Проверка материалов — 2026-11-07

Проверяется выпуск 2026.11.07.4, Windows x64 Release, D3D11, NVIDIA RTX 4070 Laptop.
Журналы и кадры: `Engine/build/playtests/20261107-materials/`.
Старые отчёты и архивы сохранены. Проверка автоматизированная, с осмотром захваченных
кадров; новые независимые прогоны четырёх персон в этот запрос не входили.

## Карта покрытия

| Функция | Вызов / проверка | Ожидаемый результат | Доказательство |
|---|---|---|---|
| HLSL и параметры float–float4 | EngineTests | Reflection/defaults, редактирование, общие ресурсы и reload | `engine-fixed.log` |
| Ошибки вариантов | `MaterialFailedVariantPreservesBindings` | Шахматка, последние значения/текстуры сохраняются, исправление восстанавливает материал | `regression-before.log`, `engine-fixed.log` |
| Текстурные регистры | EngineTests, CoreTests, Qt UI | t31/t33/t127 дают правильные GPU-пиксели, t32 отклоняется; распределители обходят t32, не выходят за t127 | `core-before.log`, `core-fixed.log`, `ui-fixed.log` |
| .mat | EngineTests, AssetTests | Валидация, числовые пределы, неизвестные поля, относительные пути, перенос проекта, shared cache, reload | `numeric-before.log`, `engine-final.log`, `assets-expanded-fixed.log` |
| Редактор HLSL | `Editor/tests/run.ps1` | Открытие, параметры, текстуры, Save/Revert, диагностика ошибок, preview, read-only отказ без потери правок | `ui-fixed.log` и UI fixture |
| Граф | CoreTests, Qt UI, `MaterialGraphCommandletTests.ps1` | Ноды/связи, ошибки, normal map, Undo/Redo, сохранение, неудачный откат и повтор | `core-fixed.log`, `ui-fixed.log`, `graph-commandlet.log` |
| Назначение и сериализация | AssetTests, `MaterialSlotsCommandletTests.ps1` | Сцена/Play, submesh slots, общий override, Undo/Redo, duplicate, save/load | `assets-expanded-fixed.log`, `slots/` |
| Импорт | AssetTests + `PAINENGINE_BLEND_FIXTURES` | FBX/GLB/BLEND, внешние/встроенные текстуры, UV-углы, несколько материалов, откат при потерянной текстуре | `assets-expanded-fixed.log` |
| Обычный меш, инстансы, скелет, VAT | EngineTests, MaterialRenderTests | Цвет/параметры и тени соответствуют экземпляру и текущей позе | `engine-fixed.log`, GPU summary |
| Forward/Deferred, lit/unlit | MaterialRenderTests | Верная маршрутизация G-buffer, видимый материал, две работающие теневые карты | GPU summary |
| Opaque/masked/translucent/additive | MaterialRenderTests | Alpha игнорируется/обрезается/смешивается по режиму; masked клипует тени, прозрачные их не отбрасывают | GPU summary |
| Normal map и отражённый scale | EngineTests + Qt UI | GPU-пиксели меняются по карте, зеркальная/неравномерная матрица сохраняет базис | `engine-fixed.log`, `ui-fixed.log` |
| Impostor | EngineTests | Atlas запекается через материал и обновляется при его изменении | `engine-fixed.log` |
| Статические тени импортированной геометрии | AssetTests | Ближние каскады детальны, дальний использует bake; yaw инвалидирует карту | `assets-expanded-fixed.log` |

## Подтверждённые исправления

1. **MAT-001 — потеря параметров при ошибке instanced-варианта.** Reflection Mesh
   записывал новую пустую таблицу до компиляции следующего варианта. Неудача оставляла
   материал без прежнего Tint/BaseMap. Регрессия до исправления: 65/66, падение на
   восстановлении Tint. Теперь Reflection применяется после успешной компиляции всех
   необходимых вариантов. Тест проверяет и назначенную текстуру, и исправление HLSL.
2. **MAT-002 — пересечение текстур материала с point-shadow t32.** Граф и HLSL-кнопка
   считали все регистры от t6 свободными. Runtime также принимал Texture2D на t32,
   когда unlit оптимизировал движковую cube texture. Распределители теперь обходят t32,
   ограничены 128 SRV, используют свободные промежутки, сообщают об исчерпании.
   Runtime явно отклоняет t32. Core-регрессия до исправления падала на SafeMap/t33.
3. **MAT-003 — числовые переполнения в .mat.** JSON `1e999` выбрасывал неотловленный
   `out_of_range`, `1e100`/`-1e100` превращались в бесконечные float-параметры,
   версия `4294967297` сужалась до поддерживаемой `1`. До исправления: 67/70,
   три воспроизведённых отказа. Загрузчик ловит JSON numeric overflow, проверяет
   конечность float и диапазон версии до сужения. Все случаи возвращают false с
   диагностикой, не меняя переданный прежний asset; после исправления 70/70.

Обычные параметры и текстуры, умноженные на InstanceData, проверены по пикселям:
гипотеза об их исчезновении при reflection не подтвердилась. Шейдеры VS/PS компилируются
раздельно; нулевое значение в Mesh VS не удаляет этот вход из общего PS.

## Уточнения тестов

- Базовый Asset workflow: 229 проверок, но без Blender-части. Для полного импорта
  подключены существующие fixtures `Engine/build/imported-model-probe/fixtures` и Blender 5.2.
- Старая проверка требовала удаления статики из всех каскадов. Это противоречило
  действующему документированному контракту `StaticCasterCascades=2`. Теперь тест
  раздельно читает slices, требует ближнюю геометрию и пустые дальние каскады,
  проверяет неизменность bake и последующую инвалидацию yaw. Рендер ради старого
  ожидания не изменялся.
- Первый GPU runner дал 12 ложных отказов additive: относительный порог цвета
  отбрасывал пастельные пиксели поверх светлого пола. Кадры показывали корректное
  сложение. Проверка использует абсолютную разницу каналов >10, вместе с изменением
  alpha, точным восстановлением кадра и проверкой неизменности depth.

## Результаты и выпуск

Базовый runtime: 64/64; итоговые runtime-регрессии: 70/70, без пропусков.
CoreTests: 374 проверки; Qt UI: 5159 проверок.
Расширенный AssetTests с Blender: 514 проверок. UV-углы читаются точно как красный,
зелёный, синий, жёлтый; тень затемняет 4761 pixel приёмника. Статика: 153759 занятых
texels в bake, 81171 в ближних каскадах, 0 в дальнем. Это измерения конкретной fixture.
Граф-командлет прошёл; импорт multi-material + 13 операций редактора прошли.
Совместимость frozen DLL: 116 проверок, повторено на окончательном runtime.
Финальный staging прошёл все 12 запусков
(6 игр, 2 редактора, создание C++ проекта, компиляция, game/editor).
SDK Engine.lib, EngineCommandlets.exe, MATERIALS.md в Docs и SDK/Docs сверены по SHA-256
с итоговой сборкой. Предыдущий внутренний staging сохранён в `package-before-numeric/`,
он не является выдаваемым архивом.
GPU финального дистрибутива: **48/48 сценариев, 288 кадров**, без отказов.
Forward/Deferred × mesh/skeletal/VAT × четыре blend mode × lit/unlit.
Исходный полный GPU-прогон с уточнённой метрикой также дал 48/48; повторный
выполнен после последнего изменения загрузчика `.mat` на новых бинарниках.

Готовый архив: `Engine/build/releases/PainEngine-2026.11.07.4-win64.zip`,
97 856 013 байт. SHA-256:
`c62499fc39568764cd3b62359835d00b6afafb67546a77ad2e819daef8429eb5`.
CRC пройден, manifest проверил 969 файлов; подробности в
`package-verification.json`. API reference `--check` и `git diff --check` прошли.
Архив содержит новый runner `Diagnostics/MaterialRenderTests.py`.

## Воспроизведение

Из корня checkout (PowerShell, MSVC + Qt):

```powershell
Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Engine/tests/EngineTests.vcxproj -Configuration Release -EnableAssimp
Engine/build/x64/Release/EngineTests.exe
Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/tests/CoreTests.vcxproj -Configuration Release
Game/build/x64/Release/Runtime/CoreTests.exe
Editor/tests/run.ps1 -Configuration Release -EnableAssimp
$env:PAINENGINE_BLEND_FIXTURES='E:/Engine/Engine/build/imported-model-probe/fixtures'
$env:PAINENGINE_BLENDER='C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'
Engine/tests/assets/run.ps1 -Configuration Release -EnableAssimp
python Editor/tests/MaterialRenderTests.py --tool Game/build/x64/Release/Runtime/EngineCommandlets.exe --output E:/scratch/materials-new
```

Runtime и `Engine/vcpkg_installed/x64-windows/bin` должны быть в PATH. Для тестов
использован отдельный TEMP/TMP под `Engine/build/playtests/20261107-materials`.
Если Blender fixtures отсутствуют, сначала создать их через существующий
`Engine/tests/assets/make-blend-fixtures.py`; без них расширенный импорт будет SKIP.
Точные JSON-запросы, ответы, BMP и журналы GPU лежат в `package-final-render/`.
Отобранные осмотренные кадры: [UV](evidence/uv-material.png),
[разные материалы по submesh](evidence/slots-mixed-deferred.png),
[masked](evidence/suzanne-masked.png), [GPU preview](evidence/editor-material-preview.png),
[граф](evidence/editor-material-graph.png).

## Границы

Это покрытие реализованных функций, а не доказательство корректности произвольного
пользовательского HLSL или всех сочетаний контента. PBR/metallic/roughness не добавлялись.
Таблица ресурсов Surface должна быть общей для вариантов; reflection берётся из Mesh
forward PS. Матрицы/массивы не являются редактируемыми параметрами. Прозрачные материалы
не отбрасывают тени; сортировка прозрачности остаётся на уровне объектов. Тангентная
база skeletal использует документированный derivative fallback.
Qt widget grab не захватывает native D3D HWND: его чёрная область в снимке графа
не означает чёрное превью; для проверки сохранён отдельный GPU `editor-material-preview.png`.

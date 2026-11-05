# Blender и импорт .blend — 2026.11.04.1

Установлен Blender 5.2.3 LTS в `C:/Program Files/Blender Foundation/Blender 5.2/blender.exe`. Использован официальный пакет BlenderFoundation.Blender через winget; SHA256 установщика сверён: `a103d4ece8d9acffd2a95c9d8860355b1f209977cc983b4a3d1e1cfb441c8605`. Источник: [Blender Download](https://www.blender.org/download/), [установщик 5.2.3](https://download.blender.org/release/Blender5.2/blender-5.2.3-windows-x64.msi). Пользователь подтвердил системный запрос UAC. Проверены версия и фоновый запуск.

Исправленный релиз PainEngine **2026.11.04.1 Release** установлен в `C:/Users/serge/AppData/Local/PainEngine/Engines/2026.11.04.1/Release`. Хеши установленного commandlet, Runtime, версии и ImportBlend.py совпадают с проверенной сборкой: [installed-identity.json](evidence/installed-identity.json).

## Что проверено

Самостоятельно созданные Blender fixtures: куб с Base Color изображением, внешняя текстура, упакованная текстура, отсутствующая текстура. Дополнительно экспортированы FBX с внешним и встроенным изображением. Исходники расположены в каталоге с кириллицей; проект также переносится в другой каталог.

| Проверка | Результат |
|---|---|
| Asset workflow с включённым Blender | **273/273**, Blender-сценарии не пропущены |
| Commandlet: OBJ, FBX, GLB, BLEND external/packed, corrupt FBX, missing BLEND image | **7/7** |
| Те же импорты из установленного релиза | **7/7**, автоматический поиск Blender без PAINENGINE_BLENDER |
| Остальные commandlet-регрессии | **25/25** |
| Финальная сборка и установка | Успешно, runner exit 0 |

BLEND преобразуется штатным фоновым Blender в GLB. Обе модели размещены в сцене: один mesh, 36 индексов, созданный материал. Проверены материалы после сохранения/переноса проекта и реальные зелёные пиксели D3D11 без magenta error checker. [Внешняя текстура](evidence/blend-external.bmp) и [упакованная текстура](evidence/blend-packed.bmp) осмотрены: одинаковая зелёная передняя грань куба на тёмном фоне. Кадры сняты с фронтальной камеры, поэтому куб выглядит квадратом.

При отсутствующей текстуре возвращается `Missing base-color texture`, exit 2; незавершённый каталог модели удаляется. Это ожидаемый отрицательный сценарий, не оставшийся дефект. Предыдущие ресурсы сохраняются.

## Найденное и исправленное

**BUG010 — ложная ошибка геометрии в asset.import для BLEND.** Production-импорт и рендер изначально работали, но команда диагностировала исходный `.blend` через Assimp после успешной конвертации. Результат содержал `placed: true`, `ok: true`, `memoryLoadError` и не содержал числа мешей/индексов. [До: 2 неуспешные проверки из 7](evidence/commands-before/results.json). Исправление в `Editor/tools/EngineCommandlets.cpp`: читать модель, реально созданную importAsset, с её расширением (GLB для BLEND). [После: 7/7](evidence/commands-final/results.json), [установленный релиз: 7/7](evidence/installed/results.json).

**Ошибка тестового скрипта:** ожидаемый exit 2 отрицательного сценария оставался в LASTEXITCODE после успешного набора и приводил к ложному отказу внешней команды. AssimpCommandletTests теперь явно сбрасывает LASTEXITCODE в 0 только после проверки всех результатов. Финальный полный runner повторён с проверкой кода завершения и завершился успешно.

Ошибок компиляции/линковки в этом прогоне не было. Blender выдаёт предупреждение об устаревании Material.use_nodes для будущего Blender 6.0; на проверенной 5.2.3 это предупреждение не мешает импорту. Поддержка Blender 6.0 не заявляется.

## Повторить

Из корня репозитория:

```powershell
& Engine/tests/assets/run-blend.ps1
# Или указать исполняемый файл явно:
& Engine/tests/assets/run-blend.ps1 -Blender 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'
```

Runner создаёт новые fixtures и отдельный output, запускает GPU/asset-проверки, собирает commandlet и выполняет 7 сценариев. Файлы исходных игровых проектов не используются. Для проверки установленного exe есть параметр `AssimpCommandletTests.ps1 -Commandlet <exe> -BlendFixtures <fixtures>`. Документация обновлена в `Engine/docs/COMMANDLETS.md` и `Engine/docs/PROJECT-ASSETS.md`.

Свидетельства до/после, fixture generator и сами fixtures сохранены в `evidence`. Проверен поддерживаемый базовый материал Base Color/UV0; процедурные ноды, сложные shader graphs и другие ранее неподдерживаемые возможности не добавлялись. Остаточных дефектов в проверенных сценариях нет.

Недельный счётчик аккаунта: 17% в начале, 18% перед финальной фазой, ниже ограничения 30%. Это общий округлённый счётчик аккаунта, а не точный расход чата. Сбросы не использовались.

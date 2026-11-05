# Assimp: установка и проверка 2026.11.04.0

Assimp 6.0.4#3 (x64-windows, Debug/Release) установлен через закреплённый manifest `Engine/vcpkg.json`. Релиз Release 2026.11.04.0 собран с импортёром и установлен в `C:/Users/serge/AppData/Local/PainEngine/Engines/2026.11.04.0/Release`. Runtime содержит `assimp-vc143-mt.dll`, SDK — соответствующую import library. Старые установленные релизы сохранены.

## Причина и исправления

- В прежнем прогоне импортёр был отключён, пакет отсутствовал. После установки одной зависимости недостаточно: Engine, шаблон Game.dll, Runtime и SDK пересобраны с Assimp.
- `Invoke-MSBuild.ps1` раньше передавал `EngineEnableAssimp=false`, когда переключатель не указан. Теперь учитывает установленный пакет. Исправлена передача необязательного переключателя в runners assets/skeletal/input/subsystems. Явное отключение остаётся доступным. Финальные сборки launcher и commandlet без переключателя записали `Assimp enabled: true`.
- Новый `asset.import` вызывает штатные importAsset/createModel и загрузку геометрии/анимации из памяти. Его первая линковка выявила LNK2001/LNK1120: commandlet не подключал vcpkg. Интеграция добавлена в EditorModule.vcxproj для этого target; повторная сборка прошла. Исходный лог сохранён в [commandlet-link-failure.log](evidence/commandlet-link-failure.log).
- **BUG009, исправлен:** DumpFrame не сохранял BMP в каталог с кириллицей из-за `fopen_s` с UTF-8 путём. Color/grayscale BMP используют `_wfopen_s` и `std::filesystem::u8path`. В графический тест добавлена проверка успешного сохранения. [До исправления](evidence/unicode-dump-failure.log), [после](evidence/asset-tests.log).

## Проверки

| Набор | Результат |
|---|---|
| Engine tests | 54/54, 0 skipped, 0 failed |
| Asset workflow | 208 проверок; импорт, материалы/текстуры, перенос сцены, Play, GPU-пиксели и standalone Runtime |
| Skeletal tests | 8/8; GLB, анимация, baker, crowd vertex/LOD |
| Assimp commandlets | 4/4: OBJ, FBX, GLB и повреждённый FBX |
| Commandlet regressions | 25/25 |
| Новый C++-проект | 8/8: создание, enable-code, ошибка компиляции, сохранение рабочей DLL, успешная сборка/hot reload |
| Установленный Runtime | OBJ/FBX/GLB импортированы и размещены через exe из пользовательского EngineStore |
| Четыре игровых проекта | 4/4 обновлены в копиях, собраны, запущены в игре и редакторе |

OBJ: 3 индекса; FBX: 3 индекса и созданный материал; GLB CesiumMan: 14 016 индексов и один анимационный клип. Повреждённый FBX возвращает exit 2 и не оставляет каталог модели. Проверены пути с кириллицей.

[Кадр импортированной геометрии](evidence/imported-model.bmp) осмотрен: видны два зелёных треугольника на тёмном фоне; проверка пикселей выполняется на реальном D3D11 render target. Это проверка импорта и рендера тестовой геометрии, а не художественного качества игровых сцен.

Сценарий `.blend` явно пропущен: не настроены PAINENGINE_BLEND_FIXTURES/PAINENGINE_BLENDER. Он не включён в 208 успешных проверок и не объявлен проверенным. Остальные форматы Assimp вне перечисленных сценариев отдельно не проверялись.

## Проекты и воспроизводимость

Тестовые копии четырёх исходных проектов обновлены штатным `EngineRuntime --upgrade-project`, собраны через `EngineLauncher --build-project` и запущены через `--smoke-project`/`--smoke-project-editor` (по 3 кадра). Все 4 прошли. Каталог копий: `E:/Engine/Engine/build/assimp-enable/projects`. Версия проверена по `Engine.lock.json`, SDK — по `Project.code.json`: везде 2026.11.04.0. Итоговые результаты находятся в [installed-projects.json](evidence/installed-projects.json). Хеши установленных Runtime, Game.dll, commandlet, Assimp DLL и SDK совпадают с проверенной сборкой: [installed-identity.json](evidence/installed-identity.json).

Первая попытка upgrade использовала ещё не созданный родительский каталог назначения и была отклонена до копирования. Каталог создан, повторный вызов штатного upgrade прошёл; исходный диагностический лог сохранён. Это ошибка подготовки проверки, а не сбой загрузки Assimp.

Все исходные игровые проекты сохранены. Логи первой и финальной сборки, регрессий и отрицательных сценариев находятся в `evidence`. Инструкция и протокол `asset.import`: `Engine/docs/COMMANDLETS.md`; новый runner: `Editor/tests/AssimpCommandletTests.ps1`.

Недельный счётчик аккаунта: 16% в начале этой задачи, 17% по завершении. Это округлённый общий счётчик аккаунта; граница 30% не достигнута. Сбросы и дополнительные кредиты не использовались.

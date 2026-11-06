# Проверяемая конфигурация

- Runtime: frozen `2026.11.05.7`, `Engine/build/playtests/20261105-r3/Runtime`.
- Проект создан штатно `EngineLauncher.exe --create-project`, затем `--enable-code`; старые `Source/` и `Content/` перенесены из r2 архива. SDK lock и метаданные остались свежими.
- Сборка: `EngineLauncher.exe --build-project Engine/build/playtests/20261105-r3/calm-city/CalmCity/Project.json` с `PAINENGINE_ENGINE_STORE` и `TEMP/TMP` в проектном каталоге.
- Импорт: `EngineCommandlets.exe asset.import` с source `Engine/build/imported-model-probe/fixtures/blend-suzanne-multiple.blend`, Blender `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`.
- Scene/material: `EngineCommandlets.exe project.scene`, результаты сохранены в `evidence/scene-*.json`. Сцена `Content/Scenes/CalmCityImported.scene.json` содержит движущуюся машину и импортированную Suzanne на позиции (0,2,1), slot 0 = Material_0, slot 1 = Material_1.
- Реальный runtime: `EngineRuntime.exe --project .../Project.json --mode game --frames 30` при `CALM_FRAME_DUMP=1`; окно запускалось скрыто, D3D кадр записан через публичный `RequestFrameDump` из проектного кода.
- SaveGame: два отдельных `EngineRuntime.exe --project ... --mode game --frames 180` процесса с `CALM_SAVE_TEST=save`, затем `load`. Собственный слот проекта сохранён в `CalmCity/SaveData/calm-city/default/manual-1.save.json`.
- Все команды выполнялись последовательно только в каталоге роли; frozen Runtime и общий Engine не менялись.

## Ограничения доказательства

Frame capture — 1000×500, камера города из `ProjectGame` (14,12,18; rotation -28°,38°), реальный D3D11 кадр. Кадр показывает город, материально разделённую импортированную модель и тени города. Тень именно импортированной модели отдельно A/B не изолировалась; UV chart загружена, но её читаемость на модели не оценивалась крупным планом. Редакторские UI действия проверялись только через штатную `.7 project.scene` очередь; ручной Qt Inspector не запускался.

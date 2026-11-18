# Дневник прогона PainEngine 0.4.0

## Подготовка

- Создал private копию проекта из r5 source ZIP и сверил SHA256. Рабочие изменения координатора в `main` не редактировал; все сборки личные и изолированы в `Engine/build/playtests/strategy-040`.
- Штатный upgrade source-only ZIP завершился `Cannot hash game DLL` (exit1): archive намеренно без DLL. Сохранил исходный лог. Взял отдельную полную копию старого r5 проекта; `EngineRuntime --upgrade-project <Project.json> --destination <new dir>` завершился exit0 и подтвердил сохранность оригинала.
- MSBuild ProjectTools Build/Rebuild активировал DLL для SDK 0.4.0; Engine.lib hash совпал с выданным Runtime. Путь проекта содержит кириллицу и пробелы.

## Практические проверки

- MSBuild standalone: `EngineRuntime --project <unicode Project.json> --mode game --frames 60`; exit0, лог содержит `GameUpdates=60`, `EngineStopped`.
- Экспорт: официальный `Export-Project.ps1` создал Unicode bundle; проверены `Game.exe`, sibling Runtime/Game/manifest и report. Запуск `Game.exe --frames 60` завершился exit0.
- Материалы: исходный `auto-multiple.fbx` потребовал внешний `maps/цвет.bmp`; первая ошибка сохранена. Сделал self-authored 1×1 BMP только в private fixture copy и повторил импорт + 13 editor scene operations: slots, assign, undo/redo, invalid traversal rejection, save/reopen, duplicate/undo, whole-object override/clear; тест прошёл.
- Автоматическое воспроизведение настоящего ввода `project.input`: отряд выбран и получил приказ, физический груз открыл проход, трое остановились на местах, gateOpen=1, `barrierViolationFrames=0`; gameplay завершился `lost=1`, потому что базовый r5 сценарий не атакует охранника. D3D BMP сохранён и просмотрен: HUD и игровые объекты видны.
- CMake project: первая сборка в sandbox упала на try-compile `C1902` (PDB); координатор подтвердил ограничение sandbox. Точная сборка повторена с elevated доступом на том же private Unicode проекте; CMake configure прошёл, 8 Ninja steps прошли, DLL активирована. Лог показывает `Build validated and activated`; module hash зафиксирован в статусе.

## UI и происхождение результатов

- Запустил Editor 0.4.0 на обновлённом private проекте. Startup лог показал EditorCreate, viewport overlay и отрисованные кадры; Windows process дал `Main.scene.json — Ремонтный коридор — Engine Editor`, HWND 2295862.
- CUA inventory не перечислил этот HWND/окно, `getApp(windowId)` ответил, что окно не найдено в inventory. Ограничился одной попыткой и не повторял долгие ожидания. Поэтому мой прямой осмотр AI dock, physics inspector, picker и light UI — partial.
- Координаторские UiTests, camera guide, AI diagnostics/API и CMake workflow сохранены отдельно в `coordinator-evidence/` вместе с provenance manifest. Это не личная UI проверка и не подменяет её.

## Сохранённые ошибки

- `standalone-msbuild.*`: первый неверный Start-Process вызов потерял quoting путей; исправленный ProcessStartInfo запуск прошёл.
- `runtime-help.*`: текущий Runtime не принимает `--help`; запуск завершился ошибкой аргумента.
- `material-slots-msbuild.*`: первая fixture-копия FBX была без external BMP; исправлено только в private fixture copy и повторено.
- `build-cmake-project-retry.log`: sandbox C1902; exact elevated повтор прошёл.

## Итог

Подтверждена рабочая совместимость проекта и C++ module для MSBuild и CMake Runtime 0.4.0, экспорт standalone EXE, gameplay replay, API-level material slots и Unicode paths. UI изменения на релизной ветке имеют coordinator test evidence, но личная CUA визуальная проверка ограничена недоступностью окна в inventory. Linux/macOS/CI, DPI масштабы >1 и конкретный визуальный shadow/caster before/after не проверялись лично.

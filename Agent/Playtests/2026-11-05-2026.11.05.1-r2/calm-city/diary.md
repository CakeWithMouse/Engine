# Дневник — роль 1, «Спокойный город»

## Контекст
SDK/Runtime: `2026.11.05.1` из read-only baseline `Engine/build/playtests/20261105-r2/Runtime`. Чистый CLI-проект: `Engine/build/playtests/20261105-r2/calm-city/CalmCityFresh`. Развитие игры — в изолированной копии предыдущего сохранённого CalmCity: `Engine/build/playtests/20261105-r2/calm-city/CalmCityDev`. Исходник из `Agent/Playtests/2026-11-05-savegame/calm-city/source.zip`; старый проект и baseline не изменялись. `PAINENGINE_ENGINE_STORE` указывал на собственную пустую папку `Engine/build/playtests/20261105-r2/calm-city/EngineStore`; SDK поставлен один раз, все сборки проекта использовали только его `Intermediate`.

## Действия и результат
- Штатный EngineLauncher `--create-project` завершился exit 0; затем `--enable-code` создал C++ проект и собрал DLL, exit 0. Smoke чистого проекта — exit 0.
- Штатный `--upgrade-project` создал отдельную копию проекта 2026.11.05.0 для 2026.11.05.1; повторные `--build-project` завершились exit 0 с MSVC в approval режиме. Текущий build ID: `f1f619460add4b6281278abfc8caed4f`.
- Следующий игровой шаг: западный блок получил отдельный `WarmStone.mat` с Tint/Gloss и /Game shader; точечный свет рядом стал мягче/теплее. Небольшая сфера-инстанс ряд имеет CPU-generated LOD; всё находится в `Source/` и `Content/` проекта.
- Прямой запуск baseline `EngineRuntime.exe --project ... --mode game --frames 7` с `CALM_SAVE_TEST=save` записал машину в x=5.6 с левой ориентацией маршрута, переместил её в x=3.1, затем загрузил обратно в x=5.6 и продолжил до x=5.35. Второй отдельный процесс с `CALM_SAVE_TEST=load` подтвердил тот же x=5.6 -> 5.35. Оба exit 0.
- Прямой D3D11 запуск записал `calm-city-lod.bmp`. Панель на завершённом кадре показывает 8/8 видимых инстансов, 0 hidden/distance/frustum; LOD0=5, LOD1=5 при переходе 18 px. Фактическая геометрия LOD сократилась с 528 до 264 треугольников; silhouette drift 0%. Материал West block зафиксирован как загруженный (`loaded=1`). BMP просмотрен.
- Дополнительные отдельные commandlet probes (не общий тест coordinator): `mesh.primitive`, `camera.screen-ray`, `scene.property`, `gizmo.snap`, `game.settings`, `session.lifecycle`, `trace.raycast`; все точные команды exit 0, входы/результаты/логи лежат в `evidence/commandlets/`.

## Ошибки и ограничения
- В первом запуске commandlet probes ошибся в именах и использовал короткие descriptive aliases вместо точных имен из `--list`. Все 7 операций закономерно вернули exit 2; после сверки с `COMMANDLETS.md` повторены с точными именами, exit 0. Записано в `issues.md`.
- При упаковке сначала указал неправильный путь к слоту; слот находится в `SaveData/calm-city/default/manual-1.save.json`. Исправил путь, упаковал исходники и контент без DLL/SDK/Intermediate. Записано в `issues.md`.
- Не запускал CUA/редактор. UI ручные сценарии (Play/Pause, Hot Reload, Inspector, Material Graph) остаются unverified. Поэтому broad-coverage feature IDs в `evidence.json` отмечены partial или not-tested, а не распространяются по аналогии с D3D/commandlet результатами.

## Воспроизведение
Сборка: `Engine/build/playtests/20261105-r2/EngineLauncher.exe --build-project Engine/build/playtests/20261105-r2/calm-city/CalmCityDev/Project.json` с `PAINENGINE_ENGINE_STORE` на собственный `EngineStore`; выполнить в штатном MSVC PowerShell с elevation.
Запуск сохранения: `Engine/build/playtests/20261105-r2/Runtime/EngineRuntime.exe --project Engine/build/playtests/20261105-r2/calm-city/CalmCityDev/Project.json --mode game --frames 7`, отдельно с `CALM_SAVE_TEST=save` и `CALM_SAVE_TEST=load`.

## Role1 backfill on SDK 2026.11.05.3
- Upgraded and built a separate CalmCityR3 copy with the frozen .3 SDK (exit 0). Hot reload replaced the active game DLL (`replaced=true`); the moving CarMover advanced x=-5.48 to -5.28 in the same runtime after replacement.
- The .3 `project.scene` run performed 21 scene-history operations on city blocks with 0 failed steps: create/delete, group transforms with undo/redo, parent hierarchy, selection, duplicate custom CarMover data, Save As, New, Load, and undo restoration. Separate primitive run created Cube/Sphere/Plane/Cylinder/Cone/Pyramid, all operations succeeded.
- A real Qt Material Graph window accepted AI paste, Undo, Redo, Apply, Save; a separate scratch Revert restored baseline material content. A locked sidecar produced an expected access-denied save (`accepted=false`, dirty retained), followed by successful Revert and Save. The graph PNG captures the Qt editor but not native D3D preview pixels.
- Native `.3` EditorUiTests on a fresh CLI project exited 0 with 5029 checks passed. Selected material, graph-error, texture-slot, directional-light, and HUD screenshots are stored in `evidence/r3/`; this is an automated native-window UI test, not manual GUI use.
- A scratch A/B D3D probe for bare `CubeComponent::CreateInstance` vs `MeshComponent` control showed incorrect legacy primitive output vs three separate mesh-control silhouettes at the same X=-3/0/3. Logs, .3 BMPs and source path are recorded in `issues.md`; no Engine/SDK code was touched by this persona.
- Updated evidence status count: passed 29, partial 11, not-tested 11 (51 assigned feature rows). Some broad features remain partial/not-tested; see per-ID evidence rather than extrapolating from the full UI suite.
- Recovery follow-up on a separate fresh CLI template: `EditorUiTests --workflow` exited 0 after group/save/undo/redo, writing a checkpoint and simulating abrupt process termination. A second process `--recovery-check` exited 0, loaded the checkpoint, counted all 7 unsaved objects and verified a non-empty module hash. Native UI recovery is now directly tested; exact logs in `evidence/r3/scene-recovery/`.
- The native UI suite also exercised selected CodeClassDialog validations; a supplemental partial note records only the exact assertions, not all four class kinds. The shared matrix assigns class-wizard to roles 3/4, so it is supplemental, not included in the role1 assigned-feature total.
- Role1 assigned feature totals now: passed 30, partial 11, not-tested 10 (51 rows). `evidence.json` includes one extra supplemental `class-wizard` row beyond those 51 assigned rows.
- After root delivered frozen .4, I created another clean CLI C++ project and repeated the exact legacy Cube/Mesh A/B with the .4 SDK. Two 30-frame D3D runs exited 0 and produced reviewed 1000×500 images; both now show three separated silhouettes at X=-3/0/3. The project source contains no workaround. Preserve .3 failing frames as before evidence and .4 frames as post-fix evidence.
- Assigned feature totals after this fix retest: passed 31, partial 10, not-tested 10 (51 feature-matrix IDs). The supplemental class-wizard note remains separate and partial.

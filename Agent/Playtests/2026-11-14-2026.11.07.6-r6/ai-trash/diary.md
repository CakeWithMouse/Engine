# Дневник — роль 4, AI Trash, r6

## Подготовка и копия проекта

Прочитаны `try-engine`, требования Agent, `COMMANDLETS.md`, `MATERIALS.md`, `RENDERING.md`, актуальные API для ввода/сцены/сохранений. Исходник r5 — `ai-trash-r5-source.zip` (SHA256 `598CFD71AF4744F0B6415EE9E4DAAB44A1DE815A93D3AA7AFB918AE13761EA2E`); отдельная распаковка r5 `Source/Content` и метаданных подготовлена в моём `Engine/build/playtests/20261114-r6/ai-trash/ImportedSource`. Старый проект r5 не изменён.

`EngineLauncher --create-project` и `--enable-code` для `FreshTrash` завершились exit 0. Затем существующий полный проект `Engine/build/playtests/20261107-r5/ai-trash/TrashGame` обновлён только в `TrashGameUpgraded`; исходник сохранён. Upgrade exit 0, `Engine.lock.json` закреплён за `2026.11.07.6`. После переноса обновлены timestamps файлов `.cpp/.h` в моей копии; `ProjectTools.ps1 -Action Build -Project ... -Runtime Game/build/x64/Release/Runtime` завершился exit 0 на финальном общем SDK. Координатор попросил пересобрать также прошлый Calm City: его изолированный проект r6 тоже пересобрался текущим SDK, exit 0. Launcher `--check-project` для TrashGame exit 0.

Все ролевые проекты, импорты и временные каталоги лежат только в `Engine/build/playtests/20261114-r6/ai-trash`. Движок и общий SDK я не менял.

## Игровой фрагмент: бросок и счёт

Маленькая правка на понятность: HUD теперь показывает `Hits | Misses | Tosses`, а панель расширена с 300 до 360 px, чтобы строка помещалась. Остальные UI подписи оставлены; типы броска, итог раунда и точность показываются прежним кодом.

Собственный `project.input` сценарий final commandlet послал Right, затем Space в игровой Play-сессии, получил 129 simulation frames, один новый `Trash.MultiMaterialProjectile` и `Hit sparks`. Это подтверждает ввод и игровой hit callback после финального исправления Play simulation, а не только появление компонента.

Stand-alone SaveGame прогон на обновлённой DLL: отдельный `TRASH_SAVE_TEST=save` процесс сохранил импортированный предмет в полёте при elapsed=0.483334, затем восстановил его после изменения сцены (ровно один бросок, hits=0). Другой процесс `TRASH_SAVE_TEST=load` восстановил elapsed=0.5 и продолжил до `hits=1 throws=1 PASS`. Оба exit 0, 180 кадров. BMP in-flight кадра просмотрен: предмет на траектории, HUD показывает `Hits 0 | Misses 0 | Tosses 1`.

Дополнительная компактная серия из 3 типов на нейтральном Aim: imported multi-material item попал; сода и скомканная бумага столкнулись с obstacle, так что итог `Hits 1 | Misses 2 | Tosses 3`, accuracy 33%. HUD явно сообщил `The trash hit an obstacle.`; это воспроизводимый игровой балансный отзыв на точность типов при Aim=0, а не дефект Engine. Игрок может менять Aim влево/вправо; для следующего шага можно подстроить разные траектории/подсказку.

## Новые функции

- **material.graph** на scratch-копии `Cardboard.mat`: `enable`, `save`, штатный preset 1, `apply`, `revert`. Командлет exit 0, все пять операций accepted, `nativePreviewWindow=true`, после save dirty=false, после revert dirty=false; HLSL получил сгенерированный preset, редактор сообщил «Шейдер скомпилирован». PNG — Qt окно/холст, native D3D preview оно не захватывает. Рабочий проект не изменён.
- **project.import**: собственный проект `FreshTrash`, GLB из r5, `scale=0.01`, `reimportSource` и `moveTo=/Game/Models/Moved/blend-suzanne-multiple.glb`. Exit 0; sidecar записал scale≈0.01 без dialogs; после первичного импорта, повторного импорта и переноса размещено 1→2→3 модели, у всех scale=[0.01,0.01,0.01]; перенесённый файл и sidecar существуют.
- **Каскады**: после освобождения desktop выполнен `Battle.exe --units 1000 --camera 120 --no-point-light --camera-sequence .../GpuChecks/cascade-dolly --camera-motion dolly` на RTX 4070 Laptop GPU. Exit 0; 121 BMP и матрицы, analyzer exit 0. У всех трёх каскадов `scaleChanges=0`; max fractional texel drift 5.12e-7 / 0 / 0 при пороге 0.002. Полный набор 121 кадр оставлен в build; в `evidence/` сохранены матрицы, summary и кадры 0/120.

Другие новые shadow/transparency опыты в этой ограниченной роли не добавлял.


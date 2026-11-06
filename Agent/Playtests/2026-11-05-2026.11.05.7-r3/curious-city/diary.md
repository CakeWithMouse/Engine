# Дневник — Curious City (персона 2)

## Подготовлено до READY

- Исходный недельный usage: codex primary `usedPercent=41`, окно 10080 минут, reset `1794317816` (Unix seconds), без кредитов. Счётчик общий аккаунту и округлён.
- Проверен git status. Рабочие изменения релиза находятся у координатора; мои записи ограничены `Agent/Playtests/2026-11-05-2026.11.05.7-r3/curious-city` и `Engine/build/playtests/20261105-r3/curious-city`.
- Прочитан REBUILD из `Agent/Playtests/2026-11-05-2026.11.05.1-r2/REBUILD.md`: чистый новый проект обязателен, затем перенести только `Source` и `Content`, сохранить свежий lock/toolchain. Корневого REBUILD.md нет.
- Предыдущий Curious City распакован в `Engine/build/playtests/20261105-r3/curious-city/old-source`; исходный архив не менялся. Рабочая staging-копия `staged` содержит только `Source`/`Content`.
- До сообщения READY не запускал launcher, генератор, компиляцию, редактор или игру. В частности, CLI `--enable-code` отложен, поскольку может начать сборку.

## Сценарий после READY

1. Через штатную поставку launcher создать чистый C++ проект документированной последовательностью `--create-project <folder> <name>`, затем `--enable-code <Project.json>`. Оба GUI процесса дождаться; сохранить stdout/stderr, exit codes и CodeBuild.log. Не переиспользовать исторические DLL.
2. Скопировать подготовленные `Source`/`Content` в новый scaffold, оставив новый Engine.lock.json, Build.ps1, Game.sln/vcxproj и метаданные. Перенести код дохода/покупки, проверить генерацию новых reflected типов.
3. Добавить короткий игровой цикл: выбор точки по экранному лучу/trace, покупка и spawn одного из разных типов домов (разные материалы), доход по времени, HUD balance/count.
4. Импортировать многоматериальную скелетную модель с минимум двумя submesh/source material IDs. Снять исходное число/ID slots, назначить материал только в одном sourceID и сравнить реальные GPU пиксели/ROI; отдельно проверить общий `SetMaterialAsset` override и очистку override, после которой должны вернуться раздельные slots. Сохранить сцену, закрыть/открыть и проверить `projectData.materialSlots`; повторить назначение/снятие через инспектор и Undo/Redo, если UI-сессия будет выделена координатором.
5. SaveGame состояния экономики и сцены, загрузка в текущем процессе, затем отдельный процесс и восстановление с тем же slot. После load снова выбрать место и построить дом.
6. Записать launch/build ошибки немедленно. Снимки только настоящих edit/game frames; путь файла, viewport, камера, render settings и шаги будут добавлены в evidence.

## Результаты на Runtime .7

- READY использовал `Engine/build/playtests/20261105-r3/Runtime` и соседний launcher. Через штатный `--create-project` создан `fresh-city`, `--enable-code` добавил и собрал код в новом scaffold; исторический Game.dll не переносился. Проектный `Engine.lock.json` закреплён за 2026.11.05.7.
- Перенёс прежние `Source`/`Content`. В наследованном `ProjectGame::AfterInitialize` находился ранний выход при `RotatingCube`; в fresh Play starter cube приводил к тому, что городские контролы не добавлялись. Удалил только этот проектный ранний выход, повторная сборка добавила `CityControls` и привязала HUD.
- Штатный `asset.import` конвертировал настоящий `blend-suzanne-multiple.blend`: два меша, 11,808 индексов, source IDs `0` и `1`, отдельные Material_0/Material_1. Scene commandlet присвоил слоты, общий material override на обе части и очистку override. Пиксельное сравнение настоящих 1000×500 GPU кадров подтверждает изменение 10,179 пикселей в пределах изменённой части. Снятый override возвращает слотные материалы.
- `project.scene` принял `/materialSlots/0`, сохранил ID 0/1 в `projectData.materialSlots`, прошёл Undo/Redo, Save и повторный Load. C++ код проекта на SkeletalMeshComponent (CesiumMan, один клип/slot) прочитал IDs, `SetMaterialSlot(0, valid.mat)=true`, неизвестный ID дал `false`, общий `SetMaterialAsset(path)` и `SetMaterialAsset("")` сработали; после очистки в slots осталась назначенная ссылка.
- Игровая порция: после world cursor raycast к `Ground` создаётся дом по позиции hit, покупка списывает $100 только при успешном добавлении, alternate build использует второй `.mat`. HUD отображает баланс, доход и число домов. Через реальные `project.input` клики получены событие спавна, кнопки Build/Save/Load и восстановление сцены. Условия: viewport 640×480, ground click (500,400), UI clicks (70/170/250,120), key down удержан 1 кадр и затем отпущен.
- Runtime cycle 130 кадров: первый дом за $100; доход $10/sec; вторую покупку сделал после тика (баланс $310/2 дома), затем загрузил прежнее сохранение $400/1 дом и продолжил доход до $410. В новом процессе сохранение загрузилось с $400/1 дом и продолжило доход до $410. SaveGame JSON содержит `materialSlots` ссылки на оба .mat; после обнаруженного координатором BUG-R3-001 восстановление материала после SaveGame пока оставляю `partial` до узкого retest на .8.
- Изолированный `project.scene` прогон проверил create/move/group move/duplicate/delete, undo/redo, Save As и reopen. Отдельный `Gameplay.Player` с W-вводом переместился по Z с 0 до -2.4 за 30 кадров; Stop восстановил edit-сцену и координату 0.
- Проверил ортографический и перспективный `camera.screen-ray`, базовый `trace.raycast`, цикл GameSession, фильтр каналов и ignore ID на луче, проходящем только через Ground (`Hit / Miss / Hit / Miss`). `DebugDrawTrace` создал 5 линий, `ClearDebugTraces` уменьшил счётчик до 0.
- Скомпилировал штатный Runtime SDK `TraceExamples.cpp` вместе с проектом. Проект через EngineModuleVfs прочитал `/Game/Config/city-settings.txt` (`district=harbor`). Завершённый frame stats показывает число draw calls и видимых объектов.
- `ui.edit-text` сохранил изменённую строку в `CityHud.ui`, а Runtime перечитал её при запуске. Одинаковые standalone A/B кадры 1000×500 совпадают вне подписи кнопки: ровно 758 изменённых пикселей в bbox x=27..124/y=114..130. Panel/labels/Save/Load не менялись. Глифы `•` и `₽` отсутствуют, что соответствует документированному диапазону font atlas; UI-макет не испортился. Manual Qt inspector, live HUD hot reload и resize viewport не тестировались.
- Export-Project валидировал проект .7 и создал локальный CityExport; отдельный bundled Runtime process прошёл 3 кадра, exit 0. Launcher check/smoke также завершились exit 0.
- `blend-uv.blend` импортирован с шестью индексами, но в двух тестовых Runtime кадрах пиксельное окно модели осталось фоновым. Попытка менять rotation/scale не сделала UV mesh видимым. Записываю `partial`, а не вывод о дефекте импортера; нужна камера/геометрия, подобранная по bounds. Imported-shadow режимы отдельно не сравнивал.

## Сборка и ограничения

- Первая game-code сборка упала на моих ошибках: `glm::vec3` не был включён в объявление overload и `GameComponent` не имеет `GetName()`. Изменения исправлены, неудачная диагностика сохранена; последующие `--build-project` завершились с exit 0.
- Первый CLI staging copy ошибочно создал вложенную структуру `Source` в PowerShell `Copy-Item`; исправлено копированием содержимого старого каталога в готовые `Source`/`Content` папки. Корневые/старые artifacts не менял.
- Первые `project.input` клики были с down/up до кадра и не выдавали событие; добавил один frame между down и up. Первый trace probe прошёл через starter Cube; изменил точку на свободный участок Ground и получил ожидаемую комбинацию фильтров.
- `view_image` preview для BMP/PNG дважды показывал неполный результат UI и не был надёжным источником. Сверил BMP через независимые pixel samples и full A/B diff; расчёт подтверждает одинаковый UI вне неподдержанных glyphs.
- После отчёта координатора о BUG-R3-001 в .7 не запускаю новые SDK/runtime сборки. Slot SaveGame JSON сохранился, но фактическое отображение slots после restore требует повторной проверки только после следующего frozen Runtime.

## Артефакты

- [CuriousCity-source.zip](CuriousCity-source.zip) содержит только `Source` и `Content` (проектный код, сцены, импортированные малые GLB/текстуры, без Engine SDK, DLL и Intermediate).
- Компактные результаты/PNG находятся в `evidence/`; полный рабочий build/run scratch остаётся в `Engine/build/playtests/20261105-r3/curious-city`.

## Узкий retest на frozen Runtime .8

- Координатор сообщил READY для `Engine/build/playtests/20261105-r3/final8/Runtime`, с Engine.lib SHA-256 `4200175F40B68E2662B14AA7D5458A2CEC00A70D9DE9721ED1AED86FEB79D549`. Через штатную команду `--upgrade-project` создан `Final8City`; новый game module собран и активирован против .8. Первая валидация моей изменённой сцены отказала из-за `id: 5.0` (ожидался unsigned integer); исправил fixture на `5`, повторная сборка прошла. Лог отказа сохранён.
- В копию Main добавил импортированную multi-material Suzanne. Первое SaveGame состояние: построил дом за 100, сохранил `city`, построил второй дом после дохода и загрузил сохранение. После загрузки balance/houses вернулись к 400/1, доход продолжился до 410. Второй отдельный процесс с тем же SaveRoot загрузил тот же слот и также продолжил доход.
- Probe в `CityEconomySubsystem::OnSaveGameLoaded` прочитал `GetMaterialSlots()` после восстановления: два source IDs; slot 0 — CesiumMan Material_0, назначенный кодом перед сохранением; slot 1 — Suzanne Material_1. Это проверка реального SkeletalMeshComponent после Scene replacement, а не только сохранённого JSON. Визуальный post-load frame содержит HUD и видимую Suzanne; обе карты сохранены в PNG. Slot persistence обновлён с .7 `partial` до .8 `pass` в пределах этого проекта.
- Scene CRUD нового .8 проекта сохранил/открыл custom `ProjectComponent` с прежним ID/именем. Это не отдельный `ENGINE_OBJECT` / GameObject adapter; feature `scene-object` оставлен `not-tested`, а результат не засчитан как его покрытие.
- Recovery на Final8City сначала не запустился: UI test получил Project.json вместо требуемой папки; ещё одна попытка не нашла неверно указанную launcher path. Исправленный штатный сценарий на чистом nocode RecoveryProbe прошёл: `--workflow` создал unsaved checkpoint и симулировал аварийное завершение; новый процесс `--recovery-check` восстановил 7 объектов и проверил module SHA. UI/native lease возвращён координатору.
- На свежем nocode AssetProbe .8 hidden `--authoring` дал exit 0 и 215 checks passed: 2×2 red→blue текстура переимпортирована, ассет перемещён в `/Game/Images/Renamed.bmp`; две операции оставили оригинальные backup и Operation.json. Сценарий и контрольное изображение помещены в evidence.
- В своём дополнительном readback коде ошибочно записал C++ character literal `'\\n'` вместо newline; в рабочем scratch-логе slot record склеился со следующим сообщением и получил числовой суффикс. Значения обоих slot paths при этом различимы; raw logs сохранены рядом с читабельной normalized evidence copy. Это дефект только диагностического кода playtest.
- Полный .7 проект и результаты не менялись; отдельная `CuriousCity-final8-source.zip` содержит только Final8City `Source` и `Content`, без SDK/движковых DLL/Intermediate.
- Завершающая короткая проверка scene-object: добавил в Final8City отдельный CityRulesObject с ENGINE_OBJECT и специализацией ProjectObjectAdapter<T> (Create/Save/Load). Проектная сборка активировалась; ProjectTypes.tsv зарегистрировал Object CuriousCity.CityRules. Scene commandlet сохранил balance=500/buildingLimit=12, затем тестовый JSON изменил на 850/20 и загрузил штатным scene loader; результат восстановил тип, ID и значения 850/20 через codec. Это независимый project object без GameComponent inheritance. Feature обновлён на pass; Inspector palette не проверял.


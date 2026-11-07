# Diary — уставший автор 2D бомбардировки, PainEngine 2026.11.07.0 (HEAD 55d3b0c)

## Создание и сборка
- Начальный общий usage: 50% от недельного окна 10080 минут; контрольная точка завершения этой порции: 53%, общий порог остановки 75%.
- Исходный source-only проект r3 скопирован в build/playtests/20261107-r5/tired-bomber/BomberProject. Чистый CLI проект создан штатным EngineLauncher в BomberCleanR5, exit 0.
- Первый sandbox вызов enable-code завершился exit 1 на LINK LNK1101: неверная версия MSPDB140.DLL. Полный лог: evidence/environment-linker-failure.log. Elevated retry прошёл exit 0; созданный проект подтвердил Project is already up to date.
- Перенёс Source, Content и ProjectTypes.tsv в чистый проект. Первый rebuild пропустил перенесённые старые по timestamp исходники и упал на LNK2001 по методам ProjectGame/Subsystem. Полный лог: evidence/copied-source-incremental-failure.log. Это дефект процесса переноса/инкрементальной сборки исходников, не runtime дефект. После смены timestamps и выхода Runtime .07.0 проект обновлён официальным upgrade-project в BomberFinalR5. Engine.lock.json фиксирует 2026.11.07.0 Release. build-project прошёл exit 0; полный лог evidence/final-build-r5.log перечисляет 7 game .cpp и GeneratedComponents.cpp.
- Свежий EngineLauncher и общий Engine не изменялись. Проект собирался в отдельном PAINENGINE_ENGINE_STORE и build каталоге.

## Игровой результат
- EngineCommandlets --list подтвердил project.input. Вызов EditorQtHost play/focus/600 frames/pause/4 frames завершился exit 0, ok:true, frames=604. Но лог не показывает вызов PreUpdate/bomb hit, а readActiveScene snapshots остались авторскими; это не подтверждение геймплея.
- Реальный standalone Runtime: EngineRuntime.exe --project BomberFinalR5/Project.json --mode game --frames 220, exit 0, NVIDIA GeForce RTX 4070 Laptop GPU. Лог: бомба выпущена на t=1.01667 s, точная трасса попала в unit id=7 на t=1.95 s: trace=Hit, queries=58, triangles=28. Счётчик события увеличен кодом игры, затем HUD показан координатором как hits 1, misses 0. Hit frame сохранён как evidence/game-hit-r5.bmp, 1000x500.
- Координатор визуально осмотрел hit frame: game visual passed; HUD читается, видны цели, цветные группы, свет/тени и LOD overlay. Этот кадр не доказывает конкретно тени point light. Edit-view отдельно не осматривался.

## Тени и шейдеры
- Кодовое чтение HEAD: UpdatePointShadows и RenderPointShadowMaps вызываются каждый кадр; до двух активных point-light shadow cubes; InstancedMesh разбивает caster stream по граням через PointShadowFaceMask; RenderShadow учитывает CastsShadow(). Это проверка кода, а не практический A/B.
- Не проверены: moving caster/light shadow следование, материал castShadows on/off на картинке, Forward/Deferred парные кадры, edit-view, Pause/Resume/Stop внутри игры. Визуальный point-shadow status — unverified.

## Воспроизведение
PowerShell 7: вызвать EngineLauncher --create-project <новая папка> TiredBomber, затем --enable-code <Project.json>; перенести Source/Content r3, обновить ProjectTypes.tsv paths; вызвать --upgrade-project <Project.json> <новая папка BomberFinalR5>; затем --build-project <BomberFinalR5/Project.json>. Запуск: EngineRuntime.exe --project <BomberFinalR5/Project.json> --mode game --frames 220. Фактические логи, commandlet request/result и кадр лежат в evidence/.

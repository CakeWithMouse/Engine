# Дневник — спокойный автор красивого города, r6

## Выполнение

Ветка/релиз по координатору: HEAD `d74aebb`, `2026.11.07.6`; общий Release Runtime/SDK уже был собран координатором. Установка Runtime и проекты работали с частным `PAINENGINE_ENGINE_STORE=Engine/build/playtests/20261114-r6/calm-city/EngineStore` и своим TEMP.

Штатный fresh path: `EngineLauncher --create-project` создал `FreshCalmCity`; `--enable-code` собрал пустой шаблон (exit 0, `enable.exitcode.txt`). Это подтвердило CLI и кодовую активацию, но проект не использовался как игровая копия.

Копия r5 из zip без `Binaries` ожидаемо не могла пройти upgrade напрямую: Runtime сообщил `LaunchError: Cannot hash game DLL`; исходник не менялся. Затем координатор указал реальный сохранённый игровой проект r5 `Engine/build/playtests/20261107-r5/calm-city/CalmCityFinal`. Его upgrade только в мой `CalmCityUpgraded` завершился exit 0, `Engine.lock.json` закрепился за `2026.11.07.6`; `--build-project` после маленькой правки также exit 0. Старую копию не менял.

В своём `CarMover.h` снизил скорость маршрута с `2.5` до `1.8` единиц/с: машина пересекает спокойный квартал медленнее и остаётся различима на общем кадре. Пересобрал проект.

Runtime `--mode game --frames 180` вернул exit 0, лог содержит загрузку игровой DLL/сцены, выбран RTX 4070 Laptop GPU и `GameUpdates=180`. `CALM_FRAME_DUMP=1` сохранил 1000×500 кадр `CalmCityUpgraded/SaveData/calm-city-lod.bmp`; кадр просмотрен: квартал, машинка, свет/тени и overlay LOD. `--check-project` после запуска exit 0.

SaveGame: сценарий `CALM_SAVE_TEST=save` вернул exit 0; лог фиксирует endpoint x=5.6, сохранение и восстановление в процессе, продолжение маршрута до x=5.35. Отдельный новый процесс `CALM_SAVE_TEST=load` также exit 0, прочитал x=5.6 из сохранённого слота и продолжил до x=5.35. В сумме это проверило сохранение и повторное открытие сохранённого игрового состояния между процессами.

## Собственные опыты функций

- `render.transparency`: отдельный параметризованный JSON, 3 пересекающиеся разноцветные translucent плоскости, 4 положения камеры, Deferred, настоящий GPU. Exit 0, 4 кадра, 117 draw calls каждый. Один кадр просмотрен: все поверхности видны и пересекаются. Это короткое наблюдение, а не пиксельный oracle полного сортировочного контракта.
- `render.point-shadows`: один собственный JSON на 3 кадра, crowd caster, 3 point lights (лимит 2), camera distance 8→14→24. В кадре 0 слоты `0,1,-1`; у первого источника интенсивность 0 — кадр 1 слоты `-1,0,1`; при восстановлении — `0,1,-1`. Для камеры отразились LOD buckets `[1,0,0]`, `[1,0,0]`, `[0,1,0]`; все кадры имеют две активные карты, все шесть cube faces попали в результат. BMP кадра со сменёнными слотами просмотрен, видны две point-shadow проекции. Результат подтверждает освобождение слота нулевой интенсивностью и один переход LOD, не полный предел/переходный sweep.
- `project.import`: отдельные scratch-проекты, настоящий импорт модели Calm City с default scale и с custom scale 0.5. Оба exit 0, `sidecarExists:true`; первый создал `.importsettings.json` со scale=1 и объект `[1,1,1]`, второй — scale=0.5 и объект `[0.5,0.5,0.5]`. Scratch проекты изолированы от основной игры.

## Непроверенное и выводы

Каскадное движение дополнительно проверено документированным побочным сценарием `Battle.exe --units 1000 --camera 120 --no-point-light --camera-sequence <мой GpuChecks/cascade-sequence> --camera-motion orbit`: 121 кадр, три каскада. `AnalyzeShadowCameraSequence.py --check` вернул scaleChanges=0 для каждого; максимальный дробный дрейф сетки 5.12e-7 / 2.36e-7 / 5.94e-8 texеля, существенно ниже порога 0.002. Это прямое техническое измерение стабильности каскадов на Battle, но не в самой городской игре. Сохранил полный набор 121 BMP в build для воспроизводимости; в кратких свидетельствах использую кадры 0 и 120, matrices.csv и cascade-analysis.json. Материал с normal map при неравномерном/зеркальном масштабе в этом проходе не отрисовывал; determinant fix подтверждён только по коду. Editor UI и ручное назначение материала не открывал. Import settings проверен через действующий commandlet, а не через ручную работу окна.

На сохранённом кадре Suzanne magenta намеренно: `Material_1.mat` имеет sourceMaterial `Second material magenta` и Tint [1,0,1,1], `BaseMap:null`; лог не содержит ошибок шейдера/материала. Сравнение с r5 save-load кадром показывает тот же объект и цвет, поэтому это baseline сцены, не сломанная миграция.

После исправления общего Release повторил финальную сборку через текущий `Game/build/x64/Release/Runtime/SDK/ProjectTools.ps1 -Action Build -Project ... -Runtime Game/build/x64/Release/Runtime`; exit 0. Затем повторил финальные save/load процессы на текущем `EngineRuntime.exe`, оба exit 0, same-process и new-process SaveGame PASS (`loadedX=5.6`, `continuedX=5.35`). Финальные логи, exit codes и кадр сохранены в `evidence/` вне ignored build. Полная каскадная последовательность остаётся в build; report копирует только матрицы, анализ и кадры 0/120.\n\nПодтверждённых дефектов этой роли нет. Все сборки относятся к отдельному проектному Intermediate; общий Engine/Runtime я не пересобирал.






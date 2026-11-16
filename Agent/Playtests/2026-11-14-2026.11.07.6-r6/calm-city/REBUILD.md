# REBUILD — Calm City r6

Копия проекта: `Engine/build/playtests/20261114-r6/calm-city/CalmCityUpgraded`.

Исходный реальный проект r5: `Engine/build/playtests/20261107-r5/calm-city/CalmCityFinal` (путь предоставил координатор; старые файлы не изменялись). Сначала проверен source-only `ImportedSource` из `calm-city-r5-source.zip`: прямой upgrade отказал с `LaunchError: Cannot hash game DLL`, так как архив осознанно не содержит Binaries. Это не считалось проблемой движка.

Рантайм: `Game/build/x64/Release/Runtime`; Launcher и EngineCommandlets — из `Game/build/x64/Release`. `EngineLauncher --install-engine <Runtime>` установил Runtime в частный `EngineStore`. Перед командами Launcher/Runtime выставлялись:

```powershell
$env:PAINENGINE_ENGINE_STORE='E:/Engine/Engine/build/playtests/20261114-r6/calm-city/EngineStore'
$env:TEMP='E:/Engine/Engine/build/playtests/20261114-r6/calm-city/Temp'
$env:TMP=$env:TEMP
```

Успешные команды (в этой последовательности):

```powershell
EngineLauncher.exe --create-project <.../FreshCalmCity> 'Calm City r6'
EngineLauncher.exe --enable-code <.../FreshCalmCity/Project.json>
EngineLauncher.exe --upgrade-project <.../20261107-r5/calm-city/CalmCityFinal/Project.json> <.../20261114-r6/calm-city/CalmCityUpgraded>
# В своей копии изменён Source/CarMover.h: скорость 2.5 -> 1.8 единиц/с.
EngineLauncher.exe --build-project <.../CalmCityUpgraded/Project.json>
EngineRuntime.exe --project <.../CalmCityUpgraded/Project.json> --mode game --frames 180
EngineLauncher.exe --check-project <.../CalmCityUpgraded/Project.json>
# Два новых процесса, каждый --frames 180:
$env:CALM_SAVE_TEST='save'; EngineRuntime.exe --project <.../Project.json> --mode game --frames 180
$env:CALM_SAVE_TEST='load'; EngineRuntime.exe --project <.../Project.json> --mode game --frames 180
```

Коды create/enable/upgrade/build/gameplay/check/save/load: 0. `enable.stderr.txt`, `CodeBuild.log`, `upgrade.stdout/stderr`, `build.stdout/stderr`, `gameplay/save/load.stdout/stderr`, `check.stdout/stderr` и соответствующие exitcode files сохранены в папке build run. Свидетельство межпроцессного SaveGame: `CalmCityUpgraded/SaveData/SaveGameSelfTest.log`. Наблюдаемый кадр города: `CalmCityUpgraded/SaveData/calm-city-lod.bmp` (1000×500), `LodPanel.log`.

После финального исправления Release обоим проектам выполнен отдельный SDK build на текущем Runtime, а Calm City затем прошёл save/load roundtrip в двух новых процессах. Финальные evidence вне build: `evidence/final-project-build.log`, `evidence/final-sdk-build.exitcode.txt`, `evidence/SaveGameSelfTest.log`, `evidence/final-save-load-new-process.bmp`, соответствующие process exit codes. Команда финальной сборки: `& Game/build/x64/Release/Runtime/SDK/ProjectTools.ps1 -Action Build -Project <.../CalmCityUpgraded/Project.json> -Runtime Game/build/x64/Release/Runtime`.\n\nСобственные командлеты, не shared test suite: входы, результаты, логи и BMP — `Engine/build/playtests/20261114-r6/calm-city/GpuChecks`. Каскадный sequence: `cascade-sequence-full` содержит 121 кадр/матрицу; сокращённый evidence с summary и двумя кадрами также зафиксирован в `cascade-analysis.json`. Прозрачность: `render.transparency transparency.json transparency-result.json`, 4 положения камеры, 3 пересекающиеся слоя. Point shadows: `render.point-shadows point-shadows.json point-shadows-result.json`, 3 источника, 3 кадра, zero-intensity slot switch и crowd LOD. Import defaults/custom: scratch проекты `ImportDefault`, `ImportCustomScale`, запросы/результаты/снимки — `ImportChecks`.

Исходный архив проекта для пересборки: `calm-city-r6-source.zip` рядом с этим файлом. SHA256 `ED6A4A817A28B5ADC4CD567F5BF338F30E042D69DF58342DEFFE61E59D093392`. Архив содержит только `Source/`, `Content/`, `Project.json`, `Engine.lock.json`, `Project.code.json`, `Build.ps1` (37 entries); DLL, SDK, `Binaries`, `Intermediate` и SaveData исключены.



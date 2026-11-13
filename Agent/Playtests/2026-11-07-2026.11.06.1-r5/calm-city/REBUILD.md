# REBUILD — Calm City r5

Исходники роли: [calm-city-r5-source.zip](calm-city-r5-source.zip). Архив содержит только Source, Content, Project.json, Engine.lock.json, Project.code.json и Build.ps1. Исходный архив r3 сохранён отдельно: [calm-city-r3-source.zip](calm-city-r3-source.zip).

Проверено Windows x64 Release, MSVC v143, Runtime `2026.11.07.0`. Свежий проект первоначально создан `EngineLauncher.exe --create-project` и включён `--enable-code` на Runtime `2026.11.06.1`. Исходники были перенесены после создания пустого проекта и собраны штатным `--build-project`. Проект затем скопирован командой `--upgrade-project` в `CalmCityFinal`, обновлён до Runtime `2026.11.07.0` и заново собран.

Здесь `$root` — `E:/Engine/Engine/build/playtests/20261107-r5/calm-city`; Launcher и Runtime — из `E:/Engine/Game/build/x64/Release`. Команды, выполнявшиеся с собственными EngineStore/TEMP:

```powershell
EngineLauncher.exe --create-project "$root/CalmCity" "Calm City r5"
EngineLauncher.exe --enable-code "$root/CalmCity/Project.json"
# Перенести Source и Content из ImportedSource, оставить свежие метаданные проекта.
EngineLauncher.exe --build-project "$root/CalmCity/Project.json"
EngineLauncher.exe --upgrade-project "$root/CalmCity/Project.json" "$root/CalmCityFinal"
EngineLauncher.exe --build-project "$root/CalmCityFinal/Project.json"
$env:CALM_SAVE_TEST='save'; EngineRuntime.exe --project "$root/CalmCityFinal/Project.json" --mode game --frames 180
$env:CALM_SAVE_TEST='load'; EngineRuntime.exe --project "$root/CalmCityFinal/Project.json" --mode game --frames 180
```

Обе финальные сборки проекта вернули exit 0; save и load — exit 0. Лог отдельного процесса подтверждает сохранение позиции машины x=5.6, изменение до x=3.1 и восстановление x=5.6 с продолжением движения до x=5.35. Наблюдаемый кадр нового процесса — `evidence/save-load-new-process.bmp` (1000×500); там виден квартал, машина, свет, тени и LOD overlay. Ни normal-map tangent test, ни визуальная проверка интерпретации tangent basis не выполнялись.

При первой попытке чистой сборки в sandbox LINK завершился LNK1101 на версии MSPDB140.dll. Повтор той же `--enable-code` команды вне sandbox по поручению координатора вернул 0. Это сохранено в `evidence/sandbox-MSPDB-failed-build.log`; ошибка не классифицирована как дефект движка.

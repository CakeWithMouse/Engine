# Сборка и воспроизведение Curious City r6

## Проект

Архив `CuriousCity-r6-source.zip` содержит `Source/` и `Content/`; распакуйте их внутрь чистого проекта, созданного Launcher .6. Архив не содержит DLL, SDK, Intermediate и EngineStore.

Fresh project и мигрированная копия с пользовательским типом дома находятся в `Engine/build/playtests/20261114-r6/curious-city/`. Проверенная миграция: `Engine/build/playtests/20261107-r5/curious-city/final-city/Project.json` → `upgraded-city/`.

## Используемая сборка

Runtime и SDK: `E:/Engine/Game/build/x64/Release/Runtime`, версия `2026.11.07.6`. Не использовать предыдущую закреплённую SDK копию внутри старого playtest EngineStore.

```powershell
$runtime = 'E:/Engine/Game/build/x64/Release/Runtime'
$project = 'E:/Engine/Engine/build/playtests/20261114-r6/curious-city/upgraded-city/Project.json'
& "$runtime/SDK/ProjectTools.ps1" -Action Build -Runtime $runtime -Project $project
& 'E:/Engine/Game/build/x64/Release/EngineLauncher.exe' --check-project $project
```

## Игровые сценарии

`city-savegame-final.request.json` проходит 130 кадров с SaveGame cycle. Для приватных сохранений задайте перед запуском `CURIOUS_CITY_SAVE_ROOT` на каталог вне профиля пользователя, `CURIOUS_CITY_CYCLE=first` и `CURIOUS_CITY_LOG` на отдельный лог. `city-ui-save-load.request.json` вводит лучевой клик, кнопки Build/Save/Build/Load; его результаты `city-ui-final.result.json` и лог показывают восстановленные два дома.

Оба запроса передаются текущему инструменту:

```powershell
$tool = 'E:/Engine/Game/build/x64/Release/Runtime/EngineCommandlets.exe'
& $tool project.input <request.json> <result.json>
```

## Новые функции

Параметрические запросы `project-import-current-scale001.request.json`, `project-import-scale01.request.json`, `point-city.request.json`, `transparency/intersection.request.json`, `transparency/resize.request.json`, `material-graph.request.json` лежат в `evidence/`. Используйте текущий `EngineCommandlets.exe` .6 и сохраняйте выходы в новый каталог, чтобы BMP и JSON не перезаписались.


## Анимированные GPU сценарии

- `evidence/animated-point-shadow-skeletal.request.json` — `render.point-shadows`, `caster=skeletal`, sceneLighting, четыре времени 0/0.5/1/return, единая камера/лампа. Результат содержит face SHA-256 для сравнения.
- `evidence/animated-transparency-skeletal-crowd.request.json` — `render.transparency`, перекрывающиеся skeletal/crowd, орбита, near/far LOD, resize; материалы `Layer.mat`/`Layer.hlsl` находятся в `Engine/build/playtests/20261114-r6/curious-city/transparency/`.
- Результаты и D3D кадры с префиксами `animated-point-shadow-skeletal-` и `animated-transparency-skeletal-crowd-` лежат в `evidence/`.
- Команда для каждого JSON: `E:/Engine/Game/build/x64/Release/Runtime/EngineCommandlets.exe <render.point-shadows|render.transparency> <request.json> <result.json>`.
- Полные локальные выходы находятся в `E:/Engine/Engine/build/playtests/20261114-r6/curious-city/animated-features/`.

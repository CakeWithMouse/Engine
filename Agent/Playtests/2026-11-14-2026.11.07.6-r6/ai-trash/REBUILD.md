# REBUILD — AI Trash r6

Исходник r5: `Agent/Playtests/2026-11-07-2026.11.06.1-r5/ai-trash/ai-trash-r5-source.zip`, SHA256 `598CFD71AF4744F0B6415EE9E4DAAB44A1DE815A93D3AA7AFB918AE13761EA2E`. Реальный r5 проект, использованный для upgrade: `Engine/build/playtests/20261107-r5/ai-trash/TrashGame`; исходник не изменён.

Своя копия: `Engine/build/playtests/20261114-r6/ai-trash/TrashGameUpgraded`. Launcher и Runtime из `Game/build/x64/Release`; финальный SDK runtime — `Game/build/x64/Release/Runtime`. EngineStore/TEMP проекта изолированы в этой папке. Установка engine, fresh create/enable, upgrade, конечный текущий SDK build и project-check завершились exit 0. После копирования старых исходников в C++ проекте обновлены timestamps `.cpp/.h`; финальный `ProjectTools.ps1` build текущего Runtime собирал все игровые translation units и проверил DLL.

```powershell
EngineLauncher.exe --create-project <.../FreshTrash> 'AI Trash r6'
EngineLauncher.exe --enable-code <.../FreshTrash/Project.json>
EngineLauncher.exe --upgrade-project <.../20261107-r5/ai-trash/TrashGame/Project.json> <.../20261114-r6/ai-trash/TrashGameUpgraded>
& 'Game/build/x64/Release/Runtime/SDK/ProjectTools.ps1' -Action Build -Project <.../TrashGameUpgraded/Project.json> -Runtime 'Game/build/x64/Release/Runtime'
EngineLauncher.exe --check-project <.../TrashGameUpgraded/Project.json>
$env:TRASH_SAVE_TEST='save'; EngineRuntime.exe --project <.../TrashGameUpgraded/Project.json> --mode game --frames 180
$env:TRASH_SAVE_TEST='load'; EngineRuntime.exe --project <.../TrashGameUpgraded/Project.json> --mode game --frames 180
$env:TRASH_SAVE_TEST='types'; EngineRuntime.exe --project <.../TrashGameUpgraded/Project.json> --mode game --frames 420
EngineCommandlets.exe project.input <InputCheck/request.json> <InputCheck/result.json>
EngineCommandlets.exe material.graph <MaterialGraphFinalArtifacts/request.json> <MaterialGraphFinalArtifacts/result.json>
EngineCommandlets.exe project.import <ImportSettings/request.json> <ImportSettings/result.json>
```

Сборку финального проекта в sandbox блокировал известный MSVC PDB C1902; `ProjectTools.ps1` выполнялся с разрешённым внешним build. `CodeBuild.log`, конечный `build.log`, exit codes и выбранные логи/снимки скопированы в `evidence/`.

Каскадный опыт выполнен: `Battle.exe --units 1000 --camera 120 --no-point-light --camera-sequence <.../GpuChecks/cascade-dolly> --camera-motion dolly`; затем `python Engine/tests/AnalyzeShadowCameraSequence.py --check <.../GpuChecks/cascade-dolly>`. Exit 0, 121 frames, scaleChanges=0 для всех трёх каскадов; max fractional drift 5.12e-7 / 0 / 0. Полный набор BMP/matrices остаётся в build; report evidence хранит CSV, анализ и кадры 0/120.

Пересобираемый исходный архив: `ai-trash-r6-source.zip` рядом с этим файлом. Он включает только Source, Content и Project/lock/code/build metadata; Binaries, SDK, Intermediate, SaveData исключены.


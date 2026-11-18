# PainEngine 0.4.0 — проверка на Repair Corridor r5

## Текущий checkpoint

- Проверяемый диапазон `114b1b69..5458f022`, ветка `release/v0.4.0`. Работа только в личном scratch `E:\Engine\Engine\build\playtests\strategy-040` и в этом отчётном каталоге; чужие рабочие деревья и общие движки не менялись.
- Исходный r5 ZIP SHA256 `8AF67B7BD0B20F24830436187B5786C66B5AA8EAC498075626E6344AD85A7C03`. Полная старая копия обновлена штатным `EngineRuntime.exe --upgrade-project`; её lock теперь 0.4.0. Исходник сохранён.
- MSBuild Runtime 0.4.0: `E:\Engine\Game\build\x64\Release\Runtime`; EngineRuntime.exe SHA256 `D7EAFF9A1668CC93955CF9ACBCDC5B25580C47AD32737FAE5E28084C0FA96FCA`, Engine.lib `1D586BE7E2D3BD8DFA11B76B852CA01310546AF6D0CE872899838E1C84224FE8`. CMake Runtime: `E:\Engine\build\cmake\editor-Release\bin\Release\Runtime`; EngineRuntime.exe `58FFC0E6E1E70D3FC0B643079352B91851DF08FC78268D45ABB3799295E29B20`, Engine.lib `B794573162A49D4A05EE99E5F32CF38EE50461AA3632058231A5540F8762BBEF`.
- **Мои результаты:** MSBuild upgrade/build, standalone 60 кадров, экспорт и экспортированный Game.exe — passed. Автоматическое воспроизведение ввода `project.input` на реальной игре — exit0, отряд на месте, проход открыт, нарушений барьера нет; финальный исход `ПОРАЖЕНИЕ` ожидаем для r5 без атаки. Материальный commandlet import+13 операций passed после добавления требуемой отсутствующей external BMP в частную fixture-копию. Unicode CMake-проект собран elevated, DLL активирована (SHA256 `9950239CA7D356243C0016B3D15F88055991B67244727333200C4C569736EBD3`). D3D-кадр игры просмотрен.
- **Ограничение UI:** Editor запущен и создал окно `Main.scene.json — Ремонтный коридор — Engine Editor` (HWND 2295862), но CUA inventory его не перечислил, а `getApp(windowId)` сообщил, что окна нет среди доступных. Повторного долгого ожидания не делал; мои непосредственные AI dock/physics inspector/asset-picker взаимодействия частичные/не выполнены. Внешние native UiTests/camera/diagnostics результаты перечислены отдельно в `coordinator-evidence/manifest.json`, а не выдаются за мой GUI сеанс.
- У sandbox CMake был подтверждённый C1902 на try-compile; точная личная сборка повторена с разрешённым elevated доступом и прошла. Первичные неверные аргументы CLI, неудача source-only upgrade и первый импорт без external BMP оставлены как ошибки invocation/setup, не дефекты Engine.

## Что уже изучено и закрытые запросы

- CMake/SDK consumer и project workflow: `Engine/docs/CMAKE-BUILD.md`, `Runtime/SDK/ProjectTools.ps1`, `Runtime/SDK/CMakeProject.ps1`. Проверен только собственный Unicode project; общие CMake targets собирал координатор. Installed SDK consumer и Linux/macOS остаются внешними/не моими проверками.
- Standalone Game export: `Engine/docs/RELIABLE-WORKFLOW.md`; `Engine/BuildSupport/Export-Project.ps1 -Project <Project.json> -Destination <new scratch dir> -Runtime <confirmed Runtime>`. Мой экспорт в Unicode dir и запуск упакованного `Game.exe --frames 60` завершились exit0.
- Material fixture: `Engine/tests/assets/fixtures/auto-multiple.fbx` содержит 2 slots и требует внешнюю `maps/цвет.bmp`; в private fixture-копии создана self-authored one-pixel BMP. `MaterialSlotsCommandletTests.ps1` прошёл импорт и 13 операций, включая назначение slot, undo/redo, duplicate, save/reopen, whole-object override/clear и отказ traversal. Это API/scene evidence; ручное визуальное сравнение рендера двух частей не состоялось.
- Новое GUI покрытие UiTestRunner включает MaterialSlotList, WheelGuard, AssetPickerDialog, AiDiagnosticsDock, ObjectTypeInspector, InspectorCardStyle, InfoButton, Separator, HighDpiIcon, ContentBrowser и InspectorRefresh/point-light shadows; копии логов/источники/хеши сохранены в `coordinator-evidence/`.

## Итоговая граница

Release Runtime/SDK выданы и проверены по хешам. Координатор разрешил UI, но собственная CUA-сессия не смогла захватить созданное Editor окно; это честный partial, а не Engine defect. Общие тесты координатора — отдельные источники evidence. Новых запусков общего Engine/Editor/SDK не выполнял.

## Feature matrix

Статусы ниже относятся к возможностям диапазона; это не общая тестовая матрица движка. `passed` — проверено мной в частном проекте; `partial` — есть ограниченное личное либо координаторское evidence с явно указанным происхождением; `not-tested` — подтверждения нет. Блокированных на момент завершения личного прогона нет.

| Feature ID | Изменение в диапазоне | Статус | Критерий и свидетельство |
|---|---|---|---|
| CMAKE-PORTABLE-CORE | portable EngineCore/Generator CMake | partial | coordinator evidence: CMake workflow/tests; я проверил только Windows CMake project, Linux/macOS не запускались |
| CMAKE-DESKTOP-RUNTIME | CMake runtime/editor integration | partial | coordinator build evidence + мой project DLL build на CMake Runtime; отдельный standalone CMake runtime не запускал |
| CMAKE-INSTALLED-SDK | compiler-independent exported SDK | partial | coordinator SDK consumer evidence; личный consumer не собирал |
| CMAKE-PROJECT-BACKEND | project C++ transactional CMake build | passed | мой Unicode project: configure, 8 Ninja steps, DLL activation; `build-cmake-project-elevated.log` |
| CMAKE-UNICODE-HIDDEN-BUILD | Unicode paths в hidden MSVC/Ninja build | passed | `Коридор CMake Ёж`, CMake DLL activated; первый sandbox C1902 сохранён отдельно |
| CMAKE-LLD-LINK | duplicate Editor symbols при LLD | not-tested | отдельный Editor target, не game-module test; общего LLD build нет |
| CMAKE-CI-FAILURE-DETAILS | подробные ошибки CI | not-tested | release CI run не запускается ролью |
| CMAKE-WINDOWS-CI-EDITOR | Windows CMake CI включает Editor | not-tested | CI workflow только читаю |
| CMAKE-QT-SVG-BASE | SVG support из Qt base package | not-tested | package/CI infrastructure, только при наличии нового SDK в private CMake build |
| GAME-EXE-EXPORT | standalone executable bundle | passed | мой export в Unicode dir и экспортированный `Game.exe --frames 60`, exit0; `export-msbuild.*`, `export-game.*` |
| EDITOR-WINDOW-TASKBAR | taskbar/minimize для standalone Editor windows | partial | coordinator editor regressions; моя CUA не смогла подключиться к окну, ручное minimize не выполнено |
| LAUNCHER-ICON-FALLBACK | icon fallback для старых projects/releases | partial | coordinator UI/regression evidence, не проверено мною вручную на launcher |
| MATERIAL-SLOT-COMPACT-ROWS | compact Material card rows | partial | coordinator MaterialSlotList tests + private commandlet import/scene APIs; визуальный Inspector недоступен |
| MATERIAL-SLOT-DRAG-DROP | drag/drop `.mat` между rows | partial | coordinator UI regression; реальный drag/drop не выполнен вручную |
| MATERIAL-PART-ISOLATION | model parts изолированы slot materials | partial | import fixture содержит 2 слота; commandlet проверил slot assignment/undo/redo/duplicate/serialization, рендер каждого part не проверен |
| MATERIAL-CREATE-EDIT-ACTION | create/edit action у Material card | partial | coordinator UI regression; ручное состояние кнопки не наблюдалось |
| MATERIAL-RETENTION-ON-REPLACE | material retention при смене модели | partial | coordinator UI regression; личная замена модели не выполнена |
| AI-DIAGNOSTICS-DOCK | docking/layout диагностики | partial | coordinator UiTests + diagnostics API lifecycle; личный live dock-сеанс не доступен через CUA |
| AI-DIAGNOSTICS-LEGEND | legend/details/scroll | partial | coordinator UiTests; моя настоящая игра дала runtime diagnostics через API, overlay не снят в Editor UI |
| AI-DIAGNOSTICS-SELECTED-GROUP | live selected group/perception/filter | partial | coordinator diagnostics tests; моя gameplay API телеметрия: selected3, Arrived, open gate, barrier violations0 |
| CAMERA-GUIDE-METRICS | guide metrics table/control | partial | coordinator camera-guide native tests pass; личное взаимодействие в Editor не выполнено |
| PHYSICS-INSPECTOR-COMBO | начальное состояние physics inspector combo | partial | coordinator inspector tests pass; личный Inspector UI не наблюдался |
| ASSET-PICKER-UNIFIED | общий picker для Material/Script/AI machine | partial | coordinator AssetPickerDialog tests pass; личный picker не открывался |
| ASSET-PICKER-VFS-PATH | picker нормализует пути в `/Game/...` | partial | coordinator UI/integration evidence; личная проверка save/reopen отсутствует |
| ASSET-PATH-ELISION | длинный asset path не растягивает Inspector | partial | coordinator UI tests; личный кадр отсутствует |
| WHEEL-GUARD | wheel не меняет value controls случайно | partial | coordinator WheelGuard tests pass; ручной desktop wheel test не выполнен |
| INSPECTOR-TYPE-MATRIX | inspector card visibility by object type | partial | coordinator ObjectTypeInspector tests; ручной осмотр типов не выполнен |
| INSPECTOR-ADD-MENU | типизированная кнопка добавления component | partial | coordinator inspector tests; ручное добавление/reopen не проверено |
| INSPECTOR-CARD-STYLE | унификация карточек/строк schema | partial | coordinator InspectorCardStyle tests pass; личного визуального кадра нет |
| INFOBUTTON-CROSS-TYPE | короткие описания через единые Info buttons | partial | coordinator InfoButton tests; multi-card manual geometry не проверена |
| AI-CAMERA-INFO-SCROLL | AI panel + camera guide info/layout scroll fixes | partial | coordinator UiTests/camera guide tests; личное изменение размеров панели не выполнено |
| HIDPI-SVG-ICONS | devicePixelRatio для SVG icons | partial | coordinator HighDpiIcon test; 100/150/200% ручные дисплеи не проверялись |
| HIDPI-SEPARATORS | crisp separators на High-DPI | partial | coordinator Separator test; DPI>1 visual capture отсутствует |
| CONTENT-BROWSER-ARROWS | HiDPI folder branch arrows | partial | coordinator ContentBrowser tests; личный tree screenshot отсутствует |
| POINTLIGHT-SHADOW-PRESERVE | shadow сохраняется при редактировании light property | partial | coordinator InspectorRefresh/light tests; личная caster/light before/after сцена не снята |
| MISC-INSPECTOR-ADD-BACKLOG | backlog note про component Add button | not-tested | документационное пожелание, не feature pass |

## Финальный артефакт и проверка распаковки

- Source archive: `RepairCorridor-0.4.0-source.zip`, SHA256 `CF4474962B03DDB5C37ED4BD164441765E377886B546D7FA8CE20BF4A36BF6C0`, 22,587 bytes. Содержит `Game.sln`, `Game.vcxproj`, Source/Content и project metadata; SDK, Runtime, `Binaries` и `Intermediate` не включены.
- Архив распакован в свежую private папку `E:\Engine\Engine\build\playtests\strategy-040\fresh-unpack-msbuild`; MSBuild SDK 0.4.0 собрал и активировал DLL SHA256 `D1B2370E36E80A820EFBA38208AA39611570E7FB3673C713233D63BD7A39A893` (`fresh-build-msbuild.status`). Затем отдельный Runtime60 завершился exit0 с `GameUpdates=60` и `EngineStopped` (`fresh-runtime60.stdout.log`).
- Команды повторения собраны в `REBUILD.md`; журнал событий и ограничений — `diary.md` и `feedback.md`; feature-specific пути/статусы и происхождение — `evidence.json`.
- CUA/Editor процесс к финалу завершился; сейчас EngineRuntime окон не осталось. Связанные manual UI строки остаются partial, потому что inventory не показал доступное окно.

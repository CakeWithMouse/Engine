# Проверяемые исправления этого круга

Исходный Runtime .1 и промежуточные .2/.3/.4 сохранены отдельно. Финальный SDK .5:
`Engine/build/playtests/20261105-r2/final5/Runtime`. Рабочая поставка:
`Game/build/x64/Release/Runtime`. Публикация в EngineStore не выполнялась.

| Дефект / функция | Реальный вызов | Критерий / регрессия |
|---|---|---|
| BUG-R2-001 orthographic LOD | `render.diagnostics`, views/orthoHeight/screenHeights | одинаковый LOD при Y8/Y24, смена при zoom; RenderDiagnosticsCommandletTests |
| BUG-R2-002 disabled controller Load | SaveGame Capture/Restore + Input/CharacterController; игровой slot + новый процесс | неизменные yaw/позиции disabled, движение enabled; SaveGameTests |
| BUG-R2-003 mouse keys | `project.input`, cursor + key500 down/up | один дом/снаряд от клика; HudInputCommandletTests |
| BUG-R2-004 instance GPU placement | `render.diagnostics`, 3 разнесённых instances, panel:false | пиксели в трёх позициях и пустые промежутки; RenderDiagnosticsCommandletTests |
| BUG-R2-005 F7/F8 | Game shared input + реальный frame обоих hosts | press/hold/release и цикл по компонентам; RenderDiagnosticsTests |
| BUG-R2-006 primitive copies | CreateInstance + frame dump; отдельная игровая DLL через Runtime/project.input | три GPU позиции и пустые промежутки, Cube bare/material против Mesh; RenderDiagnosticsTests |
| BUG-R2-007 pending texture | scene assets + TrySetTexture до Game; Runtime/project.scene | HasTexture после attachment, GPU-пиксели Mesh/Instanced; AssetRenderTests + CuriousCity restart |
| Граф / отказ записи | `material.graph`, lockedFile на шаге revert | текущие правки сохраняются при отказе, повтор восстанавливает graph/HLSL; MaterialGraphCommandletTests |
| Сцена / история / группы | `project.scene`, разные steps | реальные объекты/ID/parent/позиции, undo/redo и Save As/New/Load; SceneCommandletTests |
| Held input и фокус | `project.input`, focus/key/frames | остановка при потере фокуса, отсутствие самопроизвольного повторного движения; ProjectInputCommandletTests |
| DLL hot reload | `project.reload-input` | 6 действий T/HUD до/после настоящей замены DLL; ReloadInputTests |

## Запуск из корня репозитория

```powershell
& Engine/tests/render-diagnostics/run.ps1 -Configuration Release
& Engine/tests/savegame/run.ps1 -Configuration Release
& Engine/tests/input/run.ps1 -Configuration Release -EnableAssimp
& Engine/tests/assets/run.ps1 -Configuration Release -EnableAssimp
& Editor/tests/RenderDiagnosticsCommandletTests.ps1
& Editor/tests/SaveGameCommandletTests.ps1 -SkipBuild
& Editor/tests/MaterialGraphCommandletTests.ps1 -Project '<scratch>/Project.json' -Material '/Game/Materials/WarmStone.mat'
& Editor/tests/SceneCommandletTests.ps1 -Project '<scratch>/Project.json'
& Editor/tests/ProjectInputCommandletTests.ps1 -Project '<bomber scratch>/Project.json' -ObjectName 'Bomber.Player'
& Editor/tests/HudInputCommandletTests.ps1 -Project '<city scratch>/Project.json' -SpawnType 'CuriousCity.House' -X 76 -Y 122
```

Полные JSON-контракты: `Engine/docs/COMMANDLETS.md` (также в SDK/Docs).
Проектные runners требуют предварительно собранную DLL соответствующей игры;
source-only архив не является запускаемым двоичным проектом.

Qt material commandlet использует скрытый native HWND для настоящего превью;
обычные UI commandlets работают offscreen. Полный EditorUiTests использует
Qt windows backend и новый неизменённый CLI template — старый post-run fixture
повторно не подходит. Для sandbox TEMP/TMP задаются на writable scratch.
Screenshot QWidget подтверждает форму/canvas, но не пиксели native swapchain;
игровые изображения получены Engine RequestFrameDump и отдельно осмотрены.

Файлы `*-before.log`/`*-before.bmp` сохранены как исходное воспроизведение;
последующие `*-after`/`*-final` не переписывают их. Ошибки собственного тестового
кода и настройки хоста перечислены в `../issues.md` отдельно от Engine defects.

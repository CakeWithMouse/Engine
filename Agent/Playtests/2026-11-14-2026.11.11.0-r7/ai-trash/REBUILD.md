# Rebuild — AI Trash r7

**Engine:** PainEngine 2026.11.11.0 Release, localOnly v143. **Engine.lib SHA-256:** `87F34A76ABF802F85264E20AF0897C85BE3B18C32D77E12DBDD91CAAE18B6527`. Использовать именно `E:\Engine\Game\build\x64\Release\Runtime`; в `Engine.lock.json` версия закреплена как .11.11.0. Private launcher trials использовали `PAINENGINE_ENGINE_STORE=E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\EngineStore`.

## Build и standalone run

Исходники проекта находятся в `TrashGameUpgraded/` внутри `ai-trash-r7-source.zip`. Архив содержит `Source`, `Content`, Project/lock/code manifest и текстовые `Game.sln`/`Game.vcxproj`; нет SDK, DLL, Binaries, Intermediate, SaveData или EngineStore.

```powershell
$zip = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\ai-trash\ai-trash-r7-source.zip'
$dest = 'E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\FreshArchive'
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project "$dest\TrashGameUpgraded\Project.json" -Runtime $runtime -Rebuild
& "$runtime\EngineRuntime.exe" --project "$dest\TrashGameUpgraded\Project.json" --mode game --frames 20
```

Обе команды прошли на свежем распакованном корне, Build exit 0 и 20 игровых кадров exit 0. Логи и список файлов распаковки: `evidence/source-archive-rebuild/`.

После HUD-правки ZIP обновлён и повторно собран из чистого извлечения; Build и standalone 20-frame run завершились с exit 0. Обновлённый source SHA-256: `8AC62D2F619FE83A1D8430611D19C1BA27167F16400E3B6F107D79A256CECBC1`. Проверка извлечения, сборки и запуска сохранена в `evidence/source-archive-hud-fix/`.

### Проверка HUD на двух размерах

`Source/ProjectGame.cpp` задаёт короткую подсказку `Aim: <value> m | L/R; T/Space toss`. Команда `evidence/RunHudResolutionCapture.ps1` запускает `project.input` на основной сцене в двух отдельных процессах с размерами viewport 1000×600 и 640×480; тест запускает все три вида предметов и сохраняет кадр 410. В обоих результатах фактический viewport и BMP имеют ожидаемый размер, а полная подсказка остаётся внутри панели. Перед правкой сохранён `evidence/trash-three-items-before-hud-fix.bmp`; новые кадры и результаты — `evidence/trash-hud-1000x600.*` и `evidence/trash-hud-640x480.*`.

## Fresh create/enable, upgrade, SaveGame

В проверке launcher установить SDK .11.11.0 в собственный EngineStore, создать проект в новом каталоге и включить C++ отдельным действием:

```powershell
$launcher = 'E:\Engine\Game\build\x64\Release\EngineLauncher.exe'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$env:PAINENGINE_ENGINE_STORE = 'E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\EngineStore'
$freshParent = 'E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\FreshTrashParent'
$freshProject = Join-Path $freshParent 'FreshTrashReplay'
$upgradeRoot = 'E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\TrashGameUpgradedReplay'
New-Item -ItemType Directory -Force -Path $freshParent | Out-Null
function Invoke-Launcher([string[]]$CommandArgs) {
  $proc = Start-Process -FilePath $launcher -ArgumentList $CommandArgs -Wait -PassThru -WindowStyle Hidden
  if ($proc.ExitCode -ne 0) { throw "EngineLauncher failed ($($proc.ExitCode)): $($CommandArgs -join ' ')" }
}
Invoke-Launcher @('--install-engine', ('"'+$runtime+'"'))
Invoke-Launcher @('--create-project', ('"'+$freshProject+'"'), '"AI Trash r7"')
Invoke-Launcher @('--enable-code', ('"'+(Join-Path $freshProject 'Project.json')+'"'))
Invoke-Launcher @('--upgrade-project', ('"E:\Engine\Engine\build\playtests\20261114-r6\ai-trash\TrashGameUpgraded\Project.json"'), ('"'+$upgradeRoot+'"'))
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project (Join-Path $upgradeRoot 'Project.json') -Runtime $runtime -Rebuild
Invoke-Launcher @('--check-project', ('"'+(Join-Path $upgradeRoot 'Project.json')+'"'))
```

Source ZIP SHA-256: `8AC62D2F619FE83A1D8430611D19C1BA27167F16400E3B6F107D79A256CECBC1` (after HUD cleanup). The source r6 project was preserved. `UpgradeReport.json` reports `originalPreserved:true` and .11.11.0. Create, enable, upgrade, build and check exit evidence is in `evidence/`.

For the standalone score/save checks, set `TRASH_SAVE_TEST=save`, run the Runtime on `Main.scene.json` for 180 frames, then run a **separate process** with `TRASH_SAVE_TEST=load` for 180 frames. The saved in-flight projectile reloads at ~0.5s and later increments hits to 1. `TRASH_SAVE_TEST=types` for 420 frames launches all three kinds and captures the actual HUD frame to project SaveData.

The optional AI editor side fixture is `Content/Scenes/TrashAIPractice.scene.json` with `Content/AI/TrashSorter.fsm`. Its real Qt state/link dialog, save/reopen and Play trace are in `evidence/ai-side-play.*`; the project manifest is left on the normal Trash Toss Main scene.





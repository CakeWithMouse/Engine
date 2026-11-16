# Calm City r7 rebuild and reproduction

Project: `Engine/build/playtests/20261114-r7/calm-city/CalmCityUpgraded/Project.json`  
Private Runtime/SDK: `Game/build/x64/Release/Runtime`  
Engine: 2026.11.11.0, Release, localOnly v143; `Engine.lib` SHA-256 `87F34A76ABF802F85264E20AF0897C85BE3B18C32D77E12DBDD91CAAE18B6527`.

## Create and upgrade

Run from `E:\Engine` with the matching launcher/runtime:

```powershell
$launcher = 'E:\Engine\Game\build\x64\Release\EngineLauncher.exe'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$env:PAINENGINE_ENGINE_STORE = 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\EngineStore-current-2026.11.11.0'
function Invoke-Launcher([string[]]$CommandArgs) {
  $proc = Start-Process -FilePath $launcher -ArgumentList $CommandArgs -Wait -PassThru -WindowStyle Hidden
  if ($proc.ExitCode -ne 0) { throw "EngineLauncher failed ($($proc.ExitCode)): $($CommandArgs -join ' ')" }
}
Invoke-Launcher @('--install-engine', ('"'+$runtime+'"'))
$freshRoot = 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\FreshCalmCityReplay'
Invoke-Launcher @('--create-project', ('"'+$freshRoot+'"'), '"Calm City r7"')
Invoke-Launcher @('--enable-code', ('"'+(Join-Path $freshRoot 'Project.json')+'"'))
Invoke-Launcher @('--upgrade-project', ('"E:\Engine\Engine\build\playtests\20261114-r7\calm-city\LegacyCalmCityInput\Project.json"'), ('"E:\Engine\Engine\build\playtests\20261114-r7\calm-city\CalmCityUpgradedReplay"'))
```

The `FreshCalmCity` project has successful create/enable exit files and its `CodeBuild.log` records the first C++ build. Upgrade output and its exit status are in `upgrade.stdout.txt` and `upgrade.exitcode.txt` in the scratch folder. Do not use the separate `FreshCalmCityNoCode` for building: it was created by a launcher resolving an unintended .11.11.1 store. Its pre-enable inventory is still valid evidence for the no-code state; the mismatch was rejected before building.

## Build the upgraded project

The final game build was run explicitly against the .11.11.0 Runtime SDK:

```powershell
& 'E:\Engine\Game\build\x64\Release\Runtime\SDK\ProjectTools.ps1' `
  -Action Build `
  -Project 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\CalmCityUpgraded\Project.json' `
  -Runtime 'E:\Engine\Game\build\x64\Release\Runtime' `
  -Rebuild
```

The validated module is recorded by `CalmCityUpgraded/Project.code.json`; final build log and exit evidence are in the report’s `evidence/savegame-portable` directory.

## Relocated SaveGame and frame run

Copy the project’s `Content`, `Project.json`, `Project.code.json` and current `Binaries` into a new project root, then run two separate processes:

```powershell
$env:CALM_FRAME_DUMP = '1'
$env:CALM_INSTANCE_TEST = '1'
$env:CALM_SAVE_TEST = 'save'
& 'E:\Engine\Game\build\x64\Release\Runtime\EngineRuntime.exe' --project 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\RelocatedCalmCity\Project.json' --mode game --frames 20
$env:CALM_SAVE_TEST = 'load'
& 'E:\Engine\Game\build\x64\Release\Runtime\EngineRuntime.exe' --project 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\RelocatedCalmCity\Project.json' --mode game --frames 20
```

Logs show `/Game` mounted from `RelocatedCalmCity/Content`; `SaveGameSelfTest.log` records the `CarMover` and score restore, and `manual-1.save.json` contains the saved `/Game` material-slot paths. Frame and instance logs are written under that relocated root via `EngineModuleVfs::Resolve("/Game")`.

## Commandlet checks

Commandlet contract and captured requests/results are under `evidence/`. The principal custom commands were:

```powershell
EngineCommandlets.exe render.point-shadows <skeletal-animation.json> <skeletal-animation.json-result>
EngineCommandlets.exe render.point-shadows <point-shadow-lod-transition-v2.json> <point-shadow-lod-transition-v2-result>
EngineCommandlets.exe render.transparency <transparency-crowd-lod-m0.json> <transparency-crowd-lod-m0-result>
EngineCommandlets.exe render.transparency <transparency-crowd-animation-m0-v2.json> <transparency-crowd-animation-m0-v2-result>
python evidence/transparency-oracle/VerifyTransparencyOracle.py
python evidence/transparency-crowd-lod/VerifyTransparencyCrowd.py
```

Requests and result JSON preserve complete output image paths; representative BMPs and analysis scripts are copied into the report. Camera cascade analysis used `Engine/tests/AnalyzeShadowCameraSequence.py` on the role’s `cascade-orbit` matrices CSV.

## Source archive

`calm-city-r7-source.zip` contains the game’s Source, Content, project metadata and role analysis/helper sources. It excludes Runtime/SDK, EngineStore, Binaries, Intermediate, SaveData and commandlet executables.

## Fresh source archive build and run

Extract `calm-city-r7-source.zip` to a new folder, then build with the pinned Runtime explicitly. This archive includes `Game.sln` and `Game.vcxproj` as well as Source, Content and project metadata; it contains no SDK or compiled binaries. The fresh extraction at `Engine/build/playtests/20261114-r7/calm-city/ArchiveRebuildCheck` rebuilt and launched for 20 frames using the local 2026.11.11.0 Runtime. Logs, exit codes and the initial missing-project-files packaging failure are in `evidence/source-archive-rebuild/`.

```powershell
$zip = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\calm-city\calm-city-r7-source.zip'
$dest = 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\FreshArchive'
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project "$dest\CalmCityUpgraded\Project.json" -Runtime $runtime -Rebuild
& "$runtime\EngineRuntime.exe" --project "$dest\CalmCityUpgraded\Project.json" --mode game --frames 20
```



Default launch is restored to Content/Scenes/Main.scene.json; specialized import/shadow scenes remain side fixtures.




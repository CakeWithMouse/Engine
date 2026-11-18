# Rebuild and replay — Repair Corridor r2

Проект собирался локальными Runtime/SDK Engine 0.4.0 из финального PR31 выпуска. Архив содержит исходники/Content, но не SDK, runtime, DLL, `Binaries`, `Intermediate` или `SaveData`.

## Зафиксированные SDK

| Backend | Runtime | EngineRuntime.exe SHA256 | Engine.lib SHA256 | Дополнительно |
|---|---|---|---|---|
| MSBuild Release | `E:\Engine\Game\build\x64\Release\Runtime` | `BBCCF2A943C902185A397AA055F2099F8672ECC1BE6AB8B130767F075E69F674` | `D288D5DF2ACEDE3836A0D89AFD2715F3AC8C318C36E68D7F159100F8530449B3` | EngineCommandlets.exe `22A1875991225DA24D44C56251B6E368572D8D5386AAD46409CA3CF8CB1D4C76`; MSVC 14.44, v143, Release, x64, `/MD`. |
| CMake Release | `E:\Engine\build\cmake\editor-Release\bin\Release\Runtime` | `CD8F4218326D91DE9322FFBA2BA093A021F20A2CA7161545442AE796C036F795` | `A4288DC061C10C16B86470C85B97BA79C2342E4369F5BFDAC739E987DEB520CB` | `Sdk.json.buildSystem=cmake`, Engine 0.4.0. |

Эти SDK нельзя подменять ранее выданными пакетами той же версии: PR31 исправления `REL040R2-001/002/003` вошли в перечисленные финальные хеши. CMake SDK с указанным hash вошёл в релизный ZIP; MSBuild SDK использовался отдельно для личных проверок и не поставляется с релизом.

## Source archive

Архив: `RepairCorridor-0.4.0-r2-source.zip`; SHA256 `111B211E3AF7B5FD52879DE12DCC55E46B625A3EA3EA10A591B75DAD97B56321`; 36 entries (35 файлов и директория `Content/Models`), 28,424 байта. Integrity inventory — `evidence/source-archive-files.json`. Архив содержит `Game.sln`, `Game.vcxproj`, `CMakeLists.txt`, `Source`, `Content`, `Engine.lock.json` и README в корне архива. В нём нет `Binaries`, `Intermediate`, `SaveData`, Engine SDK или compiled libraries.

Распаковка в ранее не существовавший каталог и проектный manifest для команд ниже:

```powershell
$Archive = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\RepairCorridor-0.4.0-r2-source.zip'
$ProjectRoot = 'E:\Engine\Engine\build\playtests\strategy-040-r2\fresh-final-cmake\RepairCorridor'
New-Item -ItemType Directory -Force -Path (Split-Path $ProjectRoot) | Out-Null
Expand-Archive -LiteralPath $Archive -DestinationPath $ProjectRoot
$Project = Join-Path $ProjectRoot 'Project.json'
```

## Личная сборка проекта

Из корня checkout команда на финальном MSBuild SDK:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$Project = Join-Path $ProjectRoot 'Project.json'
& "$Runtime\SDK\ProjectTools.ps1" -Action Build -Project $Project -Runtime $Runtime -Rebuild
```

Для CMake SDK аналогично, задав CMake tools из релизного checkout в `PATH`:

```powershell
$env:PATH = 'E:\Engine\Engine\build\tools\python\cmake\data\bin;E:\Engine\Engine\build\tools\python\bin;' + $env:PATH
$Runtime = 'E:\Engine\build\cmake\editor-Release\bin\Release\Runtime'
$Project = Join-Path $ProjectRoot 'Project.json'
& "$Runtime\SDK\ProjectTools.ps1" -Action Build -Project $Project -Runtime $Runtime -Rebuild
```

Обе финальные личные сборки завершились сообщением `Build validated and activated`. Fresh source extraction CMake build завершился тем же результатом; active DLL SHA256 `FB565E3D8CEAA7580D03D27F3F5C8EB8AE9CA7CAD5EB012A7D63D7AB187EA9AD`.

## Game Play replay

Запрос `evidence/fresh-cmake-play-request.json` является копией прежнего r5 project.input сценария с путём свежей распаковки. Он запускается MSBuild EngineCommandlets против CMake-собранной DLL:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$Request = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\fresh-cmake-play-request.json'
$Result = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\fresh-cmake-play-result.json'
$env:PAINENGINE_STRATEGY_TELEMETRY = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\fresh-cmake-play-telemetry.log'
$env:PAINENGINE_D3D_SCREENSHOT = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\fresh-cmake-play-frame.bmp'
$env:PAINENGINE_D3D_SCREENSHOT_FRAME = '150'
& "$Runtime\EngineCommandlets.exe" project.input $Request $Result
$code = $LASTEXITCODE
Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY,Env:PAINENGINE_D3D_SCREENSHOT,Env:PAINENGINE_D3D_SCREENSHOT_FRAME -ErrorAction SilentlyContinue
if ($code -ne 0) { throw "project.input failed: $code" }
```

Итог: 1241 frames, commandlet `exit=0`; mesh gate and dynamic crate bodies exist; crate passes the arch and stops at z≈5.52. The old squad route completes its order, then guard capture yields `lost=1`. `barrierViolationFrames=0` относится к препятствию ServiceBarrier старого приказа, а не к MeshGate. The run is not a win and the squad does not cross the arch in this old scenario.

## Mesh persistence host

`MeshPersistenceHost.vcxproj` and `mesh-persistence-host.cpp` are included beside this document. Copy both into a private helper folder before compiling, because the vcxproj includes its adjacent source file. This avoids dependence on the original project scratch:

```powershell
$Runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$HostRoot = 'E:\Engine\Engine\build\playtests\strategy-040-r2\fresh-final-cmake\MeshPersistenceHost'
New-Item -ItemType Directory -Force -Path $HostRoot | Out-Null
Copy-Item 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\MeshPersistenceHost.vcxproj' $HostRoot
Copy-Item 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\mesh-persistence-host.cpp' $HostRoot
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' (Join-Path $HostRoot 'MeshPersistenceHost.vcxproj') /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:RuntimeSdkInclude=$Runtime\SDK\Include"
```

Run against the rebuilt project DLL, content root and self-authored fixture OBJ files:

```powershell
$manifest = Get-Content -Raw "$ProjectRoot\Project.json" | ConvertFrom-Json
$GameDll = Join-Path $ProjectRoot $manifest.module
$MeshHostExe = Join-Path $HostRoot 'out\MeshPersistenceHost.exe'
$Fixtures = 'E:\Engine\Engine\build\playtests\strategy-040-r2\fixtures'
New-Item -ItemType Directory -Force -Path $Fixtures | Out-Null
Copy-Item 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\StaticMeshArch.obj' $Fixtures
Copy-Item 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\OpenCargo.obj' $Fixtures
& $MeshHostExe $GameDll $Runtime "$ProjectRoot\Content" $Fixtures
```

Expected output: `Mesh physics persistence PASS`; imported model assets and static/dynamic Mesh PhysicsObject settings survive save, destroy/create of edit session, and reload. The helper does not simulate the reopened imported scene.

## Fresh runtime and export

The fresh CMake build was launched by the actual CMake Runtime for 60 frames:

```powershell
& 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\run-runtime-smoke.ps1' `
  -Runtime 'E:\Engine\build\cmake\editor-Release\bin\Release\Runtime' `
  -Project 'E:\Engine\Engine\build\playtests\strategy-040-r2\fresh-final-cmake\RepairCorridor\Project.json' `
  -Frames 60 -OutputPrefix 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\fresh-cmake-runtime60'
```

Actual result: `GameUpdates=60`, `EngineStopped`, exit 0. MSBuild Runtime export and packaged `Game.exe` smoke also passed:

```powershell
& 'E:\Engine\Engine\BuildSupport\Export-Project.ps1' `
  -Project 'E:\Engine\Engine\build\playtests\strategy-040-r2\RepairCorridor\Project.json' `
  -Destination 'E:\Engine\Engine\build\playtests\strategy-040-r2\export-msbuild-040' `
  -Runtime 'E:\Engine\Game\build\x64\Release\Runtime'
& 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\run-exported-game-smoke.ps1' `
  -GameExe 'E:\Engine\Engine\build\playtests\strategy-040-r2\export-msbuild-040\Game.exe' `
  -Frames 60 -OutputPrefix 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\evidence\exported-game60'
```

Raw CMake build-tree Runtime does not contain `GameExecutable.exe`, so its direct export attempt returned `LaunchError: Build or install the matching Release runtime with game export support first.` This is a documented build-tree/package distinction, not a packaged Engine defect. The coordinator separately verified the packaged CMake Runtime export path; see `../2026-11-18-0.4.0-r2-release/report.md`.

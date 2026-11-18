# Повтор сборки и прогона

Проектная копия с актуальным lock находится в source archive `RepairCorridor-0.4.0-source.zip`, SHA256 `CF4474962B03DDB5C37ED4BD164441765E377886B546D7FA8CE20BF4A36BF6C0`. Архив не содержит SDK, Runtime, DLL, `Binaries` или `Intermediate`. Для сборки нужен выданный Engine 0.4.0 SDK; проверенные библиотеки этой сессии:

- MSBuild Release: `E:\Engine\Game\build\x64\Release\Runtime\SDK\Lib\Engine.lib`, SHA256 `1D586BE7E2D3BD8DFA11B76B852CA01310546AF6D0CE872899838E1C84224FE8`.
- CMake Release: `E:\Engine\build\cmake\editor-Release\bin\Release\Runtime\SDK\lib\Engine.lib`, SHA256 `B794573162A49D4A05EE99E5F32CF38EE50461AA3632058231A5540F8762BBEF`.

Распаковать архив в новую папку. Команды PowerShell (поменять `$game` и Runtime на действительные пути):

```powershell
$game = 'E:\Games\Repair Corridor 0.4'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$env:PAINENGINE_ENGINE_STORE = 'E:\Games\Repair Corridor 0.4\EngineStore'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project "$game\Project.json" -Runtime $runtime -Rebuild
& "$runtime\EngineRuntime.exe" --project "$game\Project.json" --mode game --frames 60
```

Экспорт проекта в свежую папку и запуск standalone EXE:

```powershell
$destination = 'E:\Games\Repair Corridor 0.4 Export'
& 'E:\Engine\Engine\BuildSupport\Export-Project.ps1' -Project "$game\Project.json" -Destination $destination -Runtime $runtime
& "$destination\Game.exe" --frames 60
```

Чтобы повторить CMake project build, выберите CMake Runtime с совпадающим 0.4.0 lock, установите PATH из локальных CMake/Ninja tools, и используйте SDK ProjectTools:

```powershell
$runtime = 'E:\Engine\build\cmake\editor-Release\bin\Release\Runtime'
$env:PATH = 'E:\Engine\Engine\build\tools\python\cmake\data\bin;E:\Engine\Engine\build\tools\python\bin;' + $env:PATH
$env:PAINENGINE_ENGINE_STORE = 'E:\Games\Repair Corridor 0.4\EngineStore-CMake'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project "$game\Project.json" -Runtime $runtime -Rebuild
```

Если try-compile падает `C1902` только в sandbox, сначала сохраните ошибку. В этом прогоне exact private CMake build прошёл с повышенным доступом из-за блокировки PDB. Не переключайте toolchain и не меняйте ABI/include paths вручную.

Автоматическое воспроизведение gameplay через `EngineCommandlets.exe project.input <request.json> <result.json>` и `PAINENGINE_STRATEGY_TELEMETRY=<new log>` описано в `Engine/docs/COMMANDLETS.md`. Для источника этого сценария см. предыдущую r5 evidence; JSON должен ссылаться на текущий `Project.json`.

Фактическая свежая распаковка этого ZIP в `Engine/build/playtests/strategy-040/fresh-unpack-msbuild` собрана и запущена на 60 кадров; лог/статус приведены в `fresh-build-msbuild.*` и `fresh-runtime60.*`.

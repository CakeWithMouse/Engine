# Rebuild and replay: Calm City continuation

All commands use the explicit current Runtime, never the historical private store. Working project and outputs are in `Engine/build/playtests/20261114-r6/calm-city/continuation/CityApiProbe`; the report is under `Agent/Playtests/2026-11-14-2026.11.07.6-r6/calm-city/continuation`.

```powershell
$runtime = (Resolve-Path 'Game/build/x64/Release/Runtime').Path
$project = (Resolve-Path 'Engine/build/playtests/20261114-r6/calm-city/continuation/CityApiProbe/Project.json').Path
& "$runtime/SDK/ProjectTools.ps1" -Action Build -Project $project -Runtime $runtime
```

Rebuild is a project-only build. On this host MSVC PDB writes failed inside the sandbox (LNK1101/MSPDB140); previous coordinator authorization allowed the escalated host build. This does not touch the shared Engine or SDK.

Examples of individual runs (input/result files in the sibling build output):

```powershell
$tool = (Resolve-Path 'Game/build/x64/Release/Runtime/EngineCommandlets.exe').Path
& $tool asset.import '<scratch>/import-texture.request.json' '<scratch>/import-texture.result.json'
& $tool render.mesh '<scratch>/mesh-normal-two-sided.request.json' '<scratch>/mesh-normal-two-sided.result.json'
& $tool project.input '<scratch>/hud-events.request.json' '<scratch>/hud-events.result.json'
& 'Engine/BuildSupport/Export-Project.ps1' -Project $project -Destination '<scratch>/CityProbeExport' -Runtime $runtime
```

`CityApiProbe/Source/ProjectGame.cpp` contains the parameterized runtime assertions and environment-controlled light/sky/camera frame capture. `evidence.json` indexes each feature by the exact matrix `feature_id`; evidence paths are relative to this report directory. `calm-city-continuation-source.zip` contains source/project/content only and excludes SDK, binaries and intermediate output.

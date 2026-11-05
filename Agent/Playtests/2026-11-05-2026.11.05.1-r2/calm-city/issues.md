# Issues

## Playtest command typo: short command aliases are rejected
- Action/time: 2026-11-05, direct EngineCommandlets probes in `Engine/build/playtests/20261105-r2/calm-city/commandlets`.
- Command: `EngineCommandlets.exe mesh-primitive <request.json> <result.json>` (repeated with `camera-screen-ray`, `scene-property`, `gizmo-snap`, `game-settings`, `session-lifecycle`, `trace-raycast`).
- Expected: documented API probe runs and writes measured result JSON.
- Actual: every call returned `{"ok":false,"error":"unknown command; use --list"}`.
- Exit: 2 for each call.
- Log: `Engine/build/playtests/20261105-r2/calm-city/commandlets/*.log`.
- Category: user-code-error (I used descriptive labels instead of the exact identifiers printed by `--list`).
- Follow-up: retry using exact identifiers from `EngineCommandlets.exe --list`.

## Report packaging path typo
- Action/time: 2026-11-05, packaging evidence for role1.
- Command: `Copy-Item CalmCityDev/SaveData/manual-1.save.json ...`.
- Expected: include the generated slot in the report evidence folder.
- Actual: source path was wrong; SaveGame stores it under `SaveData/calm-city/default/manual-1.save.json`.
- Exit: PowerShell `Copy-Item` error (the packaging command stopped before archive creation; previous log/frame copies succeeded).
- Log: current console/tool output; source slot verified at `Engine/build/playtests/20261105-r2/calm-city/CalmCityDev/SaveData/calm-city/default/manual-1.save.json`.
- Category: user-code-error.
- Follow-up: retry with the actual nested slot path.

Retry outcome: exact `--list` names (`mesh.primitive`, `camera.screen-ray`, `scene.property`, `gizmo.snap`, `game.settings`, `session.lifecycle`, `trace.raycast`) all exited 0. Their request/result/log triples are in `evidence/commandlets/`. Packaging succeeded after correcting the slot path; archive created.

## Diagnostic CreateInstance probe build failures (scratch-only, corrected)
- Action/time: 2026-11-05, isolated `CalmCityInstanceProbe` C++ visual reproduction setup.
- Command: launcher `--build-project` first returned `Cannot create CodeBuild.log` (exit 1); direct PowerShell 5.1 `Build.ps1` returned `Get-FileHash is not recognized` (exit 1); official `Invoke-MSBuild.ps1` then reached MSVC but reported `LNK1101: неправильная версия MSPDB140.DLL` (exit 1).
- Expected: build the scratch-only reproduction through installed .3 SDK.
- Actual: environment/toolchain mismatch before C++ diagnostics; switching to project `Build.ps1` with the frozen .3 Runtime reached the compiler. First compile attempt then exposed my `unique_ptr<Derived>` / `unique_ptr<GameComponent>` API misuse (C2664), and second attempt exposed calling `SetMesh` through the base pointer (C2039). Both source errors were corrected locally; no Engine/SDK files changed.
- Exit/logs: launcher stdout recorded in tool output; `Engine/build/playtests/20261105-r2/calm-city/CalmCityInstanceProbe/CodeBuild.log`, and `.../Binaries/Release/930a4a4d4b0843e28667c9c893546818/build.log`.
- Category: setup/toolchain errors followed by corrected user-code errors; not a confirmed Engine defect.
- Follow-up: official project Build.ps1 with `.3/Runtime`; run frame only after successful build.

## BUG-R2-006: Legacy CubeComponent CreateInstance submits an incorrect visual batch
- Action/time: 2026-11-05, isolated `CalmCityInstanceProbe`, frozen Runtime/SDK 2026.11.05.3.
- Command: `EngineRuntime.exe --project CalmCityInstanceProbe/Project.json --mode game --frames 4`, twice with `INSTANCE_PROBE_MODE` unset (`CubeComponent`) and `mesh` (`MeshComponent` control); both use orthographic top-down `SceneCamera`, same 3 positions X=-3/0/3, scale 1, colors red/green/blue, no assigned material, actual D3D11 device NVIDIA GeForce RTX 4070 Laptop GPU.
- Expected: both component APIs render three distinct cube copies at their requested positions/colors.
- Actual: `MeshComponent` control shows three distinct silhouettes at matching horizontal positions. Bare `CubeComponent` shows a large, incorrectly filled green surface and lacks the three expected distinct silhouettes. This is confirmed visually (not inferred from counters); root reproduced the same failure independently. No Engine/SDK workaround was applied in this project.
- Exit: 0 in both runtime processes. Logs: `evidence/r3/instance-legacy-r3.log`, `evidence/r3/instance-mesh-r3.log`. Frames: `evidence/r3/instance-legacy-topdown-r3.bmp`, `evidence/r3/instance-mesh-topdown-r3.bmp`.
- Category: confirmed Engine rendering defect reproduced independently by root; legacy primitive `GameComponent::CreateInstance` path. MeshComponent control succeeds. Root is preparing the .4 fix; this .3 probe remains the before record.
- Reproduction source: `Engine/build/playtests/20261105-r2/calm-city/CalmCityInstanceProbe/Source/ProjectGame.cpp`; isolated project manifest and .3 build log under same scratch folder.



### BUG-R2-006 post-fix retest on frozen Runtime/SDK .4
- The identical clean CLI C++ probe was created and built separately using final4 launcher/Runtime, with no primitive/material workaround in the bare Cube path. Both 30-frame runs exited 0 and wrote 1000×500 D3D BMPs (`written=1`).
- Result: bare Cube and MeshComponent control each show three separated silhouettes at X=-3/0/3. The .3 before-fix frame remains in `evidence/r3/`; post-fix .4 frames/logs are in `evidence/r4/`.
- Status: confirmed defect fixed in .4 for this exact no-material Cube scenario; no broader geometry/color claim beyond these screenshots.

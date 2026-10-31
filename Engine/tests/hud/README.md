# HUD regression checks

Run from the repository root on Windows with Visual Studio C++ and the Windows SDK.

```powershell
Engine/tests/hud/run.ps1
Engine/tests/hud/export.ps1
```

The first suite verifies XML round trips, named callbacks after reload, malformed
updates, optional project settings, repeated Play/Stop, real D3D11 WARP rendering,
shader invalidation, embedded pointer input and gameplay input suppression. It
needs an interactive Windows desktop for GetCursorPos. The export suite creates
an isolated project, exports it, temporarily moves its source directory and
launches the packaged game; it also rejects missing and escaping HUD paths.

The visual editor flow is part of `Editor/tests/EditorUiTests.cpp`. Build with:

```powershell
Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Debug -EditorUiTest
```

Run `Game/build/x64/Debug/Runtime/EditorUiTests.exe` with a newly created disposable
project directory and the runtime EngineContent directory. It creates and edits
the HUD using the actual Qt widgets, tests the preview and saves the HUD selection.
Run the same command in a new process with an additional `--hud-restart` argument
to verify that the selection survives restarting, select None, and check Play/Stop.
The Qt tests require normal desktop/registry access for editor preferences.

Validated for 2026.10.31.3: 52 HUD checks, 54 existing Input/Character checks,
the full editor UI suite, a separate HUD restart/None run, Debug and Release
launcher builds, Battle Debug build, and portable exported-game launch.

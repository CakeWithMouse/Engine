# AI viewport diagnostics fixture

The fixture uses the actual scene editor, NavVolume bake, AI state machine, movement, perception and a nine-member group. It is a test project, not a runtime dependency.

From the repository root (PowerShell), build the fixture and editor UI test using Engine/BuildSupport/Invoke-MSBuild.ps1 with Debug configuration:

```powershell
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Engine/tests/ai/diagnostics/DiagnosticsModule.vcxproj -Configuration Debug
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Debug -EditorUiTest
$fixtureOutput = Join-Path $env:TEMP ('AiDiagnostics-' + [guid]::NewGuid())
New-Item -ItemType Directory $fixtureOutput | Out-Null
Copy-Item Engine/tests/ai/diagnostics/Fixture/* $fixtureOutput -Recurse
Copy-Item Engine/build/x64/Debug/AiDiagnosticsFixture/*.dll $fixtureOutput
$env:PATH = (Join-Path $PWD 'Engine/ThirdParty/Qt6.7/bin') + ';' + $env:PATH
& ./Game/build/x64/Debug/Runtime/EditorUiTests.exe $fixtureOutput (Join-Path $PWD 'Engine/Content') --ai-diagnostics
```

For an interactive screenshot pass only, set AI_DIAGNOSTICS_CAPTURE=1. The test writes capture-ready.txt and waits up to five minutes for capture-done.txt to contain the same stage name. Capture the actual visible editor window before acknowledging each stage. Without that variable the test runs unattended. Keep generated files outside Fixture.

The optional Engine.Scene.AiDiagnostics v1 extension accepts a JSON request on the scene thread and invokes a response visitor synchronously; copy the response before returning. Layer bits: grid=1, cost=2, clearance=4, paths=8, field=16, perception=32, formation=64, probe=128. A zero mask clears overlays. Other controls are documented by the request construction in Editor/UI/AiDiagnosticsPanel.h. The extension and controller serials belong to the current module/session; reacquire after reload. The editor gets it through the runtime service; tests also support direct injection. Geometry is diagnostic only and does not modify gameplay navigation.

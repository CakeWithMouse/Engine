$ErrorActionPreference = 'Stop'
Set-Location E:/Engine
$out = 'E:/Engine/Engine/build/playtests/release-040-r2'
$runtime = 'E:/Engine/Game/build/x64/Release/Runtime'
$env:PATH = 'E:\Engine\Editor\ThirdParty\Qt6.7\bin;E:\Engine\Engine\vcpkg_installed\x64-windows\bin;' + $env:PATH
$env:PAINENGINE_ENGINE_STORE = "$out/EngineStore"
function Stage([string]$StageLabel, [scriptblock]$Action) {
    $global:LASTEXITCODE = 0
    try {
        & $Action *> "$out/$StageLabel.log"
        if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        '0' | Set-Content "$out/$StageLabel.exit"
        Write-Output "PASS $StageLabel"
    } catch {
        '1' | Set-Content "$out/$StageLabel.exit"
        Get-Content "$out/$StageLabel.log" -Tail 20
        throw "$StageLabel failed: $_"
    }
}
foreach ($name in @('CoreTests','HostTests','UiTests')) {
    Stage "$name-build" { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project "Editor/tests/$name.vcxproj" -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk }
}
Stage 'editor-regression' { & ./Editor/tools/test-regression.ps1 -Configuration Release -SkipBuild }
foreach ($item in @(@('engine','Engine/tests/EngineTests.vcxproj'),@('ai','Engine/tests/ai/AiTests.vcxproj'))) {
    $name = $item[0]; $project = $item[1]
    Stage "$name-build" { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project $project -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk }
}
Stage 'engine-tests' { & ./Engine/build/x64/Release/EngineTests.exe }
Stage 'ai-tests' { & ./Engine/build/x64/Release/AiTests.exe }
Stage 'editor-ui-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EditorUiTest -LocalSdk }
Stage 'commandlets-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EngineCommandlet -LocalSdk }
Stage 'diagnostics-fixture-build' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Engine/tests/ai/diagnostics/DiagnosticsModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk }
$fixture = "$out/diagnostics-fixture"
New-Item -ItemType Directory -Force $fixture | Out-Null
Copy-Item Engine/tests/ai/diagnostics/Fixture/* $fixture -Recurse -Force
Copy-Item Engine/build/x64/Release/AiDiagnosticsFixture/*.dll $fixture -Force
foreach ($mode in @('ai-diagnostics','camera-guides')) {
    Stage $mode { & "$runtime/EditorUiTests.exe" $fixture E:/Engine/Engine/Content "--$mode" }
}
Stage 'diagnostics-api' {
    & ./Editor/tests/AiDiagnosticsCommandletTests.ps1 -Project "$fixture/Project.json" -OutputDirectory "$out/diagnostics-api"
    $global:LASTEXITCODE = 0
}
Get-FileHash "$runtime/SDK/Lib/Engine.lib","$runtime/EngineRuntime.exe","$runtime/Editor.dll",'E:/Engine/Game/build/x64/Release/EngineLauncher.exe' | ConvertTo-Json | Set-Content "$out/runtime-hashes.json"
Write-Output 'Verification complete'

Stage 'final-runtime-sdk' { & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Game/Launcher/EngineLauncher.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk }

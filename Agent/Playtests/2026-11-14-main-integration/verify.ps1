$ErrorActionPreference = 'Stop'
$repo = 'E:\Engine'
$out = Join-Path $repo 'Engine/build/main-integration-20261114'
$runtime = Join-Path $repo 'Game/build/x64/Release/Runtime'
$env:PATH = "$repo/Engine/vcpkg_installed/x64-windows/bin;$runtime;$env:PATH"
$env:TEMP = Join-Path $out 'Temp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
function Step([string]$name, [scriptblock]$work) {
    Write-Output "START $name"
    try { & $work *> "$out/$name.log"; Set-Content "$out/$name.exit.txt" 0 }
    catch { $_ | Out-String | Add-Content "$out/$name.log"; Set-Content "$out/$name.exit.txt" 1; Get-Content -Tail 25 "$out/$name.log"; throw }
    Write-Output "PASS $name"
    Get-Content -Tail 3 "$out/$name.log"
}
function Build([string]$project, [switch]$Commandlet) {
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/$project" -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk -EngineCommandlet:$Commandlet
}
function Run([string]$exe, [string[]]$arguments = @()) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed: $LASTEXITCODE" }
}
Step 'engine-tests' { & "$repo/Engine/tests/run.ps1" -Configuration Release -PlatformToolset v143 -EnableAssimp }
Step 'edit-session' { & "$repo/Engine/tests/edit-session/run.ps1" -Configuration Release -PlatformToolset v143 }
Step 'reload-build' { Build 'Engine/tests/input/EditorReloadTests.vcxproj' }
Step 'reload' { Run "$repo/Engine/build/x64/Release/EditorReloadTests.exe" @($runtime,"$out/reload-fixture") }
Step 'ai-build' { Build 'Engine/tests/ai/AiTests.vcxproj' }
Step 'ai' { & "$repo/Engine/tests/ai/run.ps1" -Configuration Release -NoBuild }
foreach ($target in @('CoreTests','HostTests','UiTests')) {
    Step "$target-build" { Build "Editor/tests/$target.vcxproj" }
    Step $target { Run "$runtime/$target.exe" }
}
Step 'commandlets-build' { Build 'Editor/EditorModule.vcxproj' -Commandlet }
Write-Output 'ALL INTEGRATION CHECKS PASSED'

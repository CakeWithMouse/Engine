param([string]$Output, [string]$Scratch)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (!$Output -or !$Scratch) { throw 'Supply new absolute Output and Scratch directories for a replay.' }
New-Item -ItemType Directory -Path $Output,$Scratch -Force | Out-Null
Set-Location $repo
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
$env:PATH=(Join-Path $repo 'Engine/vcpkg_installed/x64-windows/bin')+';'+$env:PATH
function Check-Stage([string]$Name,[scriptblock]$Action) {
    $global:LASTEXITCODE=0
    try {
        & $Action *> (Join-Path $Output ($Name+'.log'))
        if ($LASTEXITCODE -ne 0) { throw "$Name exit $LASTEXITCODE" }
        '0' | Set-Content (Join-Path $Output ($Name+'.exit'))
        Write-Output "$Name passed"
    } catch {
        '1' | Set-Content (Join-Path $Output ($Name+'.exit'))
        Get-Content (Join-Path $Output ($Name+'.log')) -Tail 25
        throw
    }
}
function Build-Project([string]$Project) {
    & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project $Project -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk
}
Check-Stage 'ai-build' { Build-Project 'Engine/tests/ai/AiTests.vcxproj' }
Check-Stage 'ai-tests' { & ./Engine/build/x64/Release/AiTests.exe }
Check-Stage 'diagnostics-build' { Build-Project 'Engine/tests/ai/diagnostics/DiagnosticsModule.vcxproj' }
Check-Stage 'editor-build' { Build-Project 'Editor/EditorModule.vcxproj' }
Check-Stage 'editor-ui-build' {
    & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EditorUiTest
}
$fixture=Join-Path $Scratch 'diagnostics'
New-Item -ItemType Directory -Path $fixture | Out-Null
Copy-Item 'Engine/tests/ai/diagnostics/Fixture/*' $fixture -Recurse
Copy-Item 'Engine/build/x64/Release/AiDiagnosticsFixture/*.dll' $fixture
$env:PATH=(Join-Path $repo 'Engine/ThirdParty/Qt6.7/bin')+';'+$env:PATH
Check-Stage 'diagnostics-ui-tests' { & "$runtime/EditorUiTests.exe" $fixture (Join-Path $repo 'Engine/Content') --ai-diagnostics }
Check-Stage 'runtime-sdk-build' { Build-Project 'Game/Launcher/EngineLauncher.vcxproj' }
Check-Stage 'commandlets-build' {
    & ./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -EngineCommandlet
}
Check-Stage 'reload-build' { Build-Project 'Engine/tests/input/EditorReloadTests.vcxproj' }
Check-Stage 'reload-tests' { & ./Engine/build/x64/Release/EditorReloadTests.exe $runtime (Join-Path $Scratch 'reload') }
Get-FileHash "$runtime/SDK/Lib/Engine.lib" -Algorithm SHA256 | ConvertTo-Json | Set-Content (Join-Path $Output 'sdk-hash.json')

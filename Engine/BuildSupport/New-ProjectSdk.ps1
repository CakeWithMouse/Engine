param([Parameter(Mandatory=$true)][string]$Runtime, [ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$PlatformToolset='v145')
$ErrorActionPreference = 'Stop'
$engineRoot = Split-Path $PSScriptRoot
$runtimeRoot = [IO.Path]::GetFullPath($Runtime).TrimEnd('\','/')
$release = Get-Content -LiteralPath "$runtimeRoot/EngineVersion.json" -Raw | ConvertFrom-Json
$publishedSdk = Join-Path $runtimeRoot 'SDK'
$sdk = Join-Path $runtimeRoot ('.sdk-stage-' + [guid]::NewGuid().ToString('N'))
$previousSdk = Join-Path $runtimeRoot ('.sdk-previous-' + [guid]::NewGuid().ToString('N'))
# Validate all directory-move/delete targets before staging or cleanup.
foreach($target in @($publishedSdk,$sdk,$previousSdk)) {
    if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($target)) -ne $runtimeRoot) { throw 'SDK path escaped runtime directory' }
    if((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'SDK deployment target must not be a reparse point' }
}
try {
foreach ($dir in @('Include','Lib','Tools','Build')) { New-Item -ItemType Directory -Force (Join-Path $sdk $dir) | Out-Null }
Copy-Item -Path "$engineRoot/Include/*" -Destination "$sdk/Include" -Recurse -Force
Copy-Item -LiteralPath "$engineRoot/ThirdParty/GLM-master/glm" -Destination "$sdk/Include" -Recurse -Force
Copy-Item -LiteralPath "$engineRoot/ThirdParty/nlohmann" -Destination "$sdk/Include" -Recurse -Force
Copy-Item -LiteralPath "$engineRoot/build/x64/$Configuration/Engine.lib" -Destination "$sdk/Lib/Engine.lib" -Force
Copy-Item -LiteralPath "$engineRoot/build/x64/$Configuration/ComponentGenerator.exe" -Destination "$sdk/Tools/ComponentGenerator.exe" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectTools.ps1" -Destination "$sdk/ProjectTools.ps1" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectSdk.props" -Destination "$sdk/Build/ProjectSdk.props" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectSdk.targets" -Destination "$sdk/Build/ProjectSdk.targets" -Force
$importDir = if ($Configuration -eq 'Debug') { "$engineRoot/vcpkg_installed/x64-windows/debug/lib" } else { "$engineRoot/vcpkg_installed/x64-windows/lib" }
if (Test-Path -LiteralPath $importDir) { Copy-Item -Path "$importDir/*.lib" -Destination "$sdk/Lib" -Force }
New-Item -ItemType Directory -Force (Join-Path $sdk 'Examples'),(Join-Path $sdk 'Docs') | Out-Null
Copy-Item -LiteralPath "$engineRoot/../Game/BasicExamples/Source/SessionCounterSubsystem.h" -Destination "$sdk/Examples/SessionCounterSubsystem.h" -Force
Copy-Item -LiteralPath "$engineRoot/docs/GAME-SUBSYSTEMS.md" -Destination "$sdk/Docs/GAME-SUBSYSTEMS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/SAVEGAME.md" -Destination "$sdk/Docs/SAVEGAME.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/RENDER-DIAGNOSTICS.md" -Destination "$sdk/Docs/RENDER-DIAGNOSTICS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/COMMANDLETS.md" -Destination "$sdk/Docs/COMMANDLETS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/SAVEGAME.html" -Destination "$sdk/Docs/SAVEGAME.html" -Force
Copy-Item -LiteralPath "$engineRoot/examples/TraceExamples.cpp" -Destination "$sdk/Examples/TraceExamples.cpp" -Force
Copy-Item -LiteralPath "$engineRoot/docs/TRACE-API.md" -Destination "$sdk/Docs/TRACE-API.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/API-REFERENCE.html" -Destination "$sdk/Docs/API-REFERENCE.html" -Force
Copy-Item -LiteralPath "$engineRoot/docs/LAUNCHER-CLI.md" -Destination "$sdk/Docs/LAUNCHER-CLI.md" -Force
New-Item -ItemType Directory -Force (Join-Path $sdk 'Examples/ApiReference') | Out-Null
Copy-Item -Path "$engineRoot/docs/api-reference/examples/*" -Destination "$sdk/Examples/ApiReference" -Force
$manifest = @{format=1;version=$release.version;configuration=$Configuration;platform='x64';toolset=$PlatformToolset;crt= $(if($Configuration -eq 'Debug'){'MDd'}else{'MD'})}
[IO.File]::WriteAllText("$sdk/Sdk.json", ($manifest | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
# Publish complete headers and library together. If the SDK is in use and cannot be
# renamed, fail with the previous SDK intact, rather than mixing two C++ layouts.
if(Test-Path -LiteralPath $publishedSdk) { [IO.Directory]::Move($publishedSdk,$previousSdk) }
try { [IO.Directory]::Move($sdk,$publishedSdk) }
catch {
    if(Test-Path -LiteralPath $previousSdk) { [IO.Directory]::Move($previousSdk,$publishedSdk) }
    throw
}
if(Test-Path -LiteralPath $previousSdk) {
    try { Remove-Item -LiteralPath $previousSdk -Recurse -Force }
    catch { Write-Warning "Complete SDK published; previous SDK is still in use and preserved at $previousSdk" }
}
} finally {
    if(Test-Path -LiteralPath $sdk) { Remove-Item -LiteralPath $sdk -Recurse -Force }
}

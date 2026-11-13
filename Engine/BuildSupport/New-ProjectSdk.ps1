param([Parameter(Mandatory=$true)][string]$Runtime, [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$PlatformToolset='v145', [ValidateSet('auto','true','false')][string]$Assimp='auto', [switch]$UseBuildToolset)
$ErrorActionPreference = 'Stop'
$engineRoot = Split-Path $PSScriptRoot
. "$PSScriptRoot/Toolchain.ps1"
# Distributable SDKs must link with VS 2019. A local Debug SDK can use the build's tools.
if ($UseBuildToolset -and $Configuration -ne 'Debug') { throw '-UseBuildToolset is only for local Debug SDKs.' }
$sdkFamily = if ($UseBuildToolset) { $PlatformToolset } else { 'v142' }
$sdkToolchain = Resolve-EngineToolchain -Toolset $sdkFamily
$baselineDir = Join-Path $engineRoot "build/sdk/x64/$Configuration/$($sdkToolchain.version)"
[IO.Directory]::CreateDirectory($baselineDir) | Out-Null
$baselineLog = Join-Path $baselineDir 'build.log'
$useAssimp = if ($Assimp -eq 'auto') { Test-Path -LiteralPath "$engineRoot/vcpkg_installed/x64-windows/include/assimp/scene.h" } else { $Assimp -eq 'true' }
$buildArgs = @((Join-Path $engineRoot 'Engine.vcxproj'), '/nologo', '/m:1', '/nr:false', '/noconsolelogger',
    "/flp:LogFile=$baselineLog;Encoding=UTF-8;Verbosity=minimal",
    (ConvertTo-MSBuildProperty 'Configuration' $Configuration),
    (ConvertTo-MSBuildProperty 'OutDir' ($baselineDir+'/')),
    (ConvertTo-MSBuildProperty 'IntDir' ($baselineDir+'/obj/')),
    (ConvertTo-MSBuildProperty 'EngineEnableAssimp' $useAssimp.ToString().ToLowerInvariant()),
    '/p:VcpkgEnableManifest=false', '/p:WholeProgramOptimization=false')
$buildArgs += @(Get-EngineToolchainArguments $sdkToolchain)
if ($useAssimp) { $buildArgs += ConvertTo-MSBuildProperty 'IncludePath' ((Get-EngineToolchainProperties $sdkToolchain).IncludePath + ";$engineRoot/vcpkg_installed/x64-windows/include") }
Write-Output "Building $Configuration SDK with $($sdkToolchain.toolset) / MSVC $($sdkToolchain.version)."
$baselineBuild = Invoke-ToolchainProcess $sdkToolchain.msbuild $buildArgs -Timeout 1800000
if ($baselineBuild.exitCode) {
    if (Test-Path -LiteralPath $baselineLog) { Get-Content -LiteralPath $baselineLog -Encoding UTF8 | Select-Object -Last 50 }
    throw "SDK build failed with $($sdkToolchain.toolset). Existing SDK preserved. See $baselineLog"
}
$generatorArgs = @($buildArgs)
$generatorArgs[0] = Join-Path $engineRoot 'Tools/ComponentGenerator/ComponentGenerator.vcxproj'
$generatorArgs += ConvertTo-MSBuildProperty 'IntDir' ($baselineDir+'/generator-obj/')
$generatorArgs += "/flp:LogFile=$baselineDir/generator.log;Encoding=UTF-8;Verbosity=minimal"
$generatorBuild = Invoke-ToolchainProcess $sdkToolchain.msbuild $generatorArgs -Timeout 180000
if ($generatorBuild.exitCode) { throw "SDK component generator build failed with $($sdkToolchain.toolset). See $baselineDir/generator.log" }
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
Copy-Item -LiteralPath "$baselineDir/Engine.lib" -Destination "$sdk/Lib/Engine.lib" -Force
Copy-Item -LiteralPath "$baselineDir/ComponentGenerator.exe" -Destination "$sdk/Tools/ComponentGenerator.exe" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectTools.ps1" -Destination "$sdk/ProjectTools.ps1" -Force
Copy-Item -LiteralPath "$PSScriptRoot/Toolchain.ps1" -Destination "$sdk/Toolchain.ps1" -Force
Copy-Item -LiteralPath "$PSScriptRoot/Test-ProjectToolchain.ps1" -Destination "$sdk/Test-ProjectToolchain.ps1" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectSdk.props" -Destination "$sdk/Build/ProjectSdk.props" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectBootstrap.props" -Destination "$sdk/Build/ProjectBootstrap.props" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectBootstrap.targets" -Destination "$sdk/Build/ProjectBootstrap.targets" -Force
Copy-Item -LiteralPath "$PSScriptRoot/ProjectSdk.targets" -Destination "$sdk/Build/ProjectSdk.targets" -Force
$importDir = if ($Configuration -eq 'Debug') { "$engineRoot/vcpkg_installed/x64-windows/debug/lib" } else { "$engineRoot/vcpkg_installed/x64-windows/lib" }
if ($useAssimp -and (Test-Path -LiteralPath $importDir)) { Copy-Item -Path "$importDir/*.lib" -Destination "$sdk/Lib" -Force }
New-Item -ItemType Directory -Force (Join-Path $sdk 'Examples'),(Join-Path $sdk 'Docs') | Out-Null
Copy-Item -LiteralPath "$engineRoot/../Game/BasicExamples/Source/SessionCounterSubsystem.h" -Destination "$sdk/Examples/SessionCounterSubsystem.h" -Force
Copy-Item -LiteralPath "$engineRoot/docs/GAME-SUBSYSTEMS.md" -Destination "$sdk/Docs/GAME-SUBSYSTEMS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/SAVEGAME.md" -Destination "$sdk/Docs/SAVEGAME.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/RENDER-DIAGNOSTICS.md" -Destination "$sdk/Docs/RENDER-DIAGNOSTICS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/COMMANDLETS.md" -Destination "$sdk/Docs/COMMANDLETS.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/RENDERING.md" -Destination "$sdk/Docs/RENDERING.md" -Force
foreach($document in @('PROJECT-ASSETS.md','MATERIALS.md','ENGINE-VERSIONS.md')) {
    Copy-Item -LiteralPath "$engineRoot/docs/$document" -Destination "$sdk/Docs/$document" -Force
}
Copy-Item -LiteralPath "$engineRoot/docs/SAVEGAME.html" -Destination "$sdk/Docs/SAVEGAME.html" -Force
Copy-Item -LiteralPath "$engineRoot/examples/TraceExamples.cpp" -Destination "$sdk/Examples/TraceExamples.cpp" -Force
Copy-Item -LiteralPath "$engineRoot/docs/TRACE-API.md" -Destination "$sdk/Docs/TRACE-API.md" -Force
Copy-Item -LiteralPath "$engineRoot/docs/API-REFERENCE.html" -Destination "$sdk/Docs/API-REFERENCE.html" -Force
# Offline manuals and the exact sources/resources used by their examples.
foreach ($guide in @('index','editor','materials','audio','code','assets','scene')) {
    Copy-Item -LiteralPath "$engineRoot/docs/$guide.html" -Destination "$sdk/Docs/$guide.html" -Force
}
New-Item -ItemType Directory -Force (Join-Path $sdk 'Docs/tutorial') | Out-Null
Copy-Item -LiteralPath "$engineRoot/docs/tutorial/examples" -Destination "$sdk/Docs/tutorial/examples" -Recurse -Force
Copy-Item -LiteralPath "$engineRoot/docs/tutorial/resources" -Destination "$sdk/Docs/tutorial/resources" -Recurse -Force
Copy-Item -LiteralPath "$engineRoot/docs/LAUNCHER-CLI.md" -Destination "$sdk/Docs/LAUNCHER-CLI.md" -Force
New-Item -ItemType Directory -Force (Join-Path $sdk 'Examples/ApiReference') | Out-Null
Copy-Item -Path "$engineRoot/docs/api-reference/examples/*" -Destination "$sdk/Examples/ApiReference" -Force
$manifest = @{format=2;version=$release.version;configuration=$Configuration;platform='x64';toolset=$sdkToolchain.toolset;crt= $(if($Configuration -eq 'Debug'){'MDd'}else{'MD'});
    buildTools=@{version=$sdkToolchain.version;minimumVersion=$sdkToolchain.version;compilerVersion=$sdkToolchain.compilerVersion;wholeProgramOptimization=$false;windowsSdk=$sdkToolchain.windowsSdk.version};
    assimp=$useAssimp;engineLibrarySha256=(Get-FileHash -LiteralPath "$sdk/Lib/Engine.lib").Hash}
[IO.File]::WriteAllText("$sdk/Sdk.json", ($manifest | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
# Reject too-new dependencies or incompatible public headers before publication.
$validationArgs = @{ Sdk = $sdk; OutputDirectory = "$baselineDir/validation" }
if ($UseBuildToolset) { $validationArgs['Toolsets'] = @($sdkToolchain.toolset) }
& "$PSScriptRoot/Test-SdkCompatibility.ps1" @validationArgs
Copy-Item -LiteralPath "$baselineDir/validation/results.json" -Destination "$sdk/Compatibility.json"
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

param([Parameter(Mandatory=$true)][string]$Runtime, [ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$PlatformToolset='v145')
$ErrorActionPreference = 'Stop'
$engineRoot = Split-Path $PSScriptRoot
$sdk = Join-Path $Runtime 'SDK'
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
$release = Get-Content -LiteralPath "$Runtime/EngineVersion.json" -Raw | ConvertFrom-Json
$manifest = @{format=1;version=$release.version;configuration=$Configuration;platform='x64';toolset=$PlatformToolset;crt= $(if($Configuration -eq 'Debug'){'MDd'}else{'MD'})}
[IO.File]::WriteAllText("$sdk/Sdk.json", ($manifest | ConvertTo-Json), [Text.UTF8Encoding]::new($false))

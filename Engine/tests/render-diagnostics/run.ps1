param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/RenderDiagnosticsTests.vcxproj" -Configuration $Configuration -EnableAssimp
$output=Join-Path $repo ('Engine/build/render-diagnostics-'+[guid]::NewGuid().ToString('N'))
& "$repo/Engine/build/x64/$Configuration/RenderDiagnosticsTests.exe" "$repo/Game/build/x64/$Configuration/Runtime/EngineContent" $output
if($LASTEXITCODE -ne 0){throw 'Render diagnostics tests failed'}
Write-Output "Evidence: $output"

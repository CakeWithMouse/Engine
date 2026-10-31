param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [switch]$SkipBuild, [switch]$IncludeUi)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$work = Join-Path $repo ('Engine/build/compatibility/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
$env:PAINENGINE_ENGINE_STORE = Join-Path $work 'Engines'
$env:TEMP = $work
$env:TMP = $work
foreach ($version in @('pre-lights','lights')) {
    $folder = Join-Path $work $version
    Expand-Archive -LiteralPath "$PSScriptRoot/fixtures/$version.zip" -DestinationPath $folder
    foreach ($line in Get-Content -LiteralPath "$folder/SHA256SUMS.txt") {
        $hash,$name = $line -split '  ',2
        if ((Get-FileHash -LiteralPath (Join-Path $folder $name)).Hash -ne $hash) { throw "Frozen fixture changed: $version/$name" }
    }
}
if (!$SkipBuild) {
    & "$repo/Engine/build.ps1" -Configuration $Configuration -Target EngineLauncher -EnableAssimp $true
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$PSScriptRoot/CompatibilityTests.vcxproj" -Configuration $Configuration -EnableAssimp
}
$runtime = Join-Path $repo "Game/build/x64/$Configuration/Runtime"
& "$repo/Engine/build/x64/$Configuration/CompatibilityTests.exe" $runtime "$work/pre-lights" "$work/lights" "$work/projects"
if ($LASTEXITCODE -ne 0) { throw 'Compatibility regression failed' }
if ($IncludeUi) {
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EditorUiTest
    & "$runtime/EditorUiTests.exe" "$work/projects/Old project" "$runtime/EngineContent" '--legacy'
    if ($LASTEXITCODE -ne 0) { throw 'Legacy Qt editor compatibility failed' }
}
Write-Output "Frozen release compatibility passed. Fixtures: $work"

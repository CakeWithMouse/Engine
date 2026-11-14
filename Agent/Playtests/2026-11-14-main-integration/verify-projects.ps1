$ErrorActionPreference='Stop'
$repo='E:\Engine'
$out="$repo/Engine/build/main-integration-20261114"
$runtime="$repo/Game/build/x64/Release/Runtime"
$env:TEMP="$out/Temp"; $env:TMP=$env:TEMP
$env:PYTHONIOENCODING='utf-8'
foreach ($entry in @(@('import','ImportSettingsCommandletTests.py'),@('authoring','AuthoringCommandletTests.py'))) {
    & python "$repo/Editor/tests/$($entry[1])" --tool "$runtime/EngineCommandlets.exe" --launcher "$repo/Game/build/x64/Release/EngineLauncher.exe" --output "$out/$($entry[0])" *> "$out/$($entry[0]).log"
    $code=$LASTEXITCODE; Set-Content "$out/$($entry[0]).exit.txt" $code
    Get-Content -Tail 8 "$out/$($entry[0]).log"
    if ($code) { throw "$($entry[0]) failed: $code" }
}
& python "$repo/Editor/tests/CommandletIntegerValidationTests.py" --tool "$runtime/EngineCommandlets.exe" --output "$out/integer" *> "$out/integer.log"
$code=$LASTEXITCODE; Set-Content "$out/integer.exit.txt" $code
if ($code) { throw "Integer tests failed: $code" }
Write-Output 'Commandlet integration scenarios passed'
$projects=@{
    'calm-city'='calm-city/CalmCityUpgraded'
    'curious-city'='curious-city/upgraded-city'
    'tired-bomber'='tired-bomber/BomberUpgradedR7'
    'ai-trash'='ai-trash/TrashGameUpgraded'
}
foreach ($role in $projects.Keys) {
    $project="$repo/Engine/build/playtests/20261114-r7/$($projects[$role])/Project.json"
    & "$runtime/SDK/ProjectTools.ps1" -Action Build -Project $project -Runtime $runtime -Rebuild *> "$out/demo-$role-build.log"
    & "$runtime/EngineRuntime.exe" --project $project --mode game --frames 20 *> "$out/demo-$role-run.log"
    $code=$LASTEXITCODE; Set-Content "$out/demo-$role-run.exit.txt" $code
    if ($code) { throw "Demo $role failed: $code" }
    Write-Output "PASS rebuilt demo $role"
}
$tutorial="$repo/Game/build/x64/Release/Tutorials/AiTutorial"
Copy-Item -Path "$runtime/Templates/SceneProject/*.dll" -Destination $tutorial -Force
& "$runtime/EngineRuntime.exe" --project "$tutorial/Project.json" --mode game --frames 20 *> "$out/demo-ai-run.log"
$code=$LASTEXITCODE; Set-Content "$out/demo-ai-run.exit.txt" $code
if ($code) { throw "AI tutorial failed: $code" }
Write-Output 'PASS refreshed AI tutorial'

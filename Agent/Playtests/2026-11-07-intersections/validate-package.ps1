param([string]$Repository='E:/Engine')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath($Repository)
$version=(Get-Content -Raw "$repo/Engine/Version.json" | ConvertFrom-Json).version
$package="$repo/Engine/build/releases/PainEngine-$version-win64"
$work="$repo/Engine/build/playtests/20261107-intersections"
$env:PAINENGINE_ENGINE_STORE="$work/store-package-finalized"
$env:TEMP=$work
$env:TMP=$work
$results=[Collections.Generic.List[object]]::new()
function Invoke-Package([string]$Name,[string[]]$Arguments) {
    # Start-Process -Wait also waits for descendant jobs (including MSVC's idle PDB server).
    # The launcher's own exit is the CLI contract; wait for that process explicitly.
    $p=Start-Process -FilePath "$package/EngineLauncher.exe" -ArgumentList $Arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$work/package-$Name.log" -RedirectStandardError "$work/package-$Name.err"
    $p.WaitForExit()
    $p.Refresh()
    $results.Add(@{test=$Name;exit=$p.ExitCode})
    if($p.ExitCode -ne 0){throw "Packaged $Name failed: $($p.ExitCode)"}
    Write-Output "$Name PASS"
}
foreach($name in @('BasicExamples','Pong','SunGame','Battle','Katamari','RuntimeExample')) {
    Invoke-Package $name @('--smoke',$name)
}
foreach($name in @('BasicExamples','RuntimeExample')) {
    Invoke-Package "$name-editor" @('--smoke-editor',$name)
}
$project="$work/packaged-cpp-finalized"
Invoke-Package 'create' @('--create-project',$project,'IntersectionsRelease')
Invoke-Package 'code' @('--enable-code',"$project/Project.json")
Invoke-Package 'project-game' @('--smoke-project',"$project/Project.json")
Invoke-Package 'project-editor' @('--smoke-project-editor',"$project/Project.json")
$results | ConvertTo-Json -Depth 5 | Set-Content "$work/package-smoke.json"

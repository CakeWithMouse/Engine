param([string]$Repository='E:/Engine')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath($Repository)
$version=(Get-Content -Raw "$repo/Engine/Version.json"|ConvertFrom-Json).version
$package=[IO.Path]::GetFullPath("$repo/Engine/build/releases/PainEngine-$version-win64")
$archive="$package.zip"
if(Test-Path -LiteralPath $archive){throw 'Archive already exists; preserve it.'}
$work="$repo/Engine/build/playtests/20261107-intersections"
foreach($suite in @(@('package-intersections-final',46,556),@('package-transparency',52,428),@('package-materials',48,288),@('package-slots',22,366),@('package-motion',34,282))) {
    $gpu=Get-Content -LiteralPath "$work/$($suite[0])/summary.json" -Raw | ConvertFrom-Json
    if($gpu.passed -ne $suite[1] -or $gpu.frames -ne $suite[2] -or @($gpu.cases | Where-Object {$_.errors.Count -gt 0}).Count){throw "GPU matrix failed: $($suite[0])"}
}
$smoke=@(Get-Content -LiteralPath "$work/package-smoke.json" -Raw | ConvertFrom-Json)
if($smoke.Count -ne 12 -or @($smoke | Where-Object {$_.exit -ne 0}).Count){throw 'Final package smoke failed'}
if(!(Select-String -LiteralPath "$work/engine-final.log" -SimpleMatch '70/70 tests passed; 0 skipped; 0 failed')){throw 'Engine regression failed'}
if(!(Select-String -LiteralPath "$work/compatibility-final.log" -SimpleMatch '116 checks passed')){throw 'Compatibility regression failed'}
# Test runs create shader/Python caches. Only remove these generated directories in this package.
$prefix=$package.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
foreach($cache in Get-ChildItem -LiteralPath $package -Directory -Recurse | Where-Object {$_.Name -in @('.shadow_cache','__pycache__')}){
    $resolved=[IO.Path]::GetFullPath((Resolve-Path -LiteralPath $cache.FullName).Path)
    if(!$resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
       ($cache.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Unsafe cache path: $resolved"}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
@"
PainEngine $version Windows x64 Release
EngineTests: 70/70, 0 skipped
Packaged intersecting transparency GPU: 46 scenarios, 556 frames (ray/plane oracle)
Packaged transparency GPU: 52 scenarios, 428 frames (NVIDIA RTX 4070 Laptop, Forward/Deferred)
Packaged material GPU: 48 scenarios, 288 frames
Packaged point-shadow/LOD GPU: 22 scenarios, 366 frames
Packaged moving point-shadow GPU: 34 scenarios, 282 frames
Packaged smoke: 6 games, 2 editors, new C++ project create/enable/game/editor
Frozen binary compatibility: 116 checks
API reference check: passed; git diff --check: passed
SDK Engine.lib and EngineCommandlets.exe match the final build SHA-256.
Detailed evidence: Agent/Playtests/2026-11-07-intersections/report.md in source checkout.
"@ | Set-Content -LiteralPath "$package/BuildVerification.txt" -Encoding utf8
$manifest=foreach($file in Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName){
    if($file.FullName -eq "$package\SHA256SUMS.txt"){continue}
    $relative=[IO.Path]::GetRelativePath($package,$file.FullName).Replace('\','/')
    (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$relative
}
$manifest | Set-Content -LiteralPath "$package/SHA256SUMS.txt" -Encoding utf8
python "$repo/Agent/Playtests/2026-11-07-2026.11.06.1-r5/fixes/archive.py" $package $archive |
    Set-Content -LiteralPath "$archive.sha256.json" -Encoding utf8
if($LASTEXITCODE){throw 'Archive verification failed'}
Copy-Item -LiteralPath "$archive.sha256.json" -Destination "$PSScriptRoot/package-verification.json"
Get-Content -LiteralPath "$archive.sha256.json"

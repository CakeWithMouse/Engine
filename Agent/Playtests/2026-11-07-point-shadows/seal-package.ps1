param([string]$Repository='E:/Engine')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath($Repository)
$version=(Get-Content -Raw "$repo/Engine/Version.json"|ConvertFrom-Json).version
$package=[IO.Path]::GetFullPath("$repo/Engine/build/releases/PainEngine-$version-win64")
$archive="$package.zip"
if(Test-Path -LiteralPath $archive){throw 'Archive already exists; preserve it.'}
# Test runs can create shader caches. Only remove these generated directories in this package.
$prefix=$package.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
foreach($cache in Get-ChildItem -LiteralPath $package -Directory -Recurse -Filter .shadow_cache){
    $resolved=[IO.Path]::GetFullPath((Resolve-Path -LiteralPath $cache.FullName).Path)
    if(!$resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
       ($cache.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Unsafe cache path: $resolved"}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
@"
PainEngine $version Windows x64 Release
EngineTests: 64/64, 0 skipped
Packaged GPU motion: 34 scenarios, 282 frames (NVIDIA RTX 4070 Laptop, Forward/Deferred)
Packaged smoke: 6 games, 2 editors, new C++ project create/enable/game/editor
Frozen binary compatibility: 116 checks; legacy Qt UI: 387 checks
Tangent commandlets: 7; point-light duplicate/save/new/load: passed
API reference check: passed; git diff --check: passed
SDK Engine.lib and EngineCommandlets.exe match the final build SHA-256.
Detailed evidence: Agent/Playtests/2026-11-07-point-shadows/report.md in source checkout.
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

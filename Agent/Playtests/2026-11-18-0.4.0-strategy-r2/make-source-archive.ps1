$ErrorActionPreference = 'Stop'
$project = 'E:\Engine\Engine\build\playtests\strategy-040-r2\RepairCorridor'
$stage = 'E:\Engine\Engine\build\playtests\strategy-040-r2\source-staging-final'
$archive = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\RepairCorridor-0.4.0-r2-source.zip'
$scratchRoot = [IO.Path]::GetFullPath('E:\Engine\Engine\build\playtests\strategy-040-r2').TrimEnd('\') + '\'
if (([IO.Path]::GetFullPath($stage) + '\').StartsWith($scratchRoot,[StringComparison]::OrdinalIgnoreCase) -eq $false) { throw 'Stage escaped r2 scratch root' }
if (Test-Path -LiteralPath $stage) { throw "Fresh staging directory already exists: $stage" }
if (Test-Path -LiteralPath $archive) { throw "Archive already exists; preserve prior evidence: $archive" }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$excluded = @('Binaries','Intermediate','SaveData','.git')
$sourceRoot = [IO.Path]::GetFullPath($project)
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force) {
    $relative = [IO.Path]::GetRelativePath($sourceRoot,$file.FullName)
    $parts = $relative.Split([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
    if ($parts | Where-Object { $_ -in $excluded }) { continue }
    if ($relative -in @('.project-build.lock','Project.json.before-build') -or $relative -like 'Project.candidate.*.json') { continue }
    $dest = Join-Path $stage $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $dest
}
foreach ($required in @('Game.sln','Game.vcxproj','CMakeLists.txt','Project.json','Source\ProjectGame.cpp','Content\Scenes\MeshPhysicsPersistence.scene.json')) {
    if (!(Test-Path -LiteralPath (Join-Path $stage $required))) { throw "Required source file missing from staging: $required" }
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entries = @($zip.Entries | ForEach-Object FullName)
    $bad = @($entries | Where-Object { $_ -match '(^|/)(Binaries|Intermediate|SaveData|SDK)(/|$)' -or ($_ -match '\.(dll|lib|pdb|ilk)$') -or ($_ -match '\.obj$' -and $_ -notmatch '^Content/') })
    if ($bad.Count) { throw "Source archive contains build artifacts: $($bad -join ', ')" }
    $requiredZip = @('Game.sln','Game.vcxproj','CMakeLists.txt','Project.json','Source/ProjectGame.cpp','Content/Scenes/MeshPhysicsPersistence.scene.json')
    foreach ($required in $requiredZip) { if ($required -notin $entries) { throw "Archive entry missing: $required" } }
} finally { $zip.Dispose() }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
$listing = Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object { [pscustomobject]@{path=$_.FullName.Substring($stage.Length+1).Replace('\','/'); sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash; bytes=$_.Length} }
$listing | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\source-archive-files.json' -Encoding UTF8
[IO.File]::WriteAllText('E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\source-archive-summary.txt', "archive=$archive`nsha256=$hash`nentries=$($entries.Count)`nbytes=$((Get-Item -LiteralPath $archive).Length)`n")
Get-Content -LiteralPath 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\source-archive-summary.txt'

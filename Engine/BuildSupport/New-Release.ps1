param(
    [Parameter(Mandatory=$true)][string]$BaselineArchive,
    [Parameter(Mandatory=$true)][hashtable]$PreviousArchives,
    [string]$OutputDirectory = '',
    [string]$RarDirectory = 'C:/Program Files/WinRAR'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$version = (Get-Content -Raw -LiteralPath "$repo/Engine/Version.json" | ConvertFrom-Json).version
if ($version -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]{0,63}$') { throw 'Invalid release version' }
if (!$OutputDirectory) { $OutputDirectory = "$repo/Engine/build/releases" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$archive = Join-Path $OutputDirectory "PainEngine-$version-win64.rar"
if (Test-Path -LiteralPath $archive) { throw 'Release archive already exists. Bump Engine/Version.json for a new release.' }
$work = Join-Path $repo ('Engine/build/releases/staging-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$env:PAINENGINE_ENGINE_STORE = Join-Path $work 'engine-store'
$env:TEMP = $work; $env:TMP = $work

# Release gate: actual historical binaries are tested, never rebuilt.
& "$repo/Engine/build.ps1" -Configuration Release -Target All -EnableAssimp $true
& "$repo/Engine/tests/compatibility/run.ps1" -IncludeUi
& "$repo/Editor/tests/run.ps1" -Configuration Release -EnableAssimp
$env:PAINENGINE_ENGINE_STORE = Join-Path $work 'engine-store'
$env:TEMP = $work; $env:TMP = $work

function Extract([string]$source,[string]$directory) {
    New-Item -ItemType Directory -Path $directory | Out-Null
    & "$RarDirectory/UnRAR.exe" x -idq -o- $source "$directory/"
    if ($LASTEXITCODE) { throw "Cannot extract $source" }
    $roots = @(Get-ChildItem -LiteralPath $directory -Directory)
    if ($roots.Count -ne 1) { throw 'Expected one release root folder' }
    return $roots[0].FullName
}
$baseline = Extract $BaselineArchive (Join-Path $work 'baseline')
$package = Join-Path $work "PainEngine-$version-win64"
Copy-Item -LiteralPath $baseline -Destination $package -Recurse
$release = "$repo/Game/build/x64/Release"
$appNames = @('EngineLauncher.exe','EngineRuntime.exe','Editor.dll','Game.dll','BasicExamples.exe','Pong.exe','SunGame.exe','Battle.exe','Katamari.exe')
$updated = 0
foreach ($file in Get-ChildItem -LiteralPath $package -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($package,$file.FullName)
    # Historical engine bundles must remain byte-for-byte historical.
    if ($relative.StartsWith('Engines' + [IO.Path]::DirectorySeparatorChar)) { continue }
    $source = Join-Path $release $relative
    if ($file.Name -in $appNames -and !(Test-Path -LiteralPath $source)) { throw "Missing rebuilt application: $relative" }
    if (Test-Path -LiteralPath $source -PathType Leaf) { Copy-Item -LiteralPath $source -Destination $file.FullName -Force; if ($file.Name -in $appNames) { ++$updated } }
}
if ($updated -ne 10) { throw "Expected ten application binaries, got $updated" }
Copy-Item -LiteralPath "$release/Runtime/SDK" -Destination "$package/Runtime" -Recurse -Force
foreach ($name in @('EngineVersion.json','EngineConfiguration.txt','TemplateCompatibility.json')) {
    Copy-Item -LiteralPath "$release/Runtime/$name" -Destination "$package/Runtime/$name" -Force
}
foreach ($key in $PreviousArchives.Keys) {
    if ($key -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]{0,63}$' -or $key -eq $version) { throw 'Invalid historical version' }
    $old = Extract $PreviousArchives[$key] (Join-Path $work "old-$key")
    $destination = Join-Path $package "Engines/$key/Release"
    if (Test-Path -LiteralPath $destination) { throw 'Historical bundle already exists in baseline' }
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath "$old/Runtime" -Destination $destination -Recurse
    [IO.File]::WriteAllText("$destination/EngineVersion.json",(@{version=$key} | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText("$destination/EngineConfiguration.txt",'Release',[Text.UTF8Encoding]::new($false))
}
# Repair the dependency in all bundles, including historical runtime packages.
# Engine/game binaries stay historical; OS-serviced DLLs must never be shipped.
& "$PSScriptRoot/Deploy-D3DCompiler.ps1" -DestinationRoot $package
Copy-Item -LiteralPath "$repo/Engine/docs/ENGINE-VERSIONS.md" -Destination "$package/Версии движка.md"
$readme = @"
PainEngine $version — Windows x64 Release

Распакуйте архив полностью и запустите EngineLauncher.exe.
Новый проект хранит выбранную версию в Engine.lock.json.
Старые Game.dll поддерживаются адаптером; вручную заменять их для открытия не нужно.
Кнопка «Закрепить версию…» выбирает Runtime из установленного пакета или Engines.
«Добавить версию…» сохраняет runtime и зависимости в %LOCALAPPDATA%/PainEngine/Engines.
«Обновить копию…» создаёт новый проект на текущем движке. Оригинал сохраняется.
Своя Game.dll остаётся в проекте. Неизвестные пользовательские DLL не заменяются шаблонными.
Подробности — «Версии движка.md». Лицензии — Licenses; зависимости — Redist.
"@
[IO.File]::WriteAllText("$package/Прочитай меня.txt",$readme,[Text.UTF8Encoding]::new($true))

function Smoke([string[]]$arguments) {
    $p = Start-Process -FilePath "$package/EngineLauncher.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(60000)) { $p.Kill(); throw 'Packaged smoke timed out' }
    if ($p.ExitCode) { throw "Packaged smoke failed: $arguments ($($p.ExitCode))" }
}
$env:ENGINE_CONTENT_ROOT = $null; $env:GAME_CONTENT_ROOT = $null
foreach ($game in @('BasicExamples','Pong','SunGame','Battle','Katamari','RuntimeExample')) { Smoke @('--smoke',$game) }
foreach ($game in @('BasicExamples','Pong','SunGame','Battle','RuntimeExample')) { Smoke @('--smoke-editor',$game) }
$fresh = Join-Path $work 'current-project'
$created = Start-Process -FilePath "$package/EngineLauncher.exe" -ArgumentList @('--create-project',('"'+$fresh+'"'),'Current') -WindowStyle Hidden -PassThru -Wait
if ($created.ExitCode) { throw 'Cannot create current packaged project' }
Smoke @('--smoke-project',('"'+$fresh+'/Project.json"'))
Smoke @('--smoke-project-editor',('"'+$fresh+'/Project.json"'))
foreach ($key in $PreviousArchives.Keys) {
    $fixture = Join-Path $work "old-project-$key"
    $p = Start-Process -FilePath "$package/EngineLauncher.exe" -ArgumentList @('--create-project',('"'+$fixture+'"'),'Compatibility') -WindowStyle Hidden -PassThru -Wait
    if ($p.ExitCode) { throw 'Cannot create packaged compatibility fixture' }
    Copy-Item -Path "$package/Engines/$key/Release/Templates/SceneProject/*.dll" -Destination $fixture -Force
    # The pre-light release cannot read point lights; use only the original cube/floor.
    $scene = Get-Content -Raw -LiteralPath "$fixture/Content/Scenes/Main.scene.json" | ConvertFrom-Json
    $scene.objects = @($scene.objects | Where-Object { $_.type -notlike 'Light.*' })
    [IO.File]::WriteAllText("$fixture/Content/Scenes/Main.scene.json",($scene | ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText("$fixture/Engine.lock.json",(@{format=1;engineVersion=$key;engineConfiguration='Release'} | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    Smoke @('--smoke-project',('"'+$fixture+'/Project.json"'))
    Smoke @('--smoke-project-editor',('"'+$fixture+'/Project.json"'))
}
# Remove only disposable caches strictly inside this staging package.
$prefix = [IO.Path]::GetFullPath($package) + [IO.Path]::DirectorySeparatorChar
foreach ($cache in Get-ChildItem -LiteralPath $package -Directory -Recurse | Where-Object Name -eq '.shadow_cache') {
    if (![IO.Path]::GetFullPath($cache.FullName).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Cache outside package' }
    Remove-Item -LiteralPath $cache.FullName -Recurse -Force
}
$hashes = Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object Name -ne 'SHA256SUMS.txt' | Sort-Object FullName | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($package,$_.FullName) }
[IO.File]::WriteAllLines("$package/SHA256SUMS.txt",$hashes,[Text.UTF8Encoding]::new($false))
Push-Location $work
try {
    & "$RarDirectory/Rar.exe" a -ma5 -m5 -r -idq $archive (Split-Path $package -Leaf)
    if ($LASTEXITCODE) { throw 'RAR packaging failed' }
    & "$RarDirectory/UnRAR.exe" t -idq $archive
    if ($LASTEXITCODE) { throw 'Archive integrity check failed' }
} finally { Pop-Location }
Write-Output "Release verified: $archive"
Get-FileHash -LiteralPath $archive -Algorithm SHA256

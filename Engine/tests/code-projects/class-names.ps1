param([string]$ToolsScript = "$PSScriptRoot/../../BuildSupport/ProjectTools.ps1")
$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot ('../../build/class-names/' + [guid]::NewGuid().ToString('N'))
$project = Join-Path $work 'Project with spaces'
$runtime = Join-Path $work 'Runtime with spaces'
New-Item -ItemType Directory -Path "$project/Source", "$runtime/SDK" -Force | Out-Null
[IO.File]::WriteAllText("$project/Engine.lock.json", '{"engineVersion":"test","engineConfiguration":"Release"}')
[IO.File]::WriteAllText("$project/Project.code.json", '{"state":"prepared"}')
[IO.File]::WriteAllText("$project/Project.json", '{}')
[IO.File]::WriteAllText("$runtime/SDK/Sdk.json", '{"version":"test","configuration":"Release","platform":"x64","toolset":"v145"}')
$checks = 0
function Check([bool]$value, [string]$message) { if (!$value) { throw $message }; $script:checks++ }
$cyrillic = [string][char]0x0413 + [char]0x0435 + [char]0x0440 + [char]0x043e + [char]0x0439
foreach ($name in @('', 'My Character', 'MyCharacter.h', '1Hero', $cyrillic, 'class', 'namespace', 'Character', 'CON', 'lpt1')) {
    $before = @(Get-ChildItem -LiteralPath "$project/Source").Count
    $failure = $null
    try { & $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name $name -Kind Character }
    catch { $failure = $_.Exception.Message }
    Check ($failure -and ($failure.Contains('class name') -or $failure.Contains('Class name'))) "Unclear rejection: $name"
    Check (@(Get-ChildItem -LiteralPath "$project/Source").Count -eq $before) "Invalid name wrote files: $name"
}
& $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name '  MyHero  ' -Kind Character
Check ((Get-Content -LiteralPath "$project/Source/MyHero.h" -Raw).Contains('class MyHero : public Character')) 'Trimmed Character inheritance'
Check ((Get-Content -LiteralPath "$project/Source/MyHero.cpp" -Raw).Contains('Character::BeginPlay()')) 'Character parent call'
$before = (Get-FileHash -LiteralPath "$project/Source/MyHero.h").Hash
$failure = $null
try { & $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name MyHero -Kind Character } catch { $failure = $_.Exception.Message }
Check ($failure -and $failure.Contains('already exists')) 'Duplicate diagnostic'
Check ((Get-FileHash -LiteralPath "$project/Source/MyHero.h").Hash -eq $before) 'Duplicate overwrote header'
$failure = $null
try { & $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name Self -Kind Component -Base Self } catch { $failure = $_.Exception.Message }
Check ($failure -and $failure.Contains('inherit from itself')) 'Self inheritance diagnostic'
& $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name ChildHero -Kind Character -Base MyHero -BaseHeader MyHero.h
Check ((Get-Content -LiteralPath "$project/Source/ChildHero.cpp" -Raw).Contains('MyHero::Tick(deltaTime)')) 'Custom immediate parent call'
& $ToolsScript -Action AddClass -Project "$project/Project.json" -Runtime $runtime -Name PlainObject -Kind Object
Check ((Get-Content -LiteralPath "$project/Source/PlainObject.h" -Raw).Contains('ENGINE_OBJECT("PlainObject")')) 'Independent object'
Write-Output "Class names: $checks checks passed. Fixture: $project"

param([string]$Blender=$env:PAINENGINE_BLENDER,
    [string]$Output="$PSScriptRoot/../../build/blend-tests/$([guid]::NewGuid().ToString('N'))")
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/../../..")
$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory; existing evidence is never overwritten.'}
if(!$Blender){
    $installed=Get-ChildItem -LiteralPath "$env:ProgramFiles/Blender Foundation" -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Where-Object {Test-Path -LiteralPath "$($_.FullName)/blender.exe"} | Select-Object -First 1
    if($installed){$Blender="$($installed.FullName)/blender.exe"}
}
if(!$Blender -or !(Test-Path -LiteralPath $Blender)){throw 'Install Blender 4.2+ or pass -Blender with its executable path.'}
[IO.Directory]::CreateDirectory($Output)|Out-Null
$previousBlender=$env:PAINENGINE_BLENDER;$previousFixtures=$env:PAINENGINE_BLEND_FIXTURES
try {
    $env:PAINENGINE_BLENDER=[IO.Path]::GetFullPath($Blender)
    $env:PAINENGINE_BLEND_FIXTURES="$Output/Исходники моделей"
    & $Blender --version *> "$Output/blender-version.log"
    if($LASTEXITCODE -ne 0){throw 'Blender did not start'}
    & $Blender --background --factory-startup --disable-autoexec --python-exit-code 1 --python "$PSScriptRoot/make-blend-fixtures.py" -- $env:PAINENGINE_BLEND_FIXTURES *> "$Output/fixtures.log"
    if($LASTEXITCODE -ne 0){throw "Blender fixtures failed; see $Output/fixtures.log"}
    & "$PSScriptRoot/run.ps1" -Configuration Release -EnableAssimp *> "$Output/assets.log"
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration Release -EngineCommandlet *> "$Output/commandlet-build.log"
    & "$repo/Editor/tests/AssimpCommandletTests.ps1" -Output "$Output/commands" -BlendFixtures $env:PAINENGINE_BLEND_FIXTURES *> "$Output/commands.log"
    Write-Output "BLEND integration passed. Evidence: $Output"
} finally {
    $env:PAINENGINE_BLENDER=$previousBlender;$env:PAINENGINE_BLEND_FIXTURES=$previousFixtures
}

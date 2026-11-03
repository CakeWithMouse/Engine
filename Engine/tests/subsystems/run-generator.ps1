param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$generator=[IO.Path]::GetFullPath("$PSScriptRoot/../../build/x64/$Configuration/ComponentGenerator.exe")
$scratch=[IO.Path]::GetFullPath("$PSScriptRoot/../../build/subsystem-generator/$([guid]::NewGuid().ToString('N'))")
New-Item -ItemType Directory -Force "$scratch/Source", "$scratch/Generated" | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Put($file,$value) {[IO.File]::WriteAllText($file,$value,$utf8)}
Put "$scratch/roots.txt" "$scratch/Source"
function Generate([bool]$success) {
    & $generator "$scratch/roots.txt" "$scratch/Generated" SubsystemScanner
    if (($LASTEXITCODE -eq 0) -ne $success) {throw 'Unexpected generator result'}
}
Generate $true
$empty=Get-Content "$scratch/Generated/GeneratedComponents.cpp" -Raw
Put "$scratch/Source/Counter.h" 'ENGINE_SUBSYSTEM() class Counter : public GameSubsystem {};'
Generate $true
if (!(Get-Content "$scratch/Generated/GeneratedComponents.cpp" -Raw).Contains('CreateGameSubsystem<::Counter>')) {throw 'Missing factory'}
Put "$scratch/Source/Other.h" 'ENGINE_SUBSYSTEM() class Counter : public GameSubsystem {};'
Generate $false
Put "$scratch/Source/Other.h" ''
foreach ($bad in @('ENGINE_SUBSYSTEM(true) class A : public GameSubsystem {};',
    'ENGINE_SUBSYSTEM() class A;', 'template<class T> ENGINE_SUBSYSTEM() class A : public GameSubsystem {};',
    "#if 1`nENGINE_SUBSYSTEM() class A : public GameSubsystem {};`n#endif")) {
    Put "$scratch/Source/Counter.h" $bad
    Generate $false
}
# Delete only this run's known fixture header, then check the stale factory disappeared.
Remove-Item -LiteralPath "$scratch/Source/Counter.h"
Generate $true
if ((Get-Content "$scratch/Generated/GeneratedComponents.cpp" -Raw) -ne $empty) {throw 'Stale subsystem factory after deletion'}
Write-Output 'Subsystem generator: empty, add, remove, duplicate and malformed declarations passed'

param(
    [Parameter(Mandatory=$true)][string]$Project,
    [Parameter(Mandatory=$true)][string]$Destination,
    [string]$Runtime = (Join-Path $PSScriptRoot '../../Game/build/x64/Release/Runtime')
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $Runtime 'EngineRuntime.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Build the Release runtime before exporting.' }
& $exe --export-project ([IO.Path]::GetFullPath($Project)) --destination ([IO.Path]::GetFullPath($Destination))
if ($LASTEXITCODE) { throw 'Game export failed. See runtime output and staging logs.' }

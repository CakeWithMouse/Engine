param(
    [Parameter(Mandatory=$true)][string]$HostExe,
    [Parameter(Mandatory=$true)][string]$GameDll,
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$Content,
    [Parameter(Mandatory=$true)][string]$Fixtures,
    [Parameter(Mandatory=$true)][string]$OutputPrefix
)
$ErrorActionPreference = 'Stop'
$stdoutPath = "$OutputPrefix.stdout.log"
$stderrPath = "$OutputPrefix.stderr.log"
& $HostExe $GameDll $Runtime $Content $Fixtures 1> $stdoutPath 2> $stderrPath
$code = $LASTEXITCODE
[IO.File]::WriteAllText("$OutputPrefix.exit", "exit=$code`n")
exit $code

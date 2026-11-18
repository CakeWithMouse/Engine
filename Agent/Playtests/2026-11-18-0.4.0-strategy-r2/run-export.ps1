param(
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$Destination,
    [string]$Project = 'E:\Engine\Engine\build\playtests\strategy-040-r2\RepairCorridor\Project.json',
    [string]$OutputPrefix = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\export'
)
$ErrorActionPreference = 'Stop'
$script = 'E:\Engine\Engine\BuildSupport\Export-Project.ps1'
$stdoutPath = "$OutputPrefix.stdout.log"
$stderrPath = "$OutputPrefix.stderr.log"
& $script -Project $project -Destination $Destination -Runtime $Runtime 1> $stdoutPath 2> $stderrPath
$code = $LASTEXITCODE
[IO.File]::WriteAllText("$OutputPrefix.exit", "exit=$code`n")
exit $code

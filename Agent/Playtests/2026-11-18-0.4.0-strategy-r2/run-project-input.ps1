param(
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$Request,
    [Parameter(Mandatory=$true)][string]$OutputPrefix,
    [int]$ScreenshotFrame = 600
)
$ErrorActionPreference = 'Stop'
$commandlet = Join-Path $Runtime 'EngineCommandlets.exe'
if (!(Test-Path -LiteralPath $commandlet)) { throw "Missing EngineCommandlets.exe: $commandlet" }
$resultPath = "$OutputPrefix.result.json"
$stdoutPath = "$OutputPrefix.stdout.log"
$stderrPath = "$OutputPrefix.stderr.log"
$env:PAINENGINE_STRATEGY_TELEMETRY = "$OutputPrefix.telemetry.log"
$env:PAINENGINE_D3D_SCREENSHOT = "$OutputPrefix.frame.bmp"
$env:PAINENGINE_D3D_SCREENSHOT_FRAME = "$ScreenshotFrame"
try {
    & $commandlet project.input $Request $resultPath 1> $stdoutPath 2> $stderrPath
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText("$OutputPrefix.exit", "exit=$code`n")
    exit $code
}
finally {
    Remove-Item Env:PAINENGINE_STRATEGY_TELEMETRY -ErrorAction SilentlyContinue
    Remove-Item Env:PAINENGINE_D3D_SCREENSHOT -ErrorAction SilentlyContinue
    Remove-Item Env:PAINENGINE_D3D_SCREENSHOT_FRAME -ErrorAction SilentlyContinue
}

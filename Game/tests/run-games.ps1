param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
# Validate deployed assets, independent of developer overrides.
$env:ENGINE_CONTENT_ROOT = $null
$env:GAME_CONTENT_ROOT = $null
$repository = Split-Path $PSScriptRoot
$output = Join-Path $repository "build/x64/$Configuration"
$launcher = Join-Path $output 'EngineLauncher.exe'
$fixture = Join-Path $repository ('build/launcher-checks/' + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $fixture -Force | Out-Null

function Invoke-CheckedProcess([string]$File, [string]$Arguments, [int]$Expected = 0) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $File
    $info.Arguments = $Arguments
    $info.WorkingDirectory = $fixture # no source content here
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit(65000)) { $process.Kill(); throw "Timeout: $File $Arguments" }
    $log = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    $log | Set-Content -LiteralPath (Join-Path $fixture ((Split-Path $File -Leaf) + '-' + ($Arguments -replace '[^a-zA-Z0-9]', '_') + '.log'))
    if ($process.ExitCode -ne $Expected) { throw "Expected $Expected, got $($process.ExitCode): $File $Arguments`n$log" }
    Write-Host "PASS: $(Split-Path $File -Leaf) $Arguments => $Expected"
    $process.Dispose()
}

foreach ($game in @('BasicExamples','Pong','SunGame')) {
    Invoke-CheckedProcess (Join-Path $output "$game/$game.exe") '--smoke'
    Invoke-CheckedProcess $launcher "--check $game"
    Invoke-CheckedProcess $launcher "--smoke $game"
}
foreach ($scene in @('triangles','cube')) {
    Invoke-CheckedProcess (Join-Path $output 'BasicExamples/BasicExamples.exe') "--scene $scene --smoke"
}
Invoke-CheckedProcess (Join-Path $output 'BasicExamples/BasicExamples.exe') '--smoke --scene unknown' 1
if (Test-Path -LiteralPath (Join-Path $output 'Katamari/Katamari.exe')) {
    Invoke-CheckedProcess (Join-Path $output 'Katamari/Katamari.exe') '--smoke'
    Invoke-CheckedProcess $launcher '--smoke Katamari'
} else {
    Invoke-CheckedProcess $launcher '--check Katamari' 1
    Write-Host 'Katamari runtime: not built (requires Assimp).'
}
Invoke-CheckedProcess $launcher '--check UnknownGame' 2

# Isolated deployment fixtures: never rename/remove real binaries or assets.
$fixtureLauncher = Join-Path $fixture 'EngineLauncher.exe'
Copy-Item -LiteralPath $launcher -Destination $fixtureLauncher
Invoke-CheckedProcess $fixtureLauncher '--check BasicExamples' 1 # missing exe
$fixtureGame = Join-Path $fixture 'BasicExamples'
New-Item -ItemType Directory -Path $fixtureGame -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $output 'BasicExamples/BasicExamples.exe') -Destination (Join-Path $fixtureGame 'BasicExamples.exe')
Invoke-CheckedProcess $fixtureLauncher '--check BasicExamples' 1 # missing manifest
Set-Content -LiteralPath (Join-Path $fixtureGame 'RequiredFiles.txt') -Value 'EngineContent/Shaders/Common.hlsli' -Encoding ascii
Invoke-CheckedProcess $fixtureLauncher '--check BasicExamples' 1 # missing shader
Invoke-CheckedProcess (Join-Path $fixtureGame 'BasicExamples.exe') '--smoke' 1 # direct startup fails too
Set-Content -LiteralPath (Join-Path $fixtureGame 'RequiredFiles.txt') -Value 'GameContent/Models/missing.obj' -Encoding ascii
Invoke-CheckedProcess $fixtureLauncher '--check BasicExamples' 1 # missing game asset

$spaced = Join-Path $fixture 'deployment with spaces'
New-Item -ItemType Directory -Path $spaced -Force | Out-Null
Copy-Item -LiteralPath $launcher -Destination (Join-Path $spaced 'EngineLauncher.exe')
Copy-Item -LiteralPath (Join-Path $output 'BasicExamples') -Destination (Join-Path $spaced 'BasicExamples') -Recurse
Invoke-CheckedProcess (Join-Path $spaced 'EngineLauncher.exe') '--smoke BasicExamples'
Write-Host "Logs and isolated fixtures: $fixture"

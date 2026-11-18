param(
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$Project,
    [Parameter(Mandatory=$true)][int]$Frames,
    [Parameter(Mandatory=$true)][string]$OutputPrefix
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $Runtime 'EngineRuntime.exe'
$psi = [Diagnostics.ProcessStartInfo]::new($exe)
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
foreach ($arg in @('--project',$Project,'--mode','game','--frames',"$Frames")) { [void]$psi.ArgumentList.Add($arg) }
$process = [Diagnostics.Process]::Start($psi)
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (!$process.WaitForExit(120000)) { $process.Kill(); throw 'Runtime smoke timed out after 120 seconds' }
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
[IO.File]::WriteAllText("$OutputPrefix.stdout.log",$stdout)
[IO.File]::WriteAllText("$OutputPrefix.stderr.log",$stderr)
[IO.File]::WriteAllText("$OutputPrefix.exit",("exit={0}`n" -f $process.ExitCode))
exit $process.ExitCode

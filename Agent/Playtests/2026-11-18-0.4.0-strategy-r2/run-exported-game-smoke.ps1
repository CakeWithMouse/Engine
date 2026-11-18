param(
    [Parameter(Mandatory=$true)][string]$GameExe,
    [Parameter(Mandatory=$true)][int]$Frames,
    [Parameter(Mandatory=$true)][string]$OutputPrefix
)
$ErrorActionPreference = 'Stop'
$psi = [Diagnostics.ProcessStartInfo]::new($GameExe)
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
[void]$psi.ArgumentList.Add('--frames')
[void]$psi.ArgumentList.Add("$Frames")
$process = [Diagnostics.Process]::Start($psi)
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (!$process.WaitForExit(120000)) { $process.Kill(); throw 'Exported game smoke timed out after 120 seconds' }
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
[IO.File]::WriteAllText("$OutputPrefix.stdout.log",$stdout)
[IO.File]::WriteAllText("$OutputPrefix.stderr.log",$stderr)
[IO.File]::WriteAllText("$OutputPrefix.exit",("exit={0}`n" -f $process.ExitCode))
exit $process.ExitCode

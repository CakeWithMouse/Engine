# Invoke-ToolchainProcess creates a hidden console with the system code page.
# Set UTF-8 inside that console before CMake/Ninja starts cl.exe: /showIncludes
# paths must match Ninja's UTF-8 build files, even outside the system locale.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Toolchain.ps1')
$OutputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $OutputEncoding
[Console]::OutputEncoding = $OutputEncoding
# Avoid Windows PowerShell's native argument re-quoting for -DNAME=paths with
# spaces. Inherit this hidden console instead of allocating another one.
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = $args[0]
$start.Arguments = (($args | Select-Object -Skip 1) | ForEach-Object { ConvertTo-NativeArgument $_ }) -join ' '
$start.UseShellExecute = $false
$start.CreateNoWindow = $false
$start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
$process = [Diagnostics.Process]::Start($start)
try {
    $process.WaitForExit()
    exit $process.ExitCode
} finally { $process.Dispose() }

param([Nullable[bool]]$EnableAssimp = $null, [switch]$DisableAssimp, [string]$PlatformToolset = 'auto')
$ErrorActionPreference = 'Stop'
try {
    $repository = Split-Path $PSScriptRoot
    $buildArgs = @{ PlatformToolset = $PlatformToolset }
    if ($DisableAssimp) {
        $buildArgs['DisableAssimp'] = $true
    } elseif ($null -ne $EnableAssimp) {
        $buildArgs['EnableAssimp'] = $EnableAssimp
    }
    & (Join-Path $repository 'build.ps1') @buildArgs
    $launcher = Join-Path $repository '../Game/build/x64/Debug/EngineLauncher.exe'
    if (!(Test-Path -LiteralPath $launcher)) { throw "Launcher was not produced: $launcher" }
    # This is the interactive window explicitly requested by the user.
    Start-Process -FilePath $launcher -WorkingDirectory $repository
    exit 0
}
catch {
    Write-Host "Build failed. The launcher was NOT started.`n$($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace
    exit 1
}

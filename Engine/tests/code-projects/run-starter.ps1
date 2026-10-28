param([Parameter(Mandatory=$true)][string]$Project, [Parameter(Mandatory=$true)][string]$Runtime)
$ErrorActionPreference='Stop'
$sdk=Join-Path $Runtime 'SDK'
$pin=Get-Content -LiteralPath (Join-Path $Project 'Engine.lock.json') -Raw | ConvertFrom-Json
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=$msbuild; $start.UseShellExecute=$false
$start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
foreach($group in ([Environment]::GetEnvironmentVariables().Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1)) {
    $value=[Environment]::GetEnvironmentVariable($group.Group[0])
    foreach($key in $group.Group){[Environment]::SetEnvironmentVariable($key,$null,'Process')}
    [Environment]::SetEnvironmentVariable($group.Name,$value,'Process')
}
$start.EnvironmentVariables.Clear()
foreach($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()){ $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()]=$entry.Value }
$start.Arguments='"'+(Join-Path $PSScriptRoot 'StarterCubeTests.vcxproj')+'" /nologo /m:1 /nr:false /verbosity:minimal /p:Platform=x64 /p:Configuration='+$pin.engineConfiguration+' "/p:EngineSdk='+$sdk+'" "/p:StarterProject='+$Project+'"'
$p=[Diagnostics.Process]::Start($start)
$stdout=$p.StandardOutput.ReadToEndAsync(); $stderr=$p.StandardError.ReadToEndAsync(); $p.WaitForExit()
Write-Output $stdout.GetAwaiter().GetResult(); Write-Output $stderr.GetAwaiter().GetResult()
if($p.ExitCode){throw 'Starter tests failed to build'}
$output=Join-Path $Project 'Binaries/StarterTests'
Copy-Item -Path "$Runtime/Templates/SceneProject/*.dll" -Destination $output -Exclude Game.dll -Force
& "$output/StarterCubeTests.exe"
if($LASTEXITCODE){throw 'Starter cube behavior failed'}

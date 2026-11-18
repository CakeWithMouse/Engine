param(
    [Parameter(Mandatory=$true)][string]$MSBuild,
    [Parameter(Mandatory=$true)][string]$Runtime
)
$ErrorActionPreference = 'Stop'
$project = 'E:\Engine\Engine\build\playtests\strategy-040-r2\MeshPersistenceHost\MeshPersistenceHost.vcxproj'
$log = 'E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\mesh-host-build.log'
$sdkInclude = Join-Path $Runtime 'SDK\Include'
& $MSBuild $project '/m' '/t:Rebuild' '/p:Configuration=Release' '/p:Platform=x64' "/p:RuntimeSdkInclude=$sdkInclude" "/flp:logfile=$log;verbosity=normal"
$code = $LASTEXITCODE
[IO.File]::WriteAllText('E:\Engine\Agent\Playtests\2026-11-18-0.4.0-strategy-r2\mesh-host-build.exit', "exit=$code`n")
exit $code

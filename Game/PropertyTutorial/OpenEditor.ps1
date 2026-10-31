$ErrorActionPreference='Stop'
$pin=Get-Content -LiteralPath "$PSScriptRoot/Engine.lock.json" -Raw | ConvertFrom-Json
$store=if($env:PAINENGINE_ENGINE_STORE){$env:PAINENGINE_ENGINE_STORE}else{Join-Path $env:LOCALAPPDATA 'PainEngine/Engines'}
$runtime=Join-Path $store ($pin.engineVersion+'/'+$pin.engineConfiguration)
if(!(Test-Path -LiteralPath "$runtime/EngineRuntime.exe")){throw "Install PainEngine $($pin.engineVersion) $($pin.engineConfiguration) through EngineLauncher first."}
$launch=New-Object System.Diagnostics.ProcessStartInfo
$launch.FileName="$runtime/EngineRuntime.exe"
$launch.Arguments='--project "'+$PSScriptRoot+'/Project.json" --mode editor --scene-api'
$launch.WorkingDirectory=$PSScriptRoot
$launch.UseShellExecute=$false
$launch.CreateNoWindow=$true # Hide only the console; the Qt editor stays visible.
[Diagnostics.Process]::Start($launch) | Out-Null

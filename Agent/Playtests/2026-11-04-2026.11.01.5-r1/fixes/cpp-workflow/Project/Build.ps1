param([string]$Runtime='')
$ErrorActionPreference='Stop'
$pin=Get-Content -LiteralPath "$PSScriptRoot/Engine.lock.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if(!$Runtime){
 $store=if($env:PAINENGINE_ENGINE_STORE){$env:PAINENGINE_ENGINE_STORE}else{Join-Path $env:LOCALAPPDATA 'PainEngine/Engines'}
 $Runtime=Join-Path $store ($pin.engineVersion+'/'+$pin.engineConfiguration)
}
& "$Runtime/SDK/ProjectTools.ps1" -Action Build -Project "$PSScriptRoot/Project.json" -Runtime $Runtime
param([string]$Output='', [switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$SkipBuild){& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration Release -EngineCommandlet}
if(!$Output){$Output="$repo/Engine/build/point-light-clone-"+[guid]::NewGuid().ToString('N')}
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
$env:TEMP=$Output; $env:TMP=$Output
$env:PAINENGINE_ENGINE_STORE="$Output/engine-store"
$project="$Output/project"
$p=Start-Process -FilePath "$repo/Game/build/x64/Release/EngineLauncher.exe" -ArgumentList @('--create-project',$project,'ShadowClone') -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode){throw 'Cannot create clone fixture'}
$scenePath="$project/Content/Scenes/Main.scene.json"
$scene=Get-Content -Raw -LiteralPath $scenePath | ConvertFrom-Json
$light=@($scene.objects | Where-Object type -eq 'Light.Point')[0]
if(!$light){throw 'Template must provide a point light'}
$light.pointLight | Add-Member -NotePropertyName castShadows -NotePropertyValue $true -Force
$scene | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $scenePath -Encoding utf8
$saved="$Output/cloned.scene.json"
$roundtrip="$Output/reloaded.scene.json"
$request=@{project="$project/Project.json";steps=@(
    @{action='duplicate';target=$light.name},
    @{action='save';path=$saved},
    @{action='new'},
    @{action='load';path=$saved},
    @{action='save';path=$roundtrip}
)}
$request | ConvertTo-Json -Depth 20 | Set-Content "$Output/request.json" -Encoding utf8
& "$repo/Game/build/x64/Release/Runtime/EngineCommandlets.exe" project.scene "$Output/request.json" "$Output/result.json" *> "$Output/commandlet.log"
if($LASTEXITCODE){throw 'Scene commandlet failed'}
$result=Get-Content -Raw "$Output/result.json" | ConvertFrom-Json
foreach($step in $result.steps){if($step.status -ne 0){throw 'Scene operation failed'}}
foreach($path in @($saved,$roundtrip)){
    $document=Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
    foreach($name in @($light.name,($light.name+' 2'))){
        $object=@($document.objects | Where-Object name -eq $name)
        if($object.Count -ne 1 -or !$object[0].pointLight.castShadows){throw "BUG-R5-003: $name lost castShadows in $path"}
    }
}
Write-Output "Point-light duplicate/save/new/load: passed. Evidence: $Output"

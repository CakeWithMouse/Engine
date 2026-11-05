param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$SkipBuild){& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EngineCommandlet -EnableAssimp}
$runtime=Join-Path $repo "Game/build/x64/$Configuration/Runtime"
$scratch=Join-Path $repo ('Engine/build/savegame-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$count=0
function Invoke-SaveCommand($name,$request,[int]$expected=0){
    $script:count++
    $inputFile=Join-Path $scratch "$script:count-request.json"
    $outputFile=Join-Path $scratch "$script:count-result.json"
    [IO.File]::WriteAllText($inputFile,($request|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
    & "$runtime/EngineCommandlets.exe" $name $inputFile $outputFile | Out-Null
    if($LASTEXITCODE -ne $expected){throw "${name}: exit $LASTEXITCODE, expected $expected"}
    if(!(Test-Path $outputFile)){throw 'Commandlet did not start or did not produce JSON'}
    return Get-Content -Raw $outputFile | ConvertFrom-Json
}
$scene=@{version=3;name='Сохранённая сцена';objects=@(@{id=1;type='Primitive.Cube';name='Cube';parent=0;inheritParentScale=$true;transform=@{position=@(2,0,4);rotation=@(0,0,0,1);scale=@(1,1,1)}})}
$request=@{root=$scratch;gameId='commandlet-test';slot='slot-1';scene=$scene}
$saved=Invoke-SaveCommand 'savegame.save' $request
$loaded=Invoke-SaveCommand 'savegame.load' $request
if($loaded.state.scene.name -ne $scene.name -or $loaded.state.scene.objects[0].transform.position[0] -ne 2){throw 'Scene not restored'}
$null=Invoke-SaveCommand 'savegame.validate' $request
$listed=Invoke-SaveCommand 'savegame.list' $request
if($listed.slots[0] -ne 'slot-1'){throw 'Slot list mismatch'}
$file=Join-Path $scratch 'commandlet-test/default/slot-1.save.json'
$original=[IO.File]::ReadAllText($file)
$lock=[IO.File]::Open($file,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try{$null=Invoke-SaveCommand 'savegame.save' $request 2}finally{$lock.Dispose()}
if([IO.File]::ReadAllText($file) -ne $original){throw 'Write failure damaged slot'}
$null=Invoke-SaveCommand 'savegame.load' $request
[IO.File]::WriteAllText($file,'{broken')
$null=Invoke-SaveCommand 'savegame.validate' $request 2
[IO.File]::WriteAllText($file,$original)
$request.slot='../escape'
$null=Invoke-SaveCommand 'savegame.save' $request 2
$request.slot='slot-1'
$null=Invoke-SaveCommand 'savegame.delete' $request
if(Test-Path $file){throw 'Delete failed'}
Write-Output "SaveGame commandlets: $count calls passed. Fixture: $scratch"

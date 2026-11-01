param([Parameter(Mandatory=$true)][string]$LegacyProject,
      [Parameter(Mandatory=$true)][string]$Runtime)
$ErrorActionPreference='Stop'
# Input: a previously built starter C++ project with RotatingCube and property annotations.
# All mutations happen in test copies, never in the supplied historical project.
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$work=Join-Path $repo ('Engine/build/property-upgrade/'+[guid]::NewGuid().ToString('N').Substring(0,8))
$source=Join-Path $work 'Old'
New-Item -ItemType Directory -Path $source -Force | Out-Null
Copy-Item -Path (Join-Path $LegacyProject '*') -Destination $source -Recurse
$encoding=[Text.UTF8Encoding]::new($false)
$manifest=Get-Content "$source/Project.json" -Raw | ConvertFrom-Json
$sceneFile=Join-Path $source $manifest.startupScene
$scene=@{version=3;name='Legacy properties';objects=@(@{
    id=1;name='Saved cube';type='RotatingCube';parent=0;inheritParentScale=$true
    transform=@{position=@(0,0,0);rotation=@(0,0,0,1);scale=@(1,1,1)}
    projectData=@{version=1;speed=237.5;enabled=$false}
})}
[IO.File]::WriteAllText($sceneFile,($scene|ConvertTo-Json -Depth 12),$encoding)
$files=@('Project.json','Engine.lock.json',$manifest.module,$manifest.startupScene,'Source/RotatingCube.h')
$hashes=@{};foreach($file in $files){$hashes[$file]=(Get-FileHash -LiteralPath (Join-Path $source $file)).Hash}
$destination=Join-Path $work 'Upgraded'
& "$Runtime/EngineRuntime.exe" --upgrade-project "$source/Project.json" --destination $destination
if($LASTEXITCODE){throw 'Property project migration failed'}
$targetPin=Get-Content "$destination/Engine.lock.json" -Raw | ConvertFrom-Json
$release=Get-Content "$Runtime/EngineVersion.json" -Raw | ConvertFrom-Json
if($targetPin.engineVersion -ne $release.version){throw 'Copy was not pinned to new engine'}
if((Get-FileHash (Join-Path $destination $manifest.module)).Hash -ne $hashes[$manifest.module]){throw 'Custom DLL replaced during upgrade'}
function CheckData {
    $saved=Get-Content (Join-Path $destination $manifest.startupScene) -Raw | ConvertFrom-Json
    $data=$saved.objects[0].projectData
    if($data.speed -ne 237.5 -or $data.enabled -ne $false){throw 'Saved property values changed'}
    foreach($file in $files){if((Get-FileHash -LiteralPath (Join-Path $source $file)).Hash -ne $hashes[$file]){throw "Original changed: $file"}}
}
CheckData
# Compile the unchanged legacy declaration against the new base class and SDK.
& "$Runtime/SDK/ProjectTools.ps1" -Action Build -Project "$destination/Project.json" -Runtime $Runtime
if((Get-FileHash "$destination/Source/RotatingCube.h").Hash -ne $hashes['Source/RotatingCube.h']){throw 'Build rewrote legacy source'}
CheckData
# The same saved scene must also load after removing the now redundant inheritance.
$header="$destination/Source/RotatingCube.h"
$text=[IO.File]::ReadAllText($header)
if(!$text.Contains(', public EditorPropertyProvider')){throw 'Fixture must contain the legacy property interface'}
[IO.File]::WriteAllText($header,$text.Replace(', public EditorPropertyProvider',''),$encoding)
& "$Runtime/SDK/ProjectTools.ps1" -Action Build -Project "$destination/Project.json" -Runtime $Runtime
CheckData
Write-Output "Property upgrade: PASS (legacy DLL, unchanged sources rebuilt, interface removed, saved values and original preserved). $work"

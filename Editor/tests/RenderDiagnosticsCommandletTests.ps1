param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$SkipBuild){& "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EngineCommandlet -EnableAssimp}
$runtime=Join-Path $repo "Game/build/x64/$Configuration/Runtime"
$scratch=Join-Path $repo ('Engine/build/render-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$count=0
function Invoke-Render($request,[int]$expected=0){
    $script:count++
    $inputFile=Join-Path $scratch "$script:count-request.json"
    $outputFile=Join-Path $scratch "$script:count-result.json"
    [IO.File]::WriteAllText($inputFile,($request|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
    & "$runtime/EngineCommandlets.exe" render.diagnostics $inputFile $outputFile *> (Join-Path $scratch "$script:count.log")
    if($LASTEXITCODE -ne $expected){throw "render.diagnostics: exit $LASTEXITCODE, expected $expected; $scratch"}
    return Get-Content -Raw $outputFile | ConvertFrom-Json
}
$request=@{instances=@(@{position=@(0,0,0)});screenHeights=@(50,20);views=@(@{position=@(0,8,0);orthoHeight=24},@{position=@(0,24,0);orthoHeight=24},@{position=@(0,24,0);orthoHeight=8})}
$ortho=Invoke-Render $request
if(($ortho.frames[0].perLod -join ',') -ne ($ortho.frames[1].perLod -join ',')){throw "BUG-R2-001: orthographic LOD changed with camera distance; $scratch"}
if($ortho.frames[0].perLod[1] -ne 1 -or $ortho.frames[2].perLod[0] -ne 1){throw 'Orthographic zoom must change projected-size LOD'}
if($ortho.frames[0].previousValid -or !$ortho.frames[1].previousValid -or $ortho.frames[2].frameIndex -ne 3){throw 'Completed-frame publication mismatch'}
$request.views=@(@{position=@(0,8,0);orthographic=$false},@{position=@(0,24,0);orthographic=$false})
$perspective=Invoke-Render $request
if($perspective.frames[0].perLod[0] -ne 1 -or $perspective.frames[1].perLod[1] -ne 1){throw 'Perspective projected-size LOD regression'}
$request=@{instances=@(@{position=@(0,0,0)},@{position=@(0,0,0);visible=$false},@{position=@(100,0,0)},@{position=@(25,0,0)});drawDistance=60;image=(Join-Path $scratch 'diagnostics.bmp')}
$result=Invoke-Render $request
$frame=$result.frames[0]
if($frame.total -ne 4 -or $frame.drawn -ne 1 -or $frame.hidden -ne 1 -or $frame.distanceCulled -ne 1 -or $frame.frustumCulled -ne 1){throw 'Visibility reasons do not partition instances'}
if($frame.drawCalls -lt 1 -or !(Test-Path $request.image)){throw 'Real rendering/frame dump missing'}
$null=Invoke-Render $request 2 # refuses overwriting an existing image
$request.Remove('image')
$request.rendering=2
$custom=Invoke-Render $request
if(!$custom.frames[0].prepared -or $custom.frames[0].drawn -ne 1){throw 'Base Game custom pass did not publish its scene draw'}
$null=Invoke-Render @{width=1} 2
$null=Invoke-Render @{instances=@(@{position=@(0,'bad',0)})} 2
$null=Invoke-Render @{screenHeights=@(1e100)} 2
$null=Invoke-Render @{views=@(@{direction=@(0,1,0);up=@(0,1,0)})} 2
# Verify GPU placement, not only CPU submission counters: default InstancedMesh
# must draw three separate copies without requiring internal shader selection.
$placement=Join-Path $scratch 'instance-placement.bmp'
$null=Invoke-Render @{panel=$false;image=$placement;instances=@(@{position=@(-3,0,0)},@{position=@(0,0,0)},@{position=@(3,0,0)});views=@(@{position=@(0,24,0);orthoHeight=12})}
Add-Type -AssemblyName System.Drawing
$bitmap=[Drawing.Bitmap]::new($placement)
try{
    foreach($x in @(200,320,440)){
        $pixel=$bitmap.GetPixel($x,240)
        if([int]$pixel.R+[int]$pixel.G+[int]$pixel.B -lt 150){throw "BUG-R2-004: expected a visible instance at pixel $x,240; $scratch"}
    }
    foreach($x in @(260,380)){
        $pixel=$bitmap.GetPixel($x,240)
        if([int]$pixel.R+[int]$pixel.G+[int]$pixel.B -gt 100){throw 'Instances overlap the expected gaps'}
    }
}finally{$bitmap.Dispose()}
Write-Output "Render diagnostics commandlets: $count calls passed. Fixture: $scratch"

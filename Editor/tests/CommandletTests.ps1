param([string]$Output = "$PSScriptRoot/../../Engine/build/commandlet-results", [switch]$Observe)
$ErrorActionPreference='Stop'
$exe=[IO.Path]::GetFullPath("$PSScriptRoot/../../Game/build/x64/Release/Runtime/EngineCommandlets.exe")
$Output=[IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory($Output)|Out-Null
$results=[Collections.Generic.List[object]]::new()
function Case($id,$command,$inputData,$check,$expectedExit=0) {
    $request=Join-Path $Output "$id.request.json"; $result=Join-Path $Output "$id.result.json"
    $inputData|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $request -Encoding utf8
    if(Test-Path -LiteralPath $result){Remove-Item -LiteralPath $result}
    & $exe $command $request $result *> (Join-Path $Output "$id.console.log")
    $exit=$LASTEXITCODE
    $data=if(Test-Path $result){Get-Content $result -Raw|ConvertFrom-Json}else{$null}
    $passed=$exit -eq $expectedExit -and $null -ne $data -and (& $check $data)
    $results.Add([pscustomobject]@{id=$id;command=$command;passed=[bool]$passed;exit=$exit})
    Write-Output "$id : $passed"
}
Case BUG001 ui.inspector @{width=310;image="$Output/inspector.png"} {param($v) $v.fields.Count -eq 9 -and $v.reachable}
Case BUG002 ui.style @{image="$Output/style.png"} {param($v) @($v.warnings|Where-Object {$_ -match 'QString::arg|parse|stylesheet'}).Count -eq 0}
$ui=Join-Path $Output 'pending.ui'
'<UI><Label name="label" text="Before" x="0" y="0" width="200" height="30" /></UI>'|Set-Content $ui -Encoding utf8
Case BUG003 ui.edit-text @{file=$ui;element='label';text='Баланс: 125 ₽'} {param($v) $v.saved.Contains('Баланс: 125 ₽')}
Case PendingClose ui.edit-text @{file=$ui;element='label';text='Не терять ввод';close='cancel'} {param($v) !$v.closeAccepted -and $v.pending -eq 'Не терять ввод'}
Case BUG007 input.rebind @{focused=$true} {param($v) $v.oldFocus -eq 1 -and $v.newFocus -eq 1}
Case FocusOff input.rebind @{focused=$false} {param($v) $v.newFocus -eq 0}
Case TraceHit trace.raycast @{origin=@(0,0,5);direction=@(0,0,-1);distance=10} {param($v) $v.hit -and [Math]::Abs($v.distance-5) -lt 0.001}
Case TraceMiss trace.raycast @{origin=@(5,0,5);direction=@(0,0,-1)} {param($v) !$v.hit}
Case Snap gizmo.snap @{delta=@(-1.6378729,0,0);step=0.5} {param($v) $v.delta[0] -eq -1.5}
Case BUG008 gizmo.center @{delta=@(-1.6378729,0,0);step=0.5} {param($v) $v.position[0] -eq -1.5}
Case SnapOff gizmo.center @{delta=@(-1.6378729,0,0);snap=$false} {param($v) [Math]::Abs($v.position[0]+1.6378729) -lt 0.0001}
Case ZeroStep gizmo.center @{delta=@(-1.6378729,0,0);step=0} {param($v) [Math]::Abs($v.position[0]+1.6378729) -lt 0.0001}
Case CameraPerspective camera.screen-ray @{origin=@(0,0,5)} {param($v) [Math]::Abs($v.direction[2]+1) -lt 0.001 -and [Math]::Abs($v.origin[2]-4.9) -lt 0.001}
Case CameraXZ camera.screen-ray @{origin=@(0,5,0);direction=@(0,-1,0);orthographic=$true;x=0;y=0} {param($v) [Math]::Abs($v.direction[1]+1) -lt 0.001 -and [Math]::Abs($v.origin[0]+10) -lt 0.001}
Case Session session.lifecycle @{scenes=2;frames=3;dt=0.1} {param($v) $v.ready -eq 2 -and $v.closing -eq 2 -and $v.ticks -eq 6 -and !$v.running}
Case SessionDisabled session.lifecycle @{scenes=2;frames=3;tickEnabled=$false} {param($v) $v.ready -eq 2 -and $v.ticks -eq 0}
Case CameraProperty scene.property @{type='Gameplay.Camera';path='/fov';value=75} {param($v) $v.status -eq 0 -and $v.data.fov -eq 75}
Case CameraInvalid scene.property @{type='Gameplay.Camera';path='/fov';value=-1} {param($v) $v.status -ne 0 -and $v.data.fov -gt 0}
Case SnapLocal gizmo.center @{delta=@(0.2,0,-1.6378729);rotation=@(0,0.70710678,0,0.70710678);local=$true;step=0.5} {param($v) [Math]::Abs($v.position[2]+1.5) -lt 0.001 -and [Math]::Abs($v.position[0]) -lt 0.001}
Case BadVector trace.raycast @{origin=@(1,2)} {param($v) !$v.ok -and $v.error.Contains('three numbers')} 2
Case BadViewport camera.screen-ray @{width=0} {param($v) !$v.ok -and $v.error.Contains('positive')} 2
Case UnknownCommand missing.command @{} {param($v) !$v.ok -and $v.error.Contains('unknown command')} 2
Case GameSettings game.settings @{rendering=1;vsync=$false;antialiasing=1} {param($v) $v.rendering -eq 1 -and !$v.vsync -and $v.antialiasing -eq 1}
Case Cube mesh.primitive @{shape='cube';size=4} {param($v) $v.triangles -eq 12 -and $v.minimum[0] -eq -2 -and $v.maximum[0] -eq 2}
Case Plane mesh.primitive @{shape='plane';size=2} {param($v) $v.triangles -eq 2 -and $v.minimum[1] -eq 0 -and $v.maximum[2] -eq 1}
$results|ConvertTo-Json|Set-Content (Join-Path $Output 'results.json') -Encoding utf8
if(!$Observe -and @($results|Where-Object {!$_.passed}).Count){throw 'Commandlet regression failed; see results.json'}

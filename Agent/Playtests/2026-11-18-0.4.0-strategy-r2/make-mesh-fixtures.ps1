$ErrorActionPreference='Stop'
$out='E:\Engine\Engine\build\playtests\strategy-040-r2\fixtures'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function New-Obj([string]$path, [System.Collections.Generic.List[double[]]]$boxes, [bool]$unusedVertex) {
    $lines=[System.Collections.Generic.List[string]]::new()
    $lines.Add('# Self-authored collision persistence fixture')
    $lines.Add('s off')
    $v=1
    $faces=@(@(1,2,3),@(1,3,4),@(5,8,7),@(5,7,6),@(1,4,8),@(1,8,5),@(2,6,7),@(2,7,3),@(1,5,6),@(1,6,2),@(4,3,7),@(4,7,8))
    foreach($b in $boxes) {
        $x0=$b[0];$y0=$b[1];$z0=$b[2];$x1=$b[3];$y1=$b[4];$z1=$b[5]
        $corners=[System.Collections.Generic.List[double[]]]::new()
        $corners.Add([double[]]@($x0,$y0,$z0));$corners.Add([double[]]@($x1,$y0,$z0))
        $corners.Add([double[]]@($x1,$y1,$z0));$corners.Add([double[]]@($x0,$y1,$z0))
        $corners.Add([double[]]@($x0,$y0,$z1));$corners.Add([double[]]@($x1,$y0,$z1))
        $corners.Add([double[]]@($x1,$y1,$z1));$corners.Add([double[]]@($x0,$y1,$z1))
        foreach($p in $corners) {
            $culture=[Globalization.CultureInfo]::InvariantCulture
            $lines.Add(('v '+$p[0].ToString('R',$culture)+' '+$p[1].ToString('R',$culture)+' '+$p[2].ToString('R',$culture)))
        }
        foreach($f in $faces) { $lines.Add(('f {0} {1} {2}' -f ($v+$f[0]-1),($v+$f[1]-1),($v+$f[2]-1))) }
        $v+=8
    }
    if($unusedVertex) {$lines.Add('v 1.05 0 0')}
    [IO.File]::WriteAllLines($path,$lines,[Text.UTF8Encoding]::new($false))
}
$arch=[System.Collections.Generic.List[double[]]]::new()
$arch.Add([double[]]@(-2.70,-0.95,-0.75,-0.75,0.95,0.75))
$arch.Add([double[]]@(1.25,-0.95,-0.75,2.70,0.95,0.75))
$arch.Add([double[]]@(-2.70,0.95,-0.75,2.70,1.45,0.75))
$tray=[System.Collections.Generic.List[double[]]]::new()
$tray.Add([double[]]@(-0.90,-0.70,-0.70,0.90,-0.52,0.70))
$tray.Add([double[]]@(-0.90,-0.60,-0.70,-0.72,0.60,0.70))
$tray.Add([double[]]@(0.72,-0.60,-0.70,0.90,0.60,0.70))
$tray.Add([double[]]@(-0.90,-0.60,0.52,0.90,0.60,0.70))
New-Obj (Join-Path $out 'StaticMeshArch.obj') $arch $true
New-Obj (Join-Path $out 'OpenCargo.obj') $tray $true
Get-FileHash (Join-Path $out 'StaticMeshArch.obj'),(Join-Path $out 'OpenCargo.obj') -Algorithm SHA256

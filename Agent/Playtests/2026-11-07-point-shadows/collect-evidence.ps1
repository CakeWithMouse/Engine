$ErrorActionPreference='Stop'
python "$PSScriptRoot/collect-evidence.py"
if($LASTEXITCODE){throw 'Evidence collection failed'}
Add-Type -AssemblyName System.Drawing
$source='E:/Engine/Engine/build/playtests/20261107-point'
$names=@('baseline-crowd-5','fixed-crowd-5')
$final=@('import-r0-skeletal-00','import-r0-skeletal-02','import-r1-crowd-03',
    'motion-r1-scene1-crowd-manual0-01','motion-r1-scene1-crowd-manual0-05',
    'seam-r1-manual0-00','seam-r1-manual0-15','seam-r1-manual0-30')
foreach($name in $names+$final){
    $folder=if($name -in $names){$source}else{"$source/final-motion"}
    $bitmap=[Drawing.Bitmap]::FromFile("$folder/$name.bmp")
    try {$bitmap.Save("$PSScriptRoot/visual/$name.png",[Drawing.Imaging.ImageFormat]::Png)}
    finally {$bitmap.Dispose()}
}

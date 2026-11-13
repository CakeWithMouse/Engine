$ErrorActionPreference='Stop'
$source='E:/Engine/Engine/build/playtests/20261107-two-lights'
foreach($name in @('baseline','final','packaged','single-packaged')){
    if(Test-Path -LiteralPath "$source/$name"){
        New-Item -ItemType Directory -Force "$PSScriptRoot/fixes/$name" | Out-Null
        Copy-Item -Path "$source/$name/*.json" -Destination "$PSScriptRoot/fixes/$name"
    }
}
New-Item -ItemType Directory -Force "$PSScriptRoot/logs","$PSScriptRoot/visual" | Out-Null
Copy-Item -Path "$source/*.log" -Destination "$PSScriptRoot/logs"
if(Test-Path -LiteralPath "$source/package-smoke.json"){
    Copy-Item -LiteralPath "$source/package-smoke.json" -Destination "$PSScriptRoot/package-smoke.json"
}
Add-Type -AssemblyName System.Drawing
foreach($phase in @('baseline','final')){
    foreach($name in @('slots-r1-crowd-00','slots-r1-crowd-07','fade-r1-0-04','fade-r1-0-07','fade-r1-0-08')){
        $bitmap=[Drawing.Bitmap]::FromFile("$source/$phase/$name.bmp")
        try {$bitmap.Save("$PSScriptRoot/visual/$phase-$name.png",[Drawing.Imaging.ImageFormat]::Png)}
        finally {$bitmap.Dispose()}
    }
}
foreach($name in @('joint-r1-crowd-04','joint-r1-crowd-10','joint-r1-skeletal-10')){
    $bitmap=[Drawing.Bitmap]::FromFile("$source/final/$name.bmp")
    try {$bitmap.Save("$PSScriptRoot/visual/$name.png",[Drawing.Imaging.ImageFormat]::Png)}
    finally {$bitmap.Dispose()}
}

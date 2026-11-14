param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'loopback-capture.cpp'
if(!(Test-Path -LiteralPath $source)){throw "Missing source: $source"}
$output=Join-Path $PSScriptRoot "LoopbackCapture-$Configuration.exe"
$intermediate=Join-Path $PSScriptRoot "LoopbackCapture-$Configuration.obj"
$pdb=Join-Path $PSScriptRoot "LoopbackCapture-$Configuration.pdb"
$cl=Get-Command cl.exe -ErrorAction SilentlyContinue
if($cl){ & $cl.Source /nologo /std:c++17 /EHsc /O2 /DWIN32_LEAN_AND_MEAN "/Fe:$output" "/Fo:$intermediate" "/Fd:$pdb" $source; if($LASTEXITCODE -ne 0){throw "cl.exe failed: $LASTEXITCODE"} }
else {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(!(Test-Path -LiteralPath $vswhere)){throw 'cl.exe is not on PATH and vswhere.exe was not found. Run from Developer PowerShell for VS 2022.'}
    $vs=@(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if(!$vs){throw 'Visual Studio C++ tools are not installed.'}
    $vcvars=Join-Path $vs[0] 'VC\Auxiliary\Build\vcvars64.bat'
    if(!(Test-Path -LiteralPath $vcvars)){throw "Missing VS x64 environment script: $vcvars"}
    $cmdLine='call "'+$vcvars+'" >nul && cl.exe /nologo /std:c++17 /EHsc /O2 /DWIN32_LEAN_AND_MEAN "/Fe:'+ $output +'" "/Fo:'+ $intermediate +'" "/Fd:'+ $pdb +'" "'+$source+'"'
    & $env:ComSpec /d /c $cmdLine
    if($LASTEXITCODE -ne 0){throw "cl.exe failed: $LASTEXITCODE"}
}
Write-Output "Built $output from $source"


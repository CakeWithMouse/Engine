param(
    [Parameter(Mandatory=$true)][string]$DestinationRoot,
    [string]$SdkDirectory = "${env:ProgramFiles(x86)}/Windows Kits/10"
)
$ErrorActionPreference = 'Stop'
# The OS-serviced System32 copy may import private APIs from a newer Windows.
# Ship only the SDK redistributable, even when windeployqt chose another copy.
$source = Join-Path $SdkDirectory 'Redist/D3D/x64/d3dcompiler_47.dll'
if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Windows SDK x64 D3D redistributable is missing: $source" }
$signature = Get-AuthenticodeSignature -LiteralPath $source
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw 'D3D compiler must have a valid Microsoft signature'
}
$bytes = [IO.File]::ReadAllBytes($source)
$pe = [BitConverter]::ToInt32($bytes,0x3c)
if ([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) { throw 'D3D compiler must be x64' }
if ([Text.Encoding]::ASCII.GetString($bytes).IndexOf('msvcp_win.dll',[StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw 'D3D compiler depends on the OS C++ runtime; do not redistribute the System32 copy'
}
$root = (Resolve-Path -LiteralPath $DestinationRoot).Path
$targets = @(Get-ChildItem -LiteralPath $root -File -Recurse -Filter 'D3Dcompiler_47.dll')
if (!$targets.Count) { throw 'No packaged D3D compiler found; check deployment layout' }
$hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
foreach ($target in $targets) {
    Copy-Item -LiteralPath $source -Destination $target.FullName -Force
    if ((Get-FileHash -LiteralPath $target.FullName -Algorithm SHA256).Hash -ne $hash) { throw 'D3D compiler deployment verification failed' }
}
Write-Output "Deployed Windows SDK D3D compiler to $($targets.Count) locations; SHA256 $hash"

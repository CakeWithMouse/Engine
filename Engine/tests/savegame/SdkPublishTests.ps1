$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$fixture=Join-Path $repo ('Engine/build/sdk-publish-'+[guid]::NewGuid().ToString('N'))
$sdk=Join-Path $fixture 'SDK'
New-Item -ItemType Directory -Path "$sdk/Lib" -Force | Out-Null
Copy-Item -LiteralPath "$repo/Engine/Version.json" -Destination "$fixture/EngineVersion.json"
[IO.File]::WriteAllText("$sdk/Lib/Engine.lib",'old library')
[IO.File]::WriteAllText("$sdk/old-marker",'old complete SDK')
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SdkDirectoryLock {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern IntPtr CreateFile(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
$lock=[SdkDirectoryLock]::CreateFile($sdk,1,3,[IntPtr]::Zero,3,0x02000000,[IntPtr]::Zero)
if($lock -eq [IntPtr](-1)){throw 'Cannot lock fixture SDK directory'}
$failed=$false
try {
    try { & "$repo/Engine/BuildSupport/New-ProjectSdk.ps1" -Runtime $fixture -Configuration Release -PlatformToolset v143 }
    catch { $failed=$true }
} finally { [void][SdkDirectoryLock]::CloseHandle($lock) }
if(!$failed){throw 'Expected locked SDK publication to fail'}
if([IO.File]::ReadAllText("$sdk/Lib/Engine.lib") -ne 'old library' -or !(Test-Path "$sdk/old-marker") -or (Test-Path "$sdk/Include")){throw 'Failed publish mixed old and new SDK files'}
if(@(Get-ChildItem $fixture -Filter '.sdk-stage-*').Count){throw 'Stage directory leaked'}
& "$repo/Engine/BuildSupport/New-ProjectSdk.ps1" -Runtime $fixture -Configuration Release -PlatformToolset v143
if((Test-Path "$sdk/old-marker") -or !(Test-Path "$sdk/Include/Engine/SaveGame/SaveGame.h") -or !(Test-Path "$sdk/Sdk.json")){throw 'Retry did not publish complete SDK'}
if((Get-FileHash "$sdk/Lib/Engine.lib").Hash -ne (Get-FileHash "$repo/Engine/build/x64/Release/Engine.lib").Hash){throw 'SDK library mismatch'}
Write-Output "SDK publication: locked failure preserved old files; retry published complete SDK. Fixture: $fixture"

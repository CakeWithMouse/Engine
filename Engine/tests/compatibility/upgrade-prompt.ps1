param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if(!$SkipBuild){
    & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Game/Launcher/EngineLauncher.vcxproj" -Configuration Release -EnableAssimp
}
$root=Join-Path $repo ('Engine/build/prompt/'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $root | Out-Null
$env:LOCALAPPDATA=Join-Path $root 'AppData'
$env:PAINENGINE_ENGINE_STORE=Join-Path $root 'EmptyStore'
$env:TEMP=$root;$env:TMP=$root
$runtime=Join-Path $repo 'Game/build/x64/Release/Runtime'
$launcher=Join-Path $repo 'Game/build/x64/Release/EngineLauncher.exe'
$version=(Get-Content "$runtime/EngineVersion.json" -Raw|ConvertFrom-Json).version
Expand-Archive -LiteralPath "$PSScriptRoot/fixtures/pre-lights.zip" -DestinationPath "$root/frozen"
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class PromptTest {
    delegate bool Visit(IntPtr window,IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Visit callback,IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window,StringBuilder text,int count);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    public static IntPtr Find(int pid) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((window,data)=>{uint owner;GetWindowThreadProcessId(window,out owner);
            if(owner==pid && GetDlgItem(window,211)!=IntPtr.Zero){found=window;return false;}return true;},IntPtr.Zero);
        return found;
    }
    public static string Text(IntPtr window) {var text=new StringBuilder(4096);GetWindowText(window,text,text.Capacity);return text.ToString();}
}
'@
function Fixture([string]$name) {
    $folder=Join-Path $root $name
    New-Item -ItemType Directory -Path "$folder/Content/Scenes" -Force|Out-Null
    Get-ChildItem "$root/frozen" -Filter '*.dll'|Copy-Item -Destination $folder
    [IO.File]::WriteAllText("$folder/Project.json",'{"name":"Old game","module":"Game.dll","content":"Content","startupScene":"Content/Scenes/Main.scene.json"}')
    [IO.File]::WriteAllText("$folder/Engine.lock.json",'{"format":1,"engineVersion":"2026.10.24.0","engineConfiguration":"Release"}')
    [IO.File]::WriteAllText("$folder/Content/Scenes/Main.scene.json",'{"version":1,"name":"Main","objects":[]}')
    return $folder
}
function Run([string]$folder,[bool]$editor,[int]$choice,[bool]$expectPrompt,[int]$expectedCode=0) {
    $mode=if($editor){'--smoke-project-editor-prompt'}else{'--smoke-project-prompt'}
    $log=Join-Path $root ([guid]::NewGuid().ToString('N'))
    $process=Start-Process -FilePath $launcher -ArgumentList @($mode,('"'+$folder+'/Project.json"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$log.out" -RedirectStandardError "$log.err"
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(30);$dialog=[IntPtr]::Zero
        while(!$process.HasExited -and [DateTime]::UtcNow -lt $deadline){
            $dialog=[PromptTest]::Find($process.Id)
            if($dialog -ne [IntPtr]::Zero){break}
            Start-Sleep -Milliseconds 50
        }
        if($expectPrompt){
            if($dialog -eq [IntPtr]::Zero){throw "Upgrade prompt missing; $log.err"}
            $text=[PromptTest]::Text([PromptTest]::GetDlgItem($dialog,211))
            if(!$text.Contains('2026.10.24.0') -or !$text.Contains($version)){throw 'Both versions must be visible'}
            if([PromptTest]::IsWindowEnabled([PromptTest]::GetDlgItem($dialog,7))){throw 'Missing previous runtime should disable old-version launch'}
            [void][PromptTest]::SendMessage([PromptTest]::GetDlgItem($dialog,$choice),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
        } elseif($dialog -ne [IntPtr]::Zero){throw 'Upgraded project prompted again'}
        if(!$process.WaitForExit(30000)){throw 'Launcher timed out'}
        if($process.ExitCode -ne $expectedCode){throw "Unexpected launcher exit $($process.ExitCode); $log.err"}
    } finally {if(!$process.HasExited){$process.Kill();$process.WaitForExit()};$process.Dispose()}
}
$cancel=Fixture 'Cancel'
Run $cancel $false 2 $true
if(Test-Path "$cancel-$version"){throw 'Cancel created a project copy'}
foreach($editor in @($false,$true)) {
    $folder=Fixture $(if($editor){'Editor'}else{'Game'})
    $hashes=@{};foreach($file in @('Project.json','Engine.lock.json','Game.dll','Content/Scenes/Main.scene.json')){$hashes[$file]=(Get-FileHash -LiteralPath "$folder/$file").Hash}
    Run $folder $editor 6 $true
    $copy="$folder-$version"
    if((Get-Content "$copy/Engine.lock.json" -Raw|ConvertFrom-Json).engineVersion -ne $version){throw 'Copy not pinned to new release'}
    foreach($file in $hashes.Keys){if((Get-FileHash -LiteralPath "$folder/$file").Hash -ne $hashes[$file]){throw "Original changed: $file"}}
    Run $copy $editor 0 $false
}
# Reopening the original must not overwrite the previously updated copy.
$firstCopy="$root/Game-$version"
$before=(Get-FileHash "$firstCopy/Engine.lock.json").Hash
Run "$root/Game" $false 6 $true
if(!(Test-Path "$firstCopy-2/Project.json") -or (Get-FileHash "$firstCopy/Engine.lock.json").Hash -ne $before){throw 'Existing upgraded copy was overwritten'}
$future=Fixture 'Future'
[IO.File]::WriteAllText("$future/Engine.lock.json",'{"format":1,"engineVersion":"9999.1.0","engineConfiguration":"Release"}')
Run $future $false 0 $false 1
$broken=Fixture 'Broken'
[IO.File]::WriteAllText("$broken/Content/Scenes/Future.scene.json",'{"version":999,"name":"Future","objects":[]}')
Run $broken $false 6 $true 1
if(Test-Path "$broken-$version"){throw 'Failed upgrade published a copy'}
Write-Output "Upgrade dialog: cancel, game/editor acceptance, original preservation, no repeat, failed migration passed. Fixture: $root"

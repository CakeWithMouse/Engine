param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('RuntimeExample','BasicExamples','Pong','SunGame','Battle')][string]$Project = 'RuntimeExample',
    [switch]$TestPlayControls)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot/../..")
$runtime = "$repo/Game/build/x64/$Configuration/Runtime/EngineRuntime.exe"
$logDir = "$repo/Engine/build/editor-viewport-$Configuration-$Project"
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

# Operate only on the window belonging to the child started by this test.
if (-not ('EditorViewportTestWindow' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class EditorViewportTestWindow {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr param);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, System.Text.StringBuilder title, int count);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int w, int h, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
    [StructLayout(LayoutKind.Sequential)] private struct Point { public int x, y; }
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int left, top, right, bottom; }
    [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] private static extern IntPtr ChildWindowFromPointEx(IntPtr hwnd, Point point, uint flags);
    [DllImport("user32.dll")] private static extern int MapWindowPoints(IntPtr from, IntPtr to, ref Point point, uint count);
    public static IntPtr Viewport(IntPtr window) {
        // The upper centre of this test's default layout is the scene viewport.
        Rect rect; if (!GetClientRect(window, out rect)) return IntPtr.Zero;
        Point point = new Point { x = rect.right / 2, y = 100 };
        IntPtr current = window;
        for (int depth = 0; depth < 32; ++depth) {
            IntPtr child = ChildWindowFromPointEx(current, point, 3);
            if (child == IntPtr.Zero || child == current) break;
            MapWindowPoints(current, child, ref point, 1);
            current = child;
        }
        return current == window ? IntPtr.Zero : current;
    }
    public static IntPtr Find(int processId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, param) => {
            uint pid; GetWindowThreadProcessId(hwnd, out pid);
            var title = new System.Text.StringBuilder(256); GetWindowText(hwnd, title, 256);
            if (pid == processId && title.ToString().StartsWith("Engine Editor")) {
                result = hwnd; return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
}
$process = Start-Process -FilePath $runtime -ArgumentList "--project Projects/$Project/Project.json --mode editor" -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru -RedirectStandardOutput "$logDir/stdout.log" -RedirectStandardError "$logDir/stderr.log"
try {
    $window = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline -and !$process.HasExited) {
        Start-Sleep -Milliseconds 100
        $window = [EditorViewportTestWindow]::Find($process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'Editor window did not appear' }
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        if ($process.HasExited) { throw 'Editor exited before its first rendered frame' }
        $startupLog = Get-Content "$logDir/stdout.log" -Raw
        if ($startupLog -and $startupLog.Contains('EngineRender (viewport)')) { break }
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Viewport did not render its first frame' }
        Start-Sleep -Milliseconds 100
    } while ($true)
    Start-Sleep -Milliseconds 700
    if ($TestPlayControls) {
        [void][EditorViewportTestWindow]::ShowWindow($window, 5)
        $viewport = [EditorViewportTestWindow]::Viewport($window)
        if ($viewport -eq [IntPtr]::Zero) { throw 'Native viewport child not found' }
        [void][EditorViewportTestWindow]::SetForegroundWindow($window)
        [void][EditorViewportTestWindow]::PostMessage($viewport, 0x201, [IntPtr]1, [IntPtr]655370)
        [void][EditorViewportTestWindow]::PostMessage($viewport, 0x202, [IntPtr]0, [IntPtr]655370)
        Start-Sleep -Milliseconds 200
        # P in Edit; two complete cycles, including Stop while paused.
        $expectedCount = 0
        foreach ($key in @(0x50, 0x20, 0x50, 0x50, 0x20, 0x20, 0x50, 0x20)) {
            $scan = if ($key -eq 0x20) { 0x39 } else { 0x19 }
            [void][EditorViewportTestWindow]::PostMessage($viewport, 0x100, [IntPtr]$key, [IntPtr](1 + ($scan -shl 16)))
            [void][EditorViewportTestWindow]::PostMessage($viewport, 0x101, [IntPtr]$key, [IntPtr](1 + ($scan -shl 16) + 0xC0000000L))
            ++$expectedCount
            $deadline = [DateTime]::UtcNow.AddSeconds(20)
            do {
                Start-Sleep -Milliseconds 100
                $stateCount = [regex]::Matches((Get-Content "$logDir/stdout.log" -Raw), 'ViewportState=(\w+)').Count
            } while ($stateCount -lt $expectedCount -and [DateTime]::UtcNow -lt $deadline -and !$process.HasExited)
            if ($stateCount -ne $expectedCount) { throw "Play control $expectedCount did not complete" }
        }
        $states = [regex]::Matches((Get-Content "$logDir/stdout.log" -Raw), 'ViewportState=(\w+)') | ForEach-Object { $_.Groups[1].Value }
        if (($states -join ',') -ne 'Edit,Play,Pause,Play,Edit,Play,Pause,Edit') {
            throw "Unexpected Play control sequence: $($states -join ',')"
        }
        Write-Host 'PASS: Qt Space Play/Stop and P Pause/Resume'
    }
    foreach ($size in @(@(960,640), @(1440,900), @(800,600))) {
        if (![EditorViewportTestWindow]::SetWindowPos($window, [IntPtr]::Zero, 0, 0, $size[0], $size[1], 6)) { throw 'Resize failed' }
        Start-Sleep -Milliseconds 250
    }
    if (![EditorViewportTestWindow]::PostMessage($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'Close failed' }
    if (!$process.WaitForExit(15000)) { throw 'Editor did not shut down after WM_CLOSE' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Editor exited with $($process.ExitCode)" }
    $log = Get-Content "$logDir/stdout.log" -Raw
    foreach ($event in @('ViewportReady','EditorViewportAttach ok','EditorRequestExit','ViewportStopped','EngineStopped')) {
        if (!$log.Contains($event)) { throw "Missing event: $event" }
    }
    if ($log -notmatch 'EditorUpdates=(\d+)' -or [int]$Matches[1] -lt 3) { throw 'Editor did not advance continuously' }
    Write-Host "PASS: Qt viewport, continuous updates without stdin, resize, close and cleanup. Logs: $logDir"
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}

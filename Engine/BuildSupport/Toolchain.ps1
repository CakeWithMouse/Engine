# Shared by SDK publication, project builds and diagnostics. Windows PowerShell 5.1.
# VS/MSBuild launched from PowerShell 7 may forward its PSModulePath to PS 5.1.
# Load the utility module belonging to the actual host (Get-FileHash, JSON, etc.).
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
function ConvertTo-NativeArgument([string]$Value) {
    '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}
function ConvertTo-MSBuildProperty([string]$Name, [string]$Value) {
    '/p:' + $Name + '=' + $Value.Replace('%','%25').Replace(';','%3B').Replace(',','%2C')
}
function Invoke-ToolchainProcess([string]$File, [string[]]$Arguments, [int]$Timeout = 60000) {
    foreach ($group in ([Environment]::GetEnvironmentVariables().Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1)) {
        $value = [Environment]::GetEnvironmentVariable($group.Group[0])
        foreach ($key in $group.Group) { [Environment]::SetEnvironmentVariable($key, $null, 'Process') }
        [Environment]::SetEnvironmentVariable($group.Name, $value, 'Process')
    }
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $File
    $start.Arguments = ($Arguments | ForEach-Object { ConvertTo-NativeArgument $_ }) -join ' '
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.Encoding]::UTF8
    $start.StandardErrorEncoding = [Text.Encoding]::UTF8
    $start.EnvironmentVariables.Clear()
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()] = $entry.Value }
    # A developer prompt for another VS instance must not choose our tools/headers.
    foreach ($key in @('VSINSTALLDIR','VCINSTALLDIR','VCTOOLSINSTALLDIR','VCTOOLSVERSION','VCTARGETSPATH',
        'WINDOWSSDKDIR','WINDOWSSDKVERSION','INCLUDE','LIB','LIBPATH','VISUALSTUDIOVERSION','PSMODULEPATH')) { $start.EnvironmentVariables.Remove($key) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit($Timeout)) { $process.Kill(); $process.WaitForExit(); throw "Tool timed out: $File" }
        [pscustomobject]@{ exitCode=$process.ExitCode; stdout=$stdout.GetAwaiter().GetResult(); stderr=$stderr.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}
function Get-EngineToolchains {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2019 or newer / Build Tools, with Desktop development with C++ and a Windows 10/11 SDK (vswhere.exe missing).' }
    $result = Invoke-ToolchainProcess $vswhere @('-products','*','-format','json','-utf8','-requires','Microsoft.Component.MSBuild')
    if ($result.exitCode) { throw "vswhere failed: $($result.stderr)" }
    # Do not wrap ConvertFrom-Json in @(...): PS 5.1 can keep a JSON array nested.
    $instances = $result.stdout | ConvertFrom-Json
    foreach ($instance in $instances) {
        if ($instance.isComplete -eq $false -or $instance.isPrerelease -eq $true) { continue }
        $root = $instance.installationPath
        $msbuild = Join-Path $root 'MSBuild/Current/Bin/MSBuild.exe'
        if (!(Test-Path -LiteralPath $msbuild)) { continue }
        foreach ($directory in (Get-ChildItem -LiteralPath (Join-Path $root 'VC/Tools/MSVC') -Directory -ErrorAction SilentlyContinue)) {
            $version = $null
            if (![version]::TryParse($directory.Name, [ref]$version) -or $version -lt [version]'14.29') { continue }
            $family = if ($version.Minor -ge 50) { 'v145' } elseif ($version.Minor -ge 30) { 'v143' } else { 'v142' }
            $targets = Get-ChildItem -Path (Join-Path $root "MSBuild/Microsoft/VC/*/Platforms/x64/PlatformToolsets/$family/Toolset.props") -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending | Select-Object -First 1
            if (!$targets) { continue }
            $targetRoot = $targets.Directory.Parent.Parent.Parent.Parent.FullName
            $bin = Join-Path $directory.FullName 'bin/Hostx64/x64'
            $complete = $true
            foreach ($file in @('bin/Hostx64/x64/cl.exe','bin/Hostx64/x64/link.exe','bin/Hostx64/x64/lib.exe','include/vector','lib/x64/msvcprt.lib')) {
                if (!(Test-Path -LiteralPath (Join-Path $directory.FullName $file))) { $complete = $false }
            }
            if (!$complete) { continue }
            [pscustomobject]@{
                version=$directory.Name; toolset=$family; installation=$root; msbuild=$msbuild
                tools=$directory.FullName; targets=$targetRoot; bin=$bin
                compilerVersion=([Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $bin 'cl.exe')).FileVersion -replace '\s.*$','')
            }
        }
    }
}
function Get-EngineWindowsSdk {
    $roots = @((Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'))
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots')) {
        $kits = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        if ($kits -and $kits.KitsRoot10) { $roots += $kits.KitsRoot10 }
    }
    $available = foreach ($root in ($roots | Select-Object -Unique)) {
        foreach ($dir in (Get-ChildItem -LiteralPath (Join-Path $root 'Include') -Directory -ErrorAction SilentlyContinue)) {
            $version = $null
            if (![version]::TryParse($dir.Name,[ref]$version) -or $version -lt [version]'10.0.17763.0') { continue }
            $complete = $true
            foreach ($file in @("Include/$version/um/Windows.h","Include/$version/ucrt/stdio.h","Lib/$version/um/x64/kernel32.lib","Lib/$version/ucrt/x64/ucrt.lib","bin/$version/x64/rc.exe")) {
                if (!(Test-Path -LiteralPath (Join-Path $root $file))) { $complete = $false }
            }
            if ($complete) { [pscustomobject]@{root=$root;version=$dir.Name} }
        }
    }
    $selected = $available | Sort-Object { [version]$_.version } -Descending | Select-Object -First 1
    if (!$selected) { throw 'Install Windows 10/11 SDK (10.0.17763.0 or newer), including x64 libraries and build tools.' }
    $selected
}
function Select-EngineToolchain($Candidates, [string]$MinimumVersion = '14.29.30133', [string]$Toolset = '', [string]$ExactVersion = '', [string]$MinimumCompilerVersion = '') {
    $selected = $Candidates | Where-Object {
        [version]$_.version -ge [version]$MinimumVersion -and (!$Toolset -or $_.toolset -eq $Toolset) -and (!$ExactVersion -or $_.version -eq $ExactVersion) -and
        (!$MinimumCompilerVersion -or [version]$_.compilerVersion -ge [version]$MinimumCompilerVersion)
    } | Sort-Object @{Expression={ [version]$_.version };Descending=$true}, @{Expression='installation';Descending=$false} | Select-Object -First 1
    if (!$selected) {
        $found = ($Candidates | ForEach-Object { "$($_.toolset) $($_.version)" }) -join ', '
        throw "No compatible x64 C++ tools. Required MSVC >= $MinimumVersion$(if($Toolset){", $Toolset"})$(if($ExactVersion){", exact $ExactVersion"})$(if($MinimumCompilerVersion){", compiler >= $MinimumCompilerVersion"}). Found: $found. Install the matching MSVC component in Visual Studio Installer (VS 2019 or newer)."
    }
    $selected
}
function Resolve-EngineToolchain($SdkInfo = $null, [string]$Toolset = '', [string]$ExactVersion = '') {
    $minimum = '14.29.30133'
    if ($SdkInfo) {
        if ($SdkInfo.buildTools -and $SdkInfo.buildTools.minimumVersion) { $minimum = $SdkInfo.buildTools.minimumVersion }
        else {
            # Legacy SDKs did not record their producer. Never label them VS 2019-compatible.
            $minimum = switch ($SdkInfo.toolset) { 'v142' {'14.29.30133'} 'v143' {'14.44.35207'} 'v145' {'14.50.35717'} default { throw 'Unknown legacy SDK toolset; update the engine SDK.' } }
        }
        if ($SdkInfo.buildTools.wholeProgramOptimization) { $ExactVersion = $SdkInfo.buildTools.version }
    }
    $selected = Select-EngineToolchain @(Get-EngineToolchains) $minimum $Toolset $ExactVersion $SdkInfo.buildTools.compilerVersion
    $sdk = Get-EngineWindowsSdk
    $selected | Add-Member -NotePropertyName windowsSdk -NotePropertyValue $sdk -Force
    $selected
}
function Get-EngineToolchainProperties($Toolchain) {
    $vc = $Toolchain.tools.TrimEnd('\','/') + '\'
    $win = $Toolchain.windowsSdk.root.TrimEnd('\','/') + '\'
    $ver = $Toolchain.windowsSdk.version
    $include = @(($vc+'include'), ($win+"Include\$ver\ucrt"), ($win+"Include\$ver\shared"), ($win+"Include\$ver\um"), ($win+"Include\$ver\winrt"), ($win+"Include\$ver\cppwinrt"))
    $library = @(($vc+'lib\x64'), ($win+"Lib\$ver\ucrt\x64"), ($win+"Lib\$ver\um\x64"))
    if (Test-Path -LiteralPath ($vc+'atlmfc\lib\x64')) { $library += $vc+'atlmfc\lib\x64'; $include += $vc+'atlmfc\include' }
    [ordered]@{
        Platform='x64'; PlatformToolset=$Toolchain.toolset; VCToolsVersion=$Toolchain.version
        VCTargetsPath=($Toolchain.targets+'\'); VCInstallDir=($Toolchain.installation+'\VC\'); VCToolsInstallDir=$vc
        WindowsTargetPlatformVersion=$ver; WindowsSdkDir=$win; UniversalCRTSdkDir=$win; UCRTVersion=$ver
        PreferredToolArchitecture='x64'; CLToolPath=($Toolchain.bin+'\'); LinkToolPath=($Toolchain.bin+'\'); LibToolPath=($Toolchain.bin+'\')
        IncludePath=($include -join ';'); LibraryPath=($library -join ';'); EngineToolchainSelected='true'
    }
}
function Get-EngineToolchainArguments($Toolchain) {
    foreach ($entry in (Get-EngineToolchainProperties $Toolchain).GetEnumerator()) { ConvertTo-MSBuildProperty $entry.Key $entry.Value }
}

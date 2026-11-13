#requires -Version 5.1
<#
Diagnose the toolchain used by PainEngine ProjectTools.ps1 without building or
changing the project. Run with Windows PowerShell 5.1 or PowerShell 7.
Example: .\Test-ProjectToolchain.ps1 -Project 'E:\Projects\MyGame'
The report contains local installation/project paths, but no project sources.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Runtime = '',
    [string]$BuildLog = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$utf8 = New-Object Text.UTF8Encoding($false)
function ResolveDiagnosticPath([string]$value) {
    if ([string]::IsNullOrWhiteSpace($value)) { throw 'The path is empty.' }
    $value = $value.Trim()
    # Also accept a path pasted with literal surrounding quotes.
    if ($value.Length -ge 2 -and (
        ($value[0] -eq [char]34 -and $value[$value.Length-1] -eq [char]34) -or
        ($value[0] -eq [char]39 -and $value[$value.Length-1] -eq [char]39) -or
        ($value[0] -eq [char]0x201C -and $value[$value.Length-1] -eq [char]0x201D))) {
        $value = $value.Substring(1, $value.Length-2)
    }
    # Resolve relative paths against PowerShell's location, not the process CWD.
    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($value)
    return [IO.Path]::GetFullPath($resolved)
}
$rawOutputDirectory = $OutputDirectory
$outputWarning = ''
$runName = 'PainEngine-Diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
try {
    # Resolve the default in the script body, not a parameter default expression.
    if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
        $OutputDirectory = [IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Path)
    }
    $reportDir = Join-Path (ResolveDiagnosticPath $OutputDirectory) $runName
    [IO.Directory]::CreateDirectory($reportDir) | Out-Null
} catch {
    $outputWarning = 'Could not use report directory ' + ($OutputDirectory | ConvertTo-Json -Compress) + ': ' + $_.Exception.Message
    Write-Warning $outputWarning
    try {
        $reportDir = Join-Path ([IO.Path]::GetTempPath()) $runName
        [IO.Directory]::CreateDirectory($reportDir) | Out-Null
    } catch {
        throw 'Cannot create a report folder. Run again with -OutputDirectory pointing to an existing writable folder.'
    }
}
$findings = New-Object 'System.Collections.Generic.List[string]'
$report = [ordered]@{
    date = (Get-Date -Format o)
    powershell = $PSVersionTable.PSVersion.ToString()
    process64bit = [Environment]::Is64BitProcess
    project = $Project
    runtimeOverride = $Runtime
    requestedOutputDirectory = $rawOutputDirectory
    reportDirectory = $reportDir
}
if ($outputWarning) { $findings.Add('[WARN] ' + $outputWarning + ' Using the temporary directory instead.') }
# .NET Framework's ProcessStartInfo cannot initialize its environment dictionary
# when the parent contains duplicate names differing only in case.
$duplicates = @([Environment]::GetEnvironmentVariables().Keys | Group-Object { $_.ToUpperInvariant() } | Where-Object Count -gt 1)
$report.duplicateEnvironmentNames = @($duplicates | ForEach-Object Name)
foreach ($group in $duplicates) {
    $value = [Environment]::GetEnvironmentVariable($group.Group[0])
    foreach ($name in $group.Group) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    [Environment]::SetEnvironmentVariable($group.Name, $value, 'Process')
}

function Finding([string]$level, [string]$message) { $findings.Add("[$level] $message") }
function SaveText([string]$name, [string]$value) { [IO.File]::WriteAllText((Join-Path $reportDir $name), $value, $utf8) }
function ReadJson([string]$path) { Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json }
function QuoteArg([string]$value) {
    # Windows CommandLineToArgvW quoting; no shell is involved.
    '"' + [regex]::Replace([regex]::Replace($value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}
function PropertyArg([string]$name, [string]$value) {
    '/p:' + $name + '=' + $value.Replace('%','%25').Replace(';','%3B').Replace(',','%2C')
}
function RunTool([string]$exe, [string[]]$arguments, [int]$timeout = 60000) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $exe
    $start.Arguments = ($arguments | ForEach-Object { QuoteArg $_ }) -join ' '
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = $utf8
    $start.StandardErrorEncoding = $utf8
    # Normalize PATH/Path inherited from some desktop hosts. Keep the remaining
    # environment intact so diagnostics reflect the editor's actual environment.
    $start.EnvironmentVariables.Clear()
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        $start.EnvironmentVariables[$entry.Key.ToUpperInvariant()] = $entry.Value
    }
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit($timeout)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Diagnostic tool timed out: $exe"
    }
    $result = [pscustomobject]@{ exitCode=$process.ExitCode; stdout=$stdout.GetAwaiter().GetResult(); stderr=$stderr.GetAwaiter().GetResult() }
    $process.Dispose()
    return $result
}

$symbols = @()
try {
    $projectPath = ResolveDiagnosticPath $Project
    if (Test-Path -LiteralPath $projectPath -PathType Container) { $projectPath = Join-Path $projectPath 'Project.json' }
    $projectRoot = Split-Path $projectPath
    $report.project = $projectPath
    if (!(Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Project file not found: $projectPath" }

    if ($BuildLog) {
        $BuildLog = ResolveDiagnosticPath $BuildLog
        $logText = Get-Content -LiteralPath $BuildLog -Raw
        $found = @([regex]::Matches($logText, '__std_[A-Za-z0-9_]+') | ForEach-Object Value | Sort-Object -Unique)
        if ($found.Count) { $symbols = $found }
        $report.buildLog = [IO.Path]::GetFullPath($BuildLog)
    }
    $report.checkedSymbols = $symbols
    $pin = ReadJson (Join-Path $projectRoot 'Engine.lock.json')
    $report.engineLock = $pin
    if (!$Runtime) {
        $store = if ($env:PAINENGINE_ENGINE_STORE) { $env:PAINENGINE_ENGINE_STORE } else { Join-Path $env:LOCALAPPDATA 'PainEngine/Engines' }
        $Runtime = Join-Path $store ($pin.engineVersion + '/' + $pin.engineConfiguration)
    }
    $Runtime = ResolveDiagnosticPath $Runtime
    $sdk = Join-Path $Runtime 'SDK'
    $report.runtime = $Runtime
    $sdkInfo = ReadJson (Join-Path $sdk 'Sdk.json')
    $report.sdk = $sdkInfo
    if ($pin.engineVersion -ne $sdkInfo.version -or $pin.engineConfiguration -ne $sdkInfo.configuration -or $sdkInfo.platform -ne 'x64') {
        Finding ERROR 'Engine.lock.json and SDK/Sdk.json disagree. ProjectTools will reject this SDK.'
    }
    $engineLib = Join-Path $sdk 'Lib/Engine.lib'
    if (Test-Path -LiteralPath $engineLib) {
        $report.engineLibrary = [ordered]@{ path=$engineLib; sha256=(Get-FileHash -LiteralPath $engineLib -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $engineLib).Length }
    } else { Finding ERROR "Engine.lib is missing: $engineLib" }

    # Allowlist only build-related paths/overrides. Do not dump the entire environment.
    $environment = [ordered]@{}
    foreach ($name in @('PATH','INCLUDE','LIB','LIBPATH','VSINSTALLDIR','VCINSTALLDIR','VCToolsInstallDir','VCToolsVersion','VCTargetsPath','WindowsSdkDir','WindowsSDKVersion','PAINENGINE_ENGINE_STORE')) {
        $environment[$name] = [Environment]::GetEnvironmentVariable($name)
    }
    $report.buildEnvironment = $environment
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe is missing. Install Visual Studio or Build Tools with Desktop development with C++.' }
    $all = RunTool $vswhere @('-all','-products','*','-format','json','-utf8')
    if ($all.exitCode) { throw "vswhere failed: $($all.stderr)" }
    $installations = $all.stdout | ConvertFrom-Json
    $inventory = @()
    foreach ($installation in $installations) {
        $msvcRoot = Join-Path $installation.installationPath 'VC/Tools/MSVC'
        $versions = @()
        if (Test-Path -LiteralPath $msvcRoot) {
            $versions = @(Get-ChildItem -LiteralPath $msvcRoot -Directory | ForEach-Object { [pscustomobject]@{
                version=$_.Name; path=$_.FullName
                compilerPresent=(Test-Path -LiteralPath (Join-Path $_.FullName 'bin/Hostx64/x64/cl.exe'))
                cppLibraryPresent=(Test-Path -LiteralPath (Join-Path $_.FullName 'lib/x64/msvcprt.lib'))
            } })
        }
        $inventory += [pscustomobject]@{ name=$installation.displayName; version=$installation.installationVersion; path=$installation.installationPath; toolchains=$versions }
    }
    $report.visualStudio = $inventory
    $toolchainArgs = @()
    if (Test-Path -LiteralPath "$sdk/Toolchain.ps1") {
        . "$sdk/Toolchain.ps1"
        $selected = Resolve-EngineToolchain $sdkInfo
        $msbuild = $selected.msbuild
        $toolchainArgs = @(Get-EngineToolchainArguments $selected)
        $report.selectedToolchain = $selected
    } else {
        # Preserve the old SDK's selection when diagnosing a legacy installation.
        $selection = RunTool $vswhere @('-latest','-products','*','-requires','Microsoft.Component.MSBuild','-find','MSBuild\**\Bin\MSBuild.exe')
        $msbuild = @($selection.stdout -split '\r?\n' | Where-Object { $_.Trim() }) | Select-Object -First 1
        if (!$msbuild) { throw 'MSBuild is missing.' }
        $msbuild = $msbuild.Trim()
        $toolchainArgs = @((PropertyArg 'PlatformToolset' $sdkInfo.toolset))
    }
    $report.selectedMSBuild = $msbuild
    $projectFile = Join-Path $projectRoot 'Game.vcxproj'
    if (!(Test-Path -LiteralPath $projectFile)) { throw 'Game.vcxproj is missing. Enable C++ in the project first.' }
    $properties = 'PlatformToolset,VCToolsVersion,VCToolsInstallDir,VCTargetsPath,VCInstallDir,WindowsTargetPlatformVersion,WindowsSdkDir,WindowsSDK_IncludePath,WindowsSDK_LibraryPath_x64,LibraryPath,IncludePath,ExecutablePath,CLToolPath,CLToolExe,LinkToolPath,LinkToolExe,IntDir,EngineSdk'
    # /getProperty is unavailable in MSBuild 16 (VS 2019). This isolated reporting
    # target has no dependencies: it evaluates properties without compiling code.
    $probe = '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><Target Name="PainEngineReportToolchain">'
    foreach ($name in ($properties -split ',')) { $probe += '<Message Importance="High" Text="[PainEngineProperty]'+$name+'=$('+$name+')" />' }
    $probe += '</Target></Project>'
    $probeFile = Join-Path $reportDir 'ReportToolchain.targets'
    [IO.File]::WriteAllText($probeFile, $probe, $utf8)
    $arguments = @($projectFile,'/nologo','/nr:false','/v:minimal','/t:PainEngineReportToolchain',
        (PropertyArg 'Platform' 'x64'), (PropertyArg 'Configuration' $pin.engineConfiguration),
        (PropertyArg 'EngineSdk' $sdk), (PropertyArg 'ForceImportAfterCppTargets' $probeFile), '/p:EngineToolchainSelected=true') + $toolchainArgs
    $evaluation = if (Test-Path -LiteralPath "$sdk/Toolchain.ps1") { Invoke-ToolchainProcess $msbuild $arguments } else { RunTool $msbuild $arguments }
    SaveText 'msbuild-properties.txt' ($evaluation.stdout + "`r`n" + $evaluation.stderr)
    if ($evaluation.exitCode) { throw 'MSBuild property evaluation failed. See msbuild-properties.txt.' }
    $values = [ordered]@{}
    foreach ($line in ($evaluation.stdout -split '\r?\n')) {
        if ($line -match '\[PainEngineProperty\]([^=]+)=(.*)$') { $values[$matches[1]] = $matches[2].Trim() }
    }
    $evaluated = [pscustomobject]$values
    if (!$evaluated.VCToolsInstallDir) { throw 'MSBuild did not resolve VCToolsInstallDir.' }
    $report.evaluated = $evaluated
    Finding INFO "ProjectTools selects $msbuild"
    $sdkVersionMatch = [regex]::Match($evaluated.WindowsSDK_IncludePath, 'Include\\(10\.\d+\.\d+\.\d+)\\')
    $resolvedWindowsSdk = if ($sdkVersionMatch.Success) { $sdkVersionMatch.Groups[1].Value } else { $evaluated.WindowsTargetPlatformVersion }
    Finding INFO "Resolved toolset $($evaluated.PlatformToolset), MSVC $($evaluated.VCToolsVersion), Windows SDK $resolvedWindowsSdk."
    if (!(Test-Path -LiteralPath "$sdk/Toolchain.ps1") -and ($evaluated.CLToolPath -or $evaluated.LinkToolPath -or $evaluated.CLToolExe -or $evaluated.LinkToolExe)) {
        Finding WARN 'Custom compiler/linker overrides are present. Inspect CLToolPath/Exe and LinkToolPath/Exe in report.json; they may bypass the resolved MSVC tools.'
    }

    $dumpbin = Join-Path $evaluated.VCToolsInstallDir 'bin/Hostx64/x64/dumpbin.exe'
    if (!(Test-Path -LiteralPath $dumpbin)) { $dumpbin = Join-Path $evaluated.VCToolsInstallDir 'bin/Hostx86/x64/dumpbin.exe' }
    if (!(Test-Path -LiteralPath $dumpbin)) { throw 'dumpbin.exe is missing in the selected MSVC installation.' }
    $compiler = Join-Path $evaluated.VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    if (Test-Path -LiteralPath $compiler) { $report.compilerFileVersion = (Get-Item -LiteralPath $compiler).VersionInfo.FileVersion }
    if (!$symbols.Count -and (Test-Path -LiteralPath $engineLib)) {
        $symbolsDump = RunTool $dumpbin @('/nologo','/symbols',$engineLib)
        if ($symbolsDump.exitCode) { throw 'Cannot inspect the SDK symbol requirements.' }
        $symbols = @([regex]::Matches($symbolsDump.stdout, '(?m)^.*\bUNDEF\b.*\|\s+(__std_[A-Za-z0-9_]+)\s*$') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    }
    $report.checkedSymbols = $symbols
    $libraryName = switch ($sdkInfo.crt) { 'MDd' {'msvcprtd.lib'} 'MT' {'libcpmt.lib'} 'MTd' {'libcpmtd.lib'} default {'msvcprt.lib'} }
    $searchDirs = @($evaluated.LibraryPath -split ';' | Where-Object { $_ })
    $windowsHeaders = @($evaluated.IncludePath -split ';' | Where-Object { $_ } | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'Windows.h') })
    $windowsLibraries = @($searchDirs | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'kernel32.lib') })
    if (!$windowsHeaders.Count -or !$windowsLibraries.Count) { Finding ERROR 'Windows SDK headers or x64 libraries are missing from the evaluated search paths.' }
    $report.evaluatedLibrarySearch = $searchDirs
    $candidates = @()
    foreach ($directory in $searchDirs) {
        $candidate = Join-Path $directory $libraryName
        if (Test-Path -LiteralPath $candidate) { $candidates += [IO.Path]::GetFullPath($candidate) }
    }
    $selectedLibrary = $candidates | Select-Object -First 1
    $report.libraryCandidates = $candidates
    $report.selectedLibraryByEvaluatedPath = $selectedLibrary
    if (!$selectedLibrary) { Finding ERROR "$libraryName is not present in the evaluated LibraryPath." }

    $libraryChecks = @()
    $libraries = @($selectedLibrary)
    foreach ($instance in $inventory) {
        foreach ($toolchain in $instance.toolchains) { $libraries += Join-Path $toolchain.path ('lib/x64/' + $libraryName) }
    }
    foreach ($library in @($libraries | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)) {
        $dump = RunTool $dumpbin @('/nologo','/linkermember:1',$library)
        if ($dump.exitCode) {
            $libraryChecks += [pscustomobject]@{ path=$library; status='inspection failed'; error=$dump.stderr }
            Finding WARN "Could not inspect $library"
            continue
        }
        # Exception/type-info helpers live in VCRuntime, not msvcprt.lib. Inspect
        # the matching CRT set instead of reporting those valid references missing.
        $crtParts = @($library)
        $debugSuffix = if ($sdkInfo.crt -eq 'MDd') { 'd' } else { '' }
        foreach ($name in @("vcruntime$debugSuffix.lib", "msvcrt$debugSuffix.lib")) {
            $part = Join-Path (Split-Path $library) $name
            if (Test-Path -LiteralPath $part) {
                $partDump = RunTool $dumpbin @('/nologo','/linkermember:1',$part)
                if (!$partDump.exitCode) { $dump.stdout += "`n" + $partDump.stdout; $crtParts += $part }
            }
        }
        $missing = @($symbols | Where-Object { $dump.stdout -notmatch ('(?m)\s' + [regex]::Escape($_) + '\r?$') })
        $libraryChecks += [pscustomobject]@{ path=$library; status='inspected'; runtimeLibraries=$crtParts; missingSymbols=$missing }
        if ($library -eq $selectedLibrary) {
            if ($missing.Count) {
                Finding ERROR "Selected MSVC runtime libraries ($library) lack: $($missing -join ', '). Update this MSVC toolset or remove an older library-path override."
            } else {
                Finding OK "Selected MSVC runtime libraries ($library) contain every checked SDK symbol."
                Finding INFO 'If the editor still fails, compare the last link command and build environment; another library path, stale build, or different VS instance may be involved.'
            }
        }
    }
    $report.libraryChecks = $libraryChecks
    # Preserve the last actual linker commands as evidence of overrides. They
    # describe previous builds and can be stale; do not present them as a new run.
    $intermediate = Join-Path $projectRoot 'Intermediate'
    if (Test-Path -LiteralPath $intermediate) {
        $linkLogs = @(Get-ChildItem -LiteralPath $intermediate -Filter 'link.command*.tlog' -Recurse -File |
            Sort-Object LastWriteTime -Descending | Select-Object -First 3)
        $report.previousLinkLogs = @($linkLogs | ForEach-Object { [pscustomobject]@{ path=$_.FullName; modified=$_.LastWriteTime.ToString('o') } })
        $index = 0
        foreach ($linkLog in $linkLogs) {
            $index++
            SaveText "previous-link-$index.txt" ([IO.File]::ReadAllText($linkLog.FullName))
        }
        if ($linkLogs.Count) { Finding INFO 'previous-link-*.txt contains earlier linker commands, including /LIBPATH and runtime-library overrides. These logs can be stale.' }
    }
    $windowsKits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/Include'
    if (Test-Path -LiteralPath $windowsKits) { $report.installedWindowsSdkVersions = @(Get-ChildItem -LiteralPath $windowsKits -Directory | Select-Object -ExpandProperty Name) }
} catch {
    Finding ERROR $_.Exception.Message
    $report.diagnosticError = $_.Exception.ToString()
    $report.diagnosticErrorPosition = $_.InvocationInfo.PositionMessage
} finally {
    $report.findings = @($findings.ToArray())
    SaveText 'report.json' ($report | ConvertTo-Json -Depth 12)
    $summary = @('PainEngine C++ toolchain diagnostics', '', ($findings -join "`r`n"), '',
        'No project build or installation was performed. Project sources and SDK were not modified.',
        'Reports include local paths and previous linker commands; inspect before sharing.',
        'An OK symbol check does not prove the project builds; it only checks the evaluated library.',
        'Send summary.txt, report.json and msbuild-properties.txt (or the ZIP) to the maintainer.') -join "`r`n"
    SaveText 'summary.txt' $summary
    Write-Output $summary
    Write-Output "Report folder: $reportDir"
    try {
        $zip = $reportDir + '.zip'
        Compress-Archive -Path (Join-Path $reportDir '*') -DestinationPath $zip
        Write-Output "Send this ZIP: $zip"
    } catch { Write-Warning "ZIP creation failed; send the report folder instead: $($_.Exception.Message)" }
}

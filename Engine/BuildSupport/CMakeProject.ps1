# CMake backend for the existing transactional project build/activation workflow.
function Get-ProjectCMakeTools($SdkInfo, [string]$Toolset, [string]$ExactVersion) {
    $native = Resolve-EngineToolchain -Toolset $Toolset -ExactVersion $ExactVersion
    $properties = Get-EngineToolchainProperties $native
    $cmakeRoot = Join-Path $native.installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    if (!$cmake) { $cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe' }
    $ninja = Get-Command ninja -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    if (!$ninja) { $ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe' }
    foreach ($tool in @($cmake,$ninja)) { if (!(Test-Path -LiteralPath $tool)) { throw 'CMake and Ninja are required. Install the CMake tools component or add them to PATH.' } }
    $compiler = $SdkInfo.cmake.compiler
    $flags = $SdkInfo.cmake.compilerFlags
    if (!(Test-Path -LiteralPath $compiler)) {
        $flags = ''
        if ($SdkInfo.cmake.compilerId -eq 'MSVC') { $compiler = Join-Path $native.bin 'cl.exe' }
        else {
            $compiler = Get-Command clang-cl -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
            if (!$compiler) { throw 'This SDK was built with Clang. Add a compatible clang-cl to PATH.' }
        }
    }
    $sdkBin = Join-Path $native.windowsSdk.root ("bin/"+$native.windowsSdk.version+'/x64')
    [pscustomobject]@{
        cmake=$cmake; ninja=$ninja; compiler=$compiler; flags=$flags; native=$native
        # Include the encoding in the build identity: old Ninja dependency caches
        # may already contain paths damaged by the system code page.
        consoleCodePage=65001
        environment=@{ INCLUDE=$properties.IncludePath; LIB=$properties.LibraryPath
            PATH=($native.bin+';'+$sdkBin+';'+$env:PATH); VSLANG='1033' }
    }
}
function Invoke-ProjectCMakeBuild($Tools, [string]$ProjectRoot, [string]$Sdk, [string]$Configuration,
    [string]$Output, [string]$Identity, [string]$Log, [bool]$Clean) {
    $binary = Join-Path $ProjectRoot ("Intermediate/CMake/$Configuration/$Identity")
    $configure = @('-S',$ProjectRoot,'-B',$binary,'-G','Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_PREFIX_PATH=$Sdk",
        "-DCMAKE_CXX_COMPILER=$($Tools.compiler)", "-DCMAKE_CXX_FLAGS=$($Tools.flags)",
        "-DCMAKE_MAKE_PROGRAM=$($Tools.ninja)", "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=$Output",
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$Output")
    $launcher = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $PSScriptRoot 'Invoke-Utf8Tool.ps1'), $Tools.cmake)
    $configured = Invoke-ToolchainProcess powershell.exe ($launcher + $configure) -Timeout 120000 -Environment $Tools.environment
    [IO.File]::WriteAllText($Log, $configured.stdout+$configured.stderr, [Text.UTF8Encoding]::new($false))
    if ($configured.exitCode) { return $configured }
    $arguments = @('--build',$binary,'--parallel','8')
    if ($Clean) { $arguments += '--clean-first' }
    $built = Invoke-ToolchainProcess powershell.exe ($launcher + $arguments) -Timeout 1800000 -Environment $Tools.environment
    [IO.File]::AppendAllText($Log, $built.stdout+$built.stderr, [Text.UTF8Encoding]::new($false))
    return $built
}

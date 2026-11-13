$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../Toolchain.ps1"
function Candidate($version, $family, $instance, $compiler) {
    [pscustomobject]@{version=$version;toolset=$family;installation=$instance;compilerVersion=$compiler}
}
function Assert($condition, $message) { if (!$condition) { throw $message } }
$tools = @(
    (Candidate '14.38.33130' 'v143' 'VS2022' '19.38.33145.0'),
    (Candidate '14.44.35207' 'v143' 'VS2022' '19.44.35225.0'),
    (Candidate '14.51.36231' 'v145' 'BuildTools2026' '19.51.36231.0'),
    (Candidate '14.29.30133' 'v142' 'VS2019' '19.29.30159.0'))
Assert ((Select-EngineToolchain $tools).version -eq '14.51.36231') 'Newest compatible tools across instances must win.'
Assert ((Select-EngineToolchain $tools -Toolset v143).version -eq '14.44.35207') 'A v143 request must not select the old 14.38 default.'
Assert ((Select-EngineToolchain $tools -Toolset v142).version -eq '14.29.30133') 'SDK publication must use the VS 2019 baseline.'
Assert ((Select-EngineToolchain @($tools[3])).version -eq '14.29.30133') 'A machine with only VS 2019 must work.'
Assert ((Select-EngineToolchain @($tools[2])).toolset -eq 'v145') 'A machine with only new Build Tools must work.'
Assert ((Select-EngineToolchain $tools -ExactVersion '14.44.35207').version -eq '14.44.35207') 'Explicit matrix selection must be respected.'
foreach ($case in @(
    @{Candidates=@($tools[0]);MinimumVersion='14.44.35207'},
    @{Candidates=@($tools[3]);MinimumVersion='14.30'},
    @{Candidates=@($tools[3]);MinimumCompilerVersion='19.29.30160.0'},
    @{Candidates=@();MinimumVersion='14.29.30133'})) {
    $rejected = $false
    try { Select-EngineToolchain @case | Out-Null } catch { $rejected = $_.Exception.Message -like 'No compatible x64 C++ tools.*' }
    Assert $rejected 'Incompatible or missing tools must fail before project compilation.'
}
$encoded = ConvertTo-NativeArgument 'C:\Path with spaces\'
Assert ($encoded -eq '"C:\Path with spaces\\"') 'Trailing backslash must survive native argument quoting.'
Assert ((ConvertTo-MSBuildProperty 'LibraryPath' 'C:\A;C:\B') -eq '/p:LibraryPath=C:\A%3BC:\B') 'Library search paths must be passed as one MSBuild property.'
Write-Output 'Toolchain selection regression checks passed.'

param([Parameter(Mandatory=$true)][string]$Runtime)
$ErrorActionPreference = 'Stop'
$Runtime = [IO.Path]::GetFullPath($Runtime)
$work = Join-Path $Runtime ('../../../sdk-checks/' + [guid]::NewGuid().ToString('N'))
$work = [IO.Path]::GetFullPath($work)
$project = Join-Path $work 'CMake проект с пробелами'
$manifest = Join-Path $project 'Project.json'
$launcher = Join-Path (Split-Path $Runtime) 'EngineLauncher.exe'
$script = Join-Path $Runtime 'SDK/ProjectTools.ps1'
if (!(Test-Path -LiteralPath (Join-Path $Runtime 'Editor.dll') -PathType Leaf)) {
    throw "CMake project tests require a complete engine release: Editor.dll is missing in $Runtime. Configure with -DPAIN_BUILD_EDITOR=ON and build the editor before running this test."
}
New-Item -ItemType Directory -Force $work | Out-Null
$previousStore = $env:PAINENGINE_ENGINE_STORE
$env:PAINENGINE_ENGINE_STORE = Join-Path $work 'EngineStore'
function Check([bool]$value, [string]$message) { if (!$value) { throw $message } }
try {
    $createOutput = Join-Path $work 'create-project.stdout.log'
    $createError = Join-Path $work 'create-project.stderr.log'
    $create = Start-Process -FilePath $launcher -ArgumentList @('--create-project', ('"'+$project+'"'), 'CMakeTest') -WindowStyle Hidden -PassThru -RedirectStandardOutput $createOutput -RedirectStandardError $createError
    if (!$create.WaitForExit(60000)) { $create.Kill(); throw "Project creation timed out. Logs: $createOutput, $createError" }
    $create.WaitForExit()
    if ($create.ExitCode -ne 0) {
        $details = Get-Content -LiteralPath $createOutput, $createError -Raw -Encoding UTF8
        throw "Project creation failed (exit code $($create.ExitCode)).`n$($details -join [Environment]::NewLine)`nLogs: $createOutput, $createError"
    }
    # Call the same Windows PowerShell host used by the editor, not the caller's developer shell.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -Action Enable -Project $manifest -Runtime $Runtime
    Check ($LASTEXITCODE -eq 0) 'CMake Enable failed'
    Check (Test-Path -LiteralPath "$project/CMakeLists.txt") 'CMake project missing'
    Check (!(Test-Path -LiteralPath "$project/Game.vcxproj")) 'CMake SDK generated an MSBuild project'
    $state = Get-Content -LiteralPath "$project/Project.code.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    Check ($state.state -eq 'ready') 'Module was not validated and activated'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -Action Build -Project $manifest -Runtime $Runtime
    Check ($LASTEXITCODE -eq 0) 'No-op build failed'
    Check ((Get-Content -LiteralPath "$project/Project.code.json" -Raw -Encoding UTF8 | ConvertFrom-Json).buildId -eq $state.buildId) 'No-op build changed module'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -Action AddClass -Kind Object -Name PortableObject -Project $manifest -Runtime $Runtime
    Check ($LASTEXITCODE -eq 0) 'AddClass failed'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -Action Build -Project $manifest -Runtime $Runtime
    Check ($LASTEXITCODE -eq 0) 'Build after AddClass failed'
    Check ((Get-Content -LiteralPath "$project/ProjectTypes.tsv" -Raw).Contains('PortableObject')) 'Updated catalog missing type'
    $before = (Get-FileHash -LiteralPath $manifest).Hash
    $activeModule = Join-Path $project (Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json).module
    $moduleHash = (Get-FileHash -LiteralPath $activeModule).Hash
    $source = Join-Path $project 'Source/ProjectComponent.cpp'
    $original = [IO.File]::ReadAllText($source)
    $timestamp = [IO.File]::GetLastWriteTimeUtc($source)
    try {
        [IO.File]::AppendAllText($source, "`n#error Expected_CMake_failure`n")
        [IO.File]::SetLastWriteTimeUtc($source, [datetime]'2000-01-01')
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -Action Build -Project $manifest -Runtime $Runtime
        Check ($LASTEXITCODE -ne 0) 'Broken source unexpectedly built'
        Check ((Get-FileHash -LiteralPath $manifest).Hash -eq $before) 'Failed build activated a candidate'
        Check ((Get-FileHash -LiteralPath $activeModule).Hash -eq $moduleHash) 'Failed build changed the active DLL'
    } finally {
        [IO.File]::WriteAllText($source, $original, [Text.UTF8Encoding]::new($false))
        [IO.File]::SetLastWriteTimeUtc($source, $timestamp)
    }
    Write-Output "CMake SDK workflow passed (create, enable, startup, no-op, add class, failure preservation): $project"
    $global:LASTEXITCODE = 0
} finally { $env:PAINENGINE_ENGINE_STORE = $previousStore }

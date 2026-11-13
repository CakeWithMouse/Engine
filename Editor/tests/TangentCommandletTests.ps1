param([ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$SkipBuild) { & "$repo/Engine/BuildSupport/Invoke-MSBuild.ps1" -Project "$repo/Editor/EditorModule.vcxproj" -Configuration $Configuration -EngineCommandlet }
$scratch=Join-Path $repo ('Engine/build/tangent-commandlets-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$tool="$repo/Game/build/x64/$Configuration/Runtime/EngineCommandlets.exe"
$count=0
function Invoke-Tangents($request, [int]$expected=0) {
    $script:count++
    $inputFile=Join-Path $scratch "$script:count-request.json"
    $outputFile=Join-Path $scratch "$script:count-result.json"
    $request | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $inputFile -Encoding utf8
    & $tool mesh.tangents $inputFile $outputFile *> (Join-Path $scratch "$script:count.log")
    if ($LASTEXITCODE -ne $expected) { throw "mesh.tangents exit $LASTEXITCODE, expected $expected; $scratch" }
    $result=Get-Content -Raw -LiteralPath $outputFile | ConvertFrom-Json
    if ($result.ok -ne ($expected -eq 0)) { throw 'Incorrect result status' }
    return $result
}
$request=@{vertices=@(
    @{position=@(0,0,0);normal=@(0,1,0);uv=@(0,0)},
    @{position=@(1,0,0);normal=@(0,1,0);uv=@(1,0)},
    @{position=@(0,0,1);normal=@(0,1,0);uv=@(0,1)}
);indices=@(0,1,2)}
$normal=Invoke-Tangents $request
foreach ($t in $normal.tangents) { if ($t[0] -lt .99 -or $t[3] -ne -1) { throw 'Expected tangent +X, bitangent +Z' } }
foreach ($v in $request.vertices) { $v.uv[0]=1-$v.uv[0] }
$mirror=Invoke-Tangents $request
foreach ($t in $mirror.tangents) { if ($t[0] -gt -.99 -or $t[3] -ne 1) { throw 'Mirrored UV must reverse tangent and handedness' } }
foreach ($v in $request.vertices) { $v.uv=@(.25,.25) }
$degenerate=Invoke-Tangents $request
foreach ($t in $degenerate.tangents) { if ($t[3] -ne 0) { throw 'Degenerate UV requires fallback' } }
$request.indices=@(0,1,9); $null=Invoke-Tangents $request 2
$request.indices=@(0,.5,2); $null=Invoke-Tangents $request 2
$request.indices=@(0,1); $null=Invoke-Tangents $request 2
$request.indices=@(0,1,2); $request.vertices[0].uv=@('bad',0); $null=Invoke-Tangents $request 2
Write-Output "Tangent commandlets: $count scenarios passed. Evidence: $scratch"

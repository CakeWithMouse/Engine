param([string]$Output="$PSScriptRoot/../../Engine/build/assimp-commandlets/$([guid]::NewGuid().ToString('N'))",
    [string]$BlendFixtures='',
    [string]$Commandlet="$PSScriptRoot/../../Game/build/x64/Release/Runtime/EngineCommandlets.exe")
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/../..");$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory'}
[IO.Directory]::CreateDirectory($Output)|Out-Null
$source="$Output/треугольник.obj"
[IO.File]::WriteAllText($source,"v -1 -1 0`nv 1 -1 0`nv 0 1 0`nf 1 2 3`n",[Text.UTF8Encoding]::new($false))
$cases=@(
    @{name='OBJ';source=$source},
    @{name='FBX';source="$repo/Engine/tests/assets/fixtures/auto-color.fbx"},
    @{name='GLB';source="$repo/Game/RuntimeExample/Content/Models/CesiumMan.glb"}
)
if($BlendFixtures){
    $BlendFixtures=[IO.Path]::GetFullPath($BlendFixtures)
    foreach($name in @('blend-external','blend-packed')){
        $cases+=@{name=$name;source="$BlendFixtures/$name.blend"}
    }
}
$results=@()
foreach($case in $cases){
    $name=$case.name
    @{source=$case.source;content="$Output/Проект/$name/Content"}|ConvertTo-Json|Set-Content "$Output/$name.request.json" -Encoding utf8
    & $Commandlet asset.import "$Output/$name.request.json" "$Output/$name.result.json" *> "$Output/$name.log"
    $exit=$LASTEXITCODE;$data=Get-Content "$Output/$name.result.json" -Raw|ConvertFrom-Json
    $passed=$exit -eq 0 -and $data.ok -and $data.placed -and $data.meshes -gt 0 -and $data.indices -gt 0
    if($name -eq 'GLB'){$passed=$passed -and $data.clips -gt 0}
    if($name -eq 'FBX' -or $name.StartsWith('blend-')){$passed=$passed -and ![string]::IsNullOrEmpty($data.material)}
    if($name.StartsWith('blend-')){$passed=$passed -and $data.asset.EndsWith('.glb') -and $data.indices -eq 36}
    $results+=@{format=$name;passed=[bool]$passed;exit=$exit;indices=$data.indices;clips=$data.clips}
}
'not a model'|Set-Content "$Output/corrupt.fbx"
@{source="$Output/corrupt.fbx";content="$Output/Corrupt/Content"}|ConvertTo-Json|Set-Content "$Output/Corrupt.request.json" -Encoding utf8
& $Commandlet asset.import "$Output/Corrupt.request.json" "$Output/Corrupt.result.json" *> "$Output/Corrupt.log"
$data=Get-Content "$Output/Corrupt.result.json" -Raw|ConvertFrom-Json
$results+=@{format='Corrupt';passed=($LASTEXITCODE -eq 2 -and !$data.ok -and !(Test-Path "$Output/Corrupt/Content/Models/corrupt"));exit=$LASTEXITCODE}
if($BlendFixtures){
    @{source="$BlendFixtures/blend-missing.blend";content="$Output/Missing/Content"}|ConvertTo-Json|Set-Content "$Output/Missing.request.json" -Encoding utf8
    & $Commandlet asset.import "$Output/Missing.request.json" "$Output/Missing.result.json" *> "$Output/Missing.log"
    $exit=$LASTEXITCODE;$data=Get-Content "$Output/Missing.result.json" -Raw|ConvertFrom-Json
    $results+=@{format='BlendMissing';passed=($exit -eq 2 -and !$data.ok -and $data.error.Contains('Missing base-color texture') -and !(Test-Path "$Output/Missing/Content/Models/blend-missing"));exit=$exit}
}
$results|ConvertTo-Json|Set-Content "$Output/results.json" -Encoding utf8
$results|ForEach-Object {Write-Output "$($_.format): $($_.passed)"}
if(@($results|Where-Object {!$_.passed}).Count){throw 'Assimp commandlet tests failed'}
# Negative cases intentionally return 2; the successful suite itself returns 0.
$global:LASTEXITCODE=0

param(
  [string]$EngineRoot = 'E:/Engine',
  [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
$EngineRoot = [IO.Path]::GetFullPath($EngineRoot)
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Choose a new output directory; preserving existing path: $Output" }
New-Item -ItemType Directory -Path $Output | Out-Null
$evidence = $PSScriptRoot
$commandlets = Join-Path $EngineRoot 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'
$launcher = Join-Path $EngineRoot 'Game/build/x64/Release/EngineLauncher.exe'
$runtime = Join-Path $EngineRoot 'Game/build/x64/Release/Runtime'
if (!(Test-Path $commandlets) -or !(Test-Path $launcher)) { throw 'Release commandlet/launcher binaries are required' }
function Write-JsonNoBom($path, $value) {
  $json = ConvertTo-Json -InputObject $value -Depth 30
  [IO.File]::WriteAllText($path, $json + "`n", [Text.UTF8Encoding]::new($false))
}
function Invoke-RenderCommand($name, $command, $request) {
  $requestPath = Join-Path $Output "$name.request.json"
  $resultPath = Join-Path $Output "$name.result.json"
  $logPath = Join-Path $Output "$name.log"
  Write-JsonNoBom $requestPath $request
  & $commandlets $command $requestPath $resultPath *> $logPath
  if ($LASTEXITCODE -ne 0) { throw "$command failed with exit $LASTEXITCODE; see $logPath" }
  $result = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
  if (!$result.ok) { throw "$command result returned ok=false; see $resultPath" }
}

# Recreate private material fixtures and run the real Forward/Deferred GPU material checks.
$materials = Join-Path $Output 'materials'
New-Item -ItemType Directory -Path $materials | Out-Null
foreach ($file in @('Opaque.hlsl','Opaque.mat','Masked.hlsl','Masked.mat','Mask.png','Translucent.hlsl','Translucent.mat')) {
  Copy-Item -LiteralPath (Join-Path $evidence $file) -Destination (Join-Path $materials $file)
}
$forward = Get-Content (Join-Path $evidence 'material-blends-forward.request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$forward.groups[0].material = Join-Path $materials 'Opaque.mat'
$forward.groups[1].material = Join-Path $materials 'Masked.mat'
$forward.groups[2].material = Join-Path $materials 'Translucent.mat'
$forward.frames[0].image = Join-Path $Output 'material-blends-forward.bmp'
Invoke-RenderCommand 'material-blends-forward' 'render.transparency' $forward
$over = Get-Content (Join-Path $evidence 'material-blends-sourceover.request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$over.groups[0].material = Join-Path $materials 'Opaque.mat'
$over.groups[1].material = Join-Path $materials 'Masked.mat'
$over.groups[2].material = Join-Path $materials 'Translucent.mat'
$over.frames[0].image = Join-Path $Output 'material-blends-sourceover.bmp'
Invoke-RenderCommand 'material-blends-sourceover' 'render.transparency' $over
$deferred = Get-Content (Join-Path $evidence 'material-blends-deferred.request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$deferred.groups[0].material = Join-Path $materials 'Opaque.mat'
$deferred.groups[1].material = Join-Path $materials 'Masked.mat'
$deferred.groups[2].material = Join-Path $materials 'Translucent.mat'
$deferred.frames[0].image = Join-Path $Output 'material-blends-deferred.bmp'
Invoke-RenderCommand 'material-blends-deferred' 'render.transparency' $deferred

# Recreate the primitive and imported skeletal caster point-light shadow A/B sequences.
$shadow = Get-Content (Join-Path $evidence 'shadows-final.request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
for ($i=0; $i -lt $shadow.frames.Count; $i++) { $shadow.frames[$i].image = Join-Path $Output "shadows-frame-$i.bmp" }
Invoke-RenderCommand 'shadows-final' 'render.point-shadows' $shadow
$cesium = Join-Path $Output 'imported-shadow-cesium-man.glb'
Copy-Item -LiteralPath (Join-Path $evidence 'imported-shadow-cesium-man.glb') -Destination $cesium
$imported = Get-Content (Join-Path $evidence 'imported-shadows.request.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$imported.model = $cesium
$imported.frames[0].image = Join-Path $Output 'imported-shadow-on.bmp'
$imported.frames[1].image = Join-Path $Output 'imported-shadow-off.bmp'
$imported.frames[2].image = Join-Path $Output 'imported-shadow-return.bmp'
Invoke-RenderCommand 'imported-shadows' 'render.point-shadows' $imported

# Rebuild the checked-in Bomber source archive, replace only the imported BaseMap image,
# then smoke the actual scene/model/material path and collect its frame.
$uvProject = Join-Path $Output 'BomberUvChartProbe'
$archive = Join-Path (Split-Path $evidence -Parent) 'tired-bomber-r7-source.zip'
Expand-Archive -LiteralPath $archive -DestinationPath $uvProject
& (Join-Path $uvProject 'Build.ps1') -Runtime $runtime *> (Join-Path $Output 'imported-uv-build.log')
if ($LASTEXITCODE -ne 0) { throw "source archive project build failed with exit $LASTEXITCODE" }
$chart = Join-Path $uvProject 'Content/Models/blend-uv/Textures/BaseColor_0.png'
Copy-Item -LiteralPath (Join-Path $evidence 'imported-uv-chart.png') -Destination $chart -Force
$oldHudProbe = $env:TB_HUD_PROBE
try {
  $env:TB_HUD_PROBE = '1'
  $projectJson = Join-Path $uvProject 'Project.json'
  $quotedProjectJson = '"{0}"' -f $projectJson
  $smoke = Start-Process -FilePath $launcher -ArgumentList @('--smoke-project-current', $quotedProjectJson) `
    -Wait -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $Output 'imported-uv-smoke.stdout.log') `
    -RedirectStandardError (Join-Path $Output 'imported-uv-smoke.stderr.log')
  if ($smoke.ExitCode -ne 0) { throw "UV chart project smoke failed with exit $($smoke.ExitCode)" }
} finally {
  if ($null -eq $oldHudProbe) { Remove-Item Env:TB_HUD_PROBE -ErrorAction SilentlyContinue }
  else { $env:TB_HUD_PROBE = $oldHudProbe }
}
Copy-Item -LiteralPath (Join-Path $uvProject 'Content/tired-bomber-frame.bmp') -Destination (Join-Path $Output 'imported-uv-chart-frame.bmp')

# Run the report-local analyzer over the replayed BMPs; its assertions validate all four cases.
$analyzer = Join-Path $Output 'material-shadow-uv-pixel-analysis.py'
Copy-Item -LiteralPath (Join-Path $evidence 'material-shadow-uv-pixel-analysis.py') -Destination $analyzer
python $analyzer
if ($LASTEXITCODE -ne 0) { throw 'pixel analyzer assertions failed' }
Write-Host "Replay evidence and asserted analysis saved to $Output"


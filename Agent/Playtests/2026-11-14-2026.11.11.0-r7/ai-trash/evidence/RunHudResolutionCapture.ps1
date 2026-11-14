$ErrorActionPreference = 'Stop'
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
$project = 'E:\Engine\Engine\build\playtests\20261114-r7\ai-trash\TrashGameUpgraded\Project.json'
$captureDir = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\ai-trash\evidence'
$tool = Join-Path $runtime 'EngineCommandlets.exe'

foreach ($size in @(@{W=1000;H=600}, @{W=640;H=480})) {
  $tag = "$($size.W)x$($size.H)"
  $request = Join-Path $captureDir "trash-hud-$tag.request.json"
  $result = Join-Path $captureDir "trash-hud-$tag.result.json"
  $bmp = Join-Path $captureDir "trash-hud-$tag.bmp"
  $log = Join-Path $captureDir "trash-hud-$tag.log"
  $json = [ordered]@{
    project = $project
    steps = @(
      @{kind='control'; action='play'},
      @{kind='resize'; width=$size.W; height=$size.H},
      @{kind='focus'; enabled=$true},
      @{kind='frames'; count=420},
      @{kind='control'; action='stop'}
    )
  } | ConvertTo-Json -Depth 8
  Set-Content -LiteralPath $request -Value $json -Encoding utf8

  $env:TRASH_SAVE_TEST = 'types'
  $env:PAINENGINE_D3D_SCREENSHOT = $bmp
  $env:PAINENGINE_D3D_SCREENSHOT_FRAME = '410'
  & $tool 'project.input' $request $result *> $log
  $exit = $LASTEXITCODE
  Set-Content -LiteralPath (Join-Path $captureDir "trash-hud-$tag.exit.txt") -Value $exit -Encoding utf8
  if ($exit -ne 0) { throw "project.input failed at $tag (exit=$exit); see $log" }
  if (!(Test-Path -LiteralPath $bmp)) { throw "No rendered D3D screenshot at $tag; see $log" }

  $bytes = [IO.File]::ReadAllBytes($bmp)
  $bmpSize = "{0}x{1}" -f [BitConverter]::ToInt32($bytes,18), [Math]::Abs([BitConverter]::ToInt32($bytes,22))
  if ($bmpSize -ne $tag) { throw "BMP size $bmpSize instead of $tag" }
  $check = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
  $frame = $check.steps | Where-Object { $_.kind -eq 'frames' } | Select-Object -Last 1
  $viewport = "{0}x{1}" -f $frame.camera.width, $frame.camera.height
  if ($viewport -ne $tag) { throw "Reported viewport $viewport instead of $tag" }
  Set-Content -LiteralPath (Join-Path $captureDir "trash-hud-$tag.verified.txt") -Value "exit=$exit; viewport=$viewport; bmp=$bmpSize; frames=$($check.frames); gameScene=Main; captureFrame=410" -Encoding utf8
  Write-Output "PASS $tag $bmp"
}

Remove-Item Env:TRASH_SAVE_TEST, Env:PAINENGINE_D3D_SCREENSHOT, Env:PAINENGINE_D3D_SCREENSHOT_FRAME

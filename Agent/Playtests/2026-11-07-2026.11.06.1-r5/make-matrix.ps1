$ErrorActionPreference='Stop'
$previous=Join-Path $PSScriptRoot '../2026-11-05-2026.11.05.7-r3/feature-matrix.csv'
$features=@(Import-Csv $previous | Where-Object feature_id -ne 'create-cpp' | Select-Object feature_id,name)
$features+=@(
    @{feature_id='mesh-tangents';name='GenerateTangents: обычные и зеркальные UV'},
    @{feature_id='tangent-degenerate';name='Вырожденные UV включают fallback w=0'},
    @{feature_id='tangent-gpu';name='TANGENT доходит до пиксельного шейдера'},
    @{feature_id='tangent-world-reflection';name='Normal map при отражённом мировом масштабе'},
    @{feature_id='tangent-savegame';name='Пересчёт тангентов после SaveGame'},
    @{feature_id='point-shadow-cubes';name='Две кубические карты точечных теней'},
    @{feature_id='shadow-cascade-fit';name='Область каскадов, snap и смешивание'},
    @{feature_id='audio-decode';name='WAV / MediaFoundation декодирование'},
    @{feature_id='audio-play2d';name='AudioEngine Play2D'},
    @{feature_id='audio-play3d';name='AudioEngine Play3D'},
    @{feature_id='audio-stream';name='Потоковое аудио'}
)
$roles=@('calm-city','curious-city','tired-bomber','ai-trash')
$passed=@(
    @('create-nocode','enable-cpp','build-activate','project-game','external','time','scene-startup','compatibility','savegame-derived','savegame-restart'),
    @('create-nocode','enable-cpp','build-activate','project-game','external','time','scene-startup','compatibility','savegame-derived','trace-screen','scene-create-delete','hud-events','mesh-tangents','scene-clone','scene-save-load'),
    @('create-nocode','enable-cpp','build-activate','project-game','external','time','scene-startup','compatibility','trace-world'),
    @('create-nocode','enable-cpp','build-activate','project-game','external','time','scene-startup','savegame-derived','savegame-restart','mesh-tangents','tangent-degenerate')
)
$partial=@(
    @('scene-transform','asset-paths','instances','lod','light-point','light-sun','shadows','diagnostics','lod-panel','material-slots','material-slot-persist'),
    @('play','input','subsystem','hud-elements','light-point','scene-properties','scene-undo'),
    @('play','input','instances','lod','camera-orthographic','particles','light-point','shadows','hud-elements','diagnostics','lod-panel','material-slots'),
    @('scene-transform','asset-paths','skeletal','hud-elements','material-slots','material-slot-persist')
)
$coordinator=@{
    'tangent-gpu'='passed: fixes/logs/tangent-before.log -> core-after.log (WARP pixels)';
    'tangent-world-reflection'='passed: fixes/logs/mirror-before.log -> core-after.log (WARP pixels)';
    'tangent-savegame'='passed: fixes/logs/savegame.log (CPU state)';
    'mesh-tangents'='passed: fixes/logs/tangent-commandlets-final.log (7 CPU scenarios)';
    'scene-clone'='passed: fixes/logs/point-light-after.log (shadow flag, data only)';
    'point-shadow-cubes'='partial: fixes/visual/battle-*.bmp (one lamp; static scene)';
    'shadow-cascade-fit'='partial: core-after.log math + static Battle image; camera motion unverified';
    'audio-decode'='partial: core-after.log WAV decoder; no listening test';
    'render-modes'='passed: static Battle Forward/Deferred frames; other scenes not compared';
    'compatibility'='passed: fixes/logs/compatibility.log (116 + legacy UI 387)';
    'model-import'='partial: fixes/logs/assets.log (229 checks; not every format/asset)';
    'material-editor'='partial: fixes/logs/editor-ui.log (5036 checks); UI grab is not GPU viewport evidence';
    'sdk-examples'='not-tested: packaged fresh C++ scaffold built; API example suite not rerun'
}
$rows=foreach($feature in $features){
    $id=$feature.feature_id
    $row=[ordered]@{feature_id=$id;name=$feature.name;criterion='Проверка в игровом фрагменте/точный сценарий в evidence.json; partial не означает полное покрытие';attempted_developers=0;successful_developers=0}
    for($i=0;$i -lt 4;$i++){
        $status=if($id -in $passed[$i]){'passed'}elseif($id -in $partial[$i]){'partial'}else{'not-tested'}
        $n=$i+1;$row["developer_$n"]=$status
        $row["evidence_$n"]=if($status -ne 'not-tested'){"$($roles[$i])/evidence.json"}else{''}
        if($status -ne 'not-tested'){$row.attempted_developers++}
        if($status -eq 'passed'){$row.successful_developers++}
    }
    $row.coverage=if($row.successful_developers -ge 2){'two-successful'}elseif($row.attempted_developers -ge 2){'partial-two-attempts'}else{'below-two'}
    $row.coordinator=if($coordinator.ContainsKey($id)){$coordinator[$id]}else{'not-counted-as-persona'}
    $row.visual=if($id -in @('external','shadows','light-point','instances','lod','hud-elements','diagnostics','lod-panel')){'Game frames observed: roles 1/3/4. Role2 and edit project views unverified.'}else{'See explicit evidence level; compilation/CPU pass is not visual confirmation.'}
    [pscustomobject]$row
}
$rows | Export-Csv -LiteralPath (Join-Path $PSScriptRoot 'feature-matrix.csv') -NoTypeInformation -Encoding utf8
@{features=$rows.Count;twoAttempted=@($rows | Where-Object attempted_developers -ge 2).Count;twoSuccessful=@($rows | Where-Object successful_developers -ge 2).Count;belowTwo=@($rows | Where-Object attempted_developers -lt 2 | ForEach-Object feature_id)} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $PSScriptRoot 'coverage.json') -Encoding utf8
Get-Content (Join-Path $PSScriptRoot 'coverage.json')

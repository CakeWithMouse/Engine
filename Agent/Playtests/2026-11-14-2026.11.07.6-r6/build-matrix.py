"""Conservative scenario-level coverage; never inherit previous-run passes."""
import csv
import json
from pathlib import Path

root = Path(__file__).resolve().parent
previous = root.parent / '2026-11-07-2026.11.06.1-r5/feature-matrix.csv'
roles = ['calm-city', 'curious-city', 'tired-bomber', 'ai-trash']
with previous.open(encoding='utf-8-sig', newline='') as source:
    rows = list(csv.DictReader(source))
for row in rows:
    row['criterion'] = 'Only the concrete scenario in the linked role evidence; partial does not validate the whole named subsystem.'
    for n in range(1, 5):
        row[f'developer_{n}'], row[f'evidence_{n}'] = 'not-tested', ''
    row['coordinator'] = 'See fixes/regressions.json, fixes/retries.json and fixes/report.md; not counted as a persona.'
    row['visual'] = 'See role evidence levels. Direct desktop observation unavailable (computer-use app approval timed out).'
by_id = {r['feature_id']: r for r in rows}
def mark(ids, people, status):
    for feature in ids.split():
        for n in people:
            by_id[feature][f'developer_{n}'] = status
            by_id[feature][f'evidence_{n}'] = roles[n-1] + '/evidence.json'

mark('create-nocode enable-cpp build-activate project-game external', [1,2,3,4], 'passed')
mark('compatibility time savegame-scene savegame-derived', [1,2,3,4], 'partial')
mark('savegame-restart', [1,3,4], 'passed')
mark('play', [2,3,4], 'passed')
mark('scene-create-delete scene-transform scene-codec hud-elements diagnostics', [1,2,3,4], 'partial')
mark('subsystem input trace-screen hud-events', [2], 'partial')
mark('input trace-world trace-filter trace-debug', [3,4], 'partial')
mark('camera-orthographic camera-perspective', [3], 'partial')
mark('camera-perspective', [1,2,4], 'partial')
mark('model-import', [1,2,3,4], 'partial')
mark('asset-reimport asset-paths', [2,4], 'passed')
mark('skeletal instances lod material-blends light-point shadows render-modes render-resize imported-shadows point-shadow-cubes', [1,2,3], 'partial')
mark('material-graph material-params material-editor', [2,4], 'partial')
mark('material-slots material-slot-persist', [1,3,4], 'partial')
mark('shadow-cascade-fit', [1,4], 'partial')
mark('hud-project hud-layout', [2,3,4], 'partial')
mark('completed-frame lod-panel', [1,3], 'partial')

new = [
 ('cascade-camera-stability', 'Каскады: стабильность сетки при orbit/dolly', '121-frame sequence; no XY scale changes, fractional texel drift < .002.', [1,4], 'passed'),
 ('point-shadow-motion', 'Point shadows: движение лампы и skeletal/VAT', 'Animated/light-motion depth hashes change; returning restores initial state.', [2,3], 'passed'),
 ('point-shadow-zero-slots', 'Point shadows: нулевая интенсивность освобождает слот', 'Three lights compete for two slots; zero-intensity light gets -1.', [1,2], 'passed'),
 ('point-shadow-lod-transition', 'Point shadows: переключение LOD', 'Camera distance changes selected LOD and returning restores shadow data; full fade matrix is coordinator coverage.', [1,2], 'partial'),
 ('material-graph-current', 'Материалы: граф/HLSL, применение и откат', 'Own material enable/save/preset/apply/revert; saved graph restored; preview pixels unverified.', [2,4], 'partial'),
 ('material-register-numeric-validation', 'Материалы: t32/t127 и неконечные JSON-числа', 'Production loader/register tests in EngineTests; not attempted by two personas.', [], 'not-tested'),
 ('transparency-camera-sorting', 'Прозрачность: движение камеры и слоёв', 'Own camera sequences preserve overlapping surfaces; no persona-independent pixel oracle.', [1,2,3], 'partial'),
 ('transparency-intersections', 'Прозрачность: пересечения и самопересечения', 'Own cross geometry and orbit frames; exact 16-layer edge cases covered by coordinator.', [1,2,3], 'partial'),
 ('transparency-animated-lod-resize', 'Прозрачность: skeletal/VAT, LOD и resize', 'Own skeletal/crowd camera/resize frames; exact crossfade oracle is coordinator coverage.', [2,3], 'partial'),
 ('import-scale-sidecar', 'Импорт: масштаб и sidecar применяются при размещении', 'Own selected scale matches written sidecar and new object transform.', [1,2,3,4], 'passed'),
 ('import-scale-reimport-move', 'Импорт: масштаб после переимпорта и переноса', 'Own object scales retained after real reimport/move and additional placement.', [2,4], 'passed'),
 ('import-scale-dialog-presets', 'Импорт: пресеты и Cancel диалога', 'Native Qt unit tests passed; personas used command parameters, not dialog interaction.', [], 'not-tested'),
]
for feature, name, criterion, people, status in new:
    row = dict(rows[0])
    row.update(feature_id=feature, name=name, criterion=criterion)
    for n in range(1,5):
        row[f'developer_{n}'] = status if n in people else 'not-tested'
        row[f'evidence_{n}'] = roles[n-1]+'/evidence.json' if n in people else ''
    rows.append(row)
for row in rows:
    statuses = [row[f'developer_{n}'] for n in range(1,5)]
    attempted = sum(s != 'not-tested' for s in statuses)
    successful = statuses.count('passed')
    row.update(attempted_developers=str(attempted), successful_developers=str(successful),
               coverage='two-successful' if successful >= 2 else 'two-attempted' if attempted >= 2 else 'below-two')
with (root/'feature-matrix.csv').open('w', encoding='utf-8-sig', newline='') as target:
    writer = csv.DictWriter(target, fieldnames=list(rows[0]))
    writer.writeheader(); writer.writerows(rows)
summary = dict(total=len(rows), attemptedByTwo=sum(int(r['attempted_developers'])>=2 for r in rows),
               successfulByTwo=sum(int(r['successful_developers'])>=2 for r in rows),
               belowTwo=[r['feature_id'] for r in rows if int(r['attempted_developers'])<2],
               newDetailsBelowTwo=[r[0] for r in new if len(r[3])<2],
               note='Partial does not prove a whole subsystem. Coordinator tests are separate. Historical passes are not carried forward.')
(root/'coverage.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps(summary, ensure_ascii=False, indent=2))

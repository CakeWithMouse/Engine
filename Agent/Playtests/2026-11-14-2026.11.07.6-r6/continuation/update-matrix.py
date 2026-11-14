"""Merge reviewed continuation evidence without rewriting historical observations."""
import csv
import json
from pathlib import Path

here = Path(__file__).resolve().parent
report = here.parent
repo = report.parents[2]
roles = ['calm-city', 'curious-city', 'tired-bomber', 'ai-trash']
with (here / 'matrix-before.csv').open(encoding='utf-8-sig', newline='') as stream:
    rows = list(csv.DictReader(stream))
by_id = {row['feature_id']: row for row in rows}
criteria = {}
merged = []
for number, role in enumerate(roles, 1):
    source = report / role / 'continuation/evidence.json'
    if not source.exists():
        continue
    document = json.loads(source.read_text(encoding='utf-8-sig'))
    entries = document if isinstance(document, list) else document.get('observations', document.get('features', []))
    assert entries, f'No observations: {source}'
    seen = set()
    for entry in entries:
        feature = entry['feature_id']
        assert feature in by_id and feature not in seen, (source, feature)
        seen.add(feature)
        status = entry['status']
        assert status in {'passed', 'partial', 'failed', 'blocked', 'not-tested'}, (feature, status)
        if status == 'not-tested':
            continue
        assert entry.get('expected') and entry.get('actual'), (source, feature, 'missing criterion/result')
        paths = entry.get('evidence_paths', [])
        assert paths, (source, feature, 'missing evidence paths')
        for value in paths:
            path = Path(value)
            candidates = [path] if path.is_absolute() else [source.parent / path, report / path, repo / path]
            assert any(candidate.is_file() for candidate in candidates), (source, feature, value)
        row = by_id[feature]
        row[f'developer_{number}'] = status
        row[f'evidence_{number}'] = source.relative_to(report).as_posix()
        criteria.setdefault(feature, []).append(f'{role}: {entry["expected"]}')
        merged.append(dict(role=role, feature_id=feature, status=status))

for row in rows:
    if row['feature_id'] in criteria:
        row['criterion'] = ' | '.join(criteria[row['feature_id']])
        row['visual'] = 'See per-role continuation evidence: measured API/Qt/GPU results are distinguished from direct desktop or listening observation.'
    states = [row[f'developer_{n}'] for n in range(1, 5)]
    attempted = sum(state != 'not-tested' for state in states)
    successful = states.count('passed')
    row.update(attempted_developers=str(attempted), successful_developers=str(successful),
               coverage='two-successful' if successful >= 2 else 'two-attempted' if attempted >= 2 else 'below-two')

summary = dict(total=len(rows), attemptedByTwo=sum(int(row['attempted_developers']) >= 2 for row in rows),
    successfulByTwo=sum(int(row['successful_developers']) >= 2 for row in rows),
    belowTwo=[row['feature_id'] for row in rows if int(row['attempted_developers']) < 2],
    belowTwoSuccessful=[row['feature_id'] for row in rows if int(row['successful_developers']) < 2],
    continuationObservations=len(merged),
    note='Actual independent persona scenarios. Partial/blocked are not full passes; visual and listening limits remain explicit.')
with (report / 'feature-matrix.csv').open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
(report / 'coverage.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
(here / 'merged-observations.json').write_text(json.dumps(merged, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2, ensure_ascii=False))

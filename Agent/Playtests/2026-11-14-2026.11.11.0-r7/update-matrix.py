"""Merge r7 evidence; retain explicit provenance for historical r6 passes."""
import csv
import hashlib
import json
import sys
import zipfile
from pathlib import Path

here = Path(__file__).resolve().parent
repo = here.parents[2]
old = here.parent / '2026-11-14-2026.11.07.6-r6'
roles = ['calm-city', 'curious-city', 'tired-bomber', 'ai-trash']
agent_by_role = {'calm-city': 'calm_city', 'ai-trash': 'calm_city',
                 'curious-city': 'curious_city', 'tired-bomber': 'tired_bomber'}
with (here / 'matrix-before.csv').open(encoding='utf-8-sig', newline='') as f:
    rows = list(csv.DictReader(f))
target_ids = {r['feature_id'] for r in rows if int(r['successful_developers']) < 2}
assert len(target_ids) == 48
for row in rows:
    row['baseline_criterion'] = row['criterion']
    if row['feature_id'] in target_ids:
        # Old partial criteria sometimes describe missing evidence (or requested
        # unavailable UI). Preserve them separately from this run's observations.
        row['criterion'] = ''
    for n in range(1, 5):
        if row[f'evidence_{n}']:
            row[f'evidence_{n}'] = f'../{old.name}/{row[f"evidence_{n}"]}'
    row['current_version_successful'] = '0'
    row['current_version_independent_agents'] = '0'
    row['provenance'] = 'Historical r6 until overridden by linked r7 evidence.'

new = {
    'ai-card-memory': 'FSM card, per-unit memory/reset/history/clone/save-reopen',
    'ai-graph-edit': 'Real state/link dialogs, graph history, variables/settings, save-reopen',
    'ai-validation': 'Node-specific error, refused Play, repair and successful Play',
    'ai-runtime-debug': 'State/history/memory and observed behavior over time',
    'ai-script-link': 'Script Noise to FSM and CallScriptEvent back to an observed script effect',
    'ai-file-reload': 'Write failure/retry, reopen and changed behavior on the next Play',
}
for feature, criterion in new.items():
    row = {key: '' for key in rows[0]}
    row.update(feature_id=feature, name=criterion, criterion=criterion,
               current_version_successful='0', provenance='New in r7')
    for n in range(1, 5):
        row[f'developer_{n}'] = 'not-tested'
    rows.append(row)
target_ids.update(new)
by_id = {r['feature_id']: r for r in rows}
merged, current = [], {}
for n, role in enumerate(roles, 1):
    source = here / role / 'evidence.json'
    if not source.exists():
        continue
    doc = json.loads(source.read_text(encoding='utf-8-sig'))
    entries = doc if isinstance(doc, list) else doc.get('features', doc.get('observations', []))
    seen = set()
    for e in entries:
        feature, status = e['feature_id'], e['status']
        assert feature in by_id and feature not in seen, (source, feature)
        seen.add(feature)
        assert status in {'passed', 'partial', 'failed', 'blocked', 'not-tested'}
        if status == 'not-tested':
            continue
        assert e.get('expected') and e.get('actual') and e.get('evidence_paths'), (role, feature)
        for value in e['evidence_paths']:
            path = Path(value)
            candidates = [path] if path.is_absolute() else [source.parent / path, here / path, repo / path]
            assert any(p.is_file() for p in candidates), (role, feature, value)
        row = by_id[feature]
        row[f'developer_{n}'], row[f'evidence_{n}'] = status, f'{role}/evidence.json'
        row['provenance'] = 'r7 observations replace corresponding r6 role statuses; untouched roles remain historical.'
        criterion = e['expected']
        if criterion.startswith('Only the concrete scenario'):
            criterion = e['actual']
        row['criterion'] += (' | ' if row['criterion'] else '') + f'{role}: {criterion}'
        row['visual'] = 'Per-role evidence separates measured runtime/Qt/GPU/audio results from source review.'
        current[(feature, role)] = status
        merged.append(dict(feature_id=feature, role=role, status=status))

for row in rows:
    statuses = [row[f'developer_{n}'] for n in range(1, 5)]
    attempted, passed = sum(s != 'not-tested' for s in statuses), statuses.count('passed')
    fresh = sum(current.get((row['feature_id'], role)) == 'passed' for role in roles)
    fresh_agents = {agent_by_role[role] for role in roles
                    if current.get((row['feature_id'], role)) == 'passed'}
    row.update(attempted_developers=str(attempted), successful_developers=str(passed),
               current_version_successful=str(fresh),
               current_version_independent_agents=str(len(fresh_agents)),
               coverage='two-successful' if passed >= 2 else 'below-two-successful')
summary = dict(total=len(rows), historicalBaselineTwoPassed=59,
    successfulByTwo=sum(int(r['successful_developers']) >= 2 for r in rows),
    currentVersionSuccessfulByTwo=sum(int(r['current_version_successful']) >= 2 for r in rows),
    currentVersionIndependentAgentsByTwo=sum(int(r['current_version_independent_agents']) >= 2 for r in rows),
    targetRows=len(target_ids),
    targetRowsComplete=sum(int(by_id[f]['current_version_independent_agents']) >= 2 for f in target_ids),
    remainingTargetRows=sorted(f for f in target_ids if int(by_id[f]['current_version_independent_agents']) < 2),
    belowTwoSuccessful=[r['feature_id'] for r in rows if int(r['successful_developers']) < 2],
    currentObservations=len(merged),
    note='59 historical rows retain versioned r6 evidence; 48 legacy gaps and 6 AI rows require fresh r7 passes from two independent agents. Calm-city and ai-trash share one agent. Root regressions do not count as a persona.')
with (here / 'feature-matrix.csv').open('w', encoding='utf-8-sig', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
(here / 'coverage.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(here / 'merged-observations.json').write_text(json.dumps(merged, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k: v for k, v in summary.items() if not isinstance(v, list)}, ensure_ascii=False))
if '--final' in sys.argv:
    assert not summary['remainingTargetRows'], summary['remainingTargetRows']
    assert not summary['belowTwoSuccessful'], summary['belowTwoSuccessful']
if '--final' in sys.argv or '--audit-artifacts' in sys.argv:
    artifact_audit = []
    for role in (roles if '--final' in sys.argv else [r for r in roles if (here / r / 'evidence.json').exists()]):
        source = here / role / 'evidence.json'
        doc = json.loads(source.read_text(encoding='utf-8-sig'))
        entries = doc if isinstance(doc, list) else doc['features']
        version = '' if isinstance(doc, list) else doc.get('engineVersion', '')
        assert all(e.get('engineVersion', version) == '2026.11.11.0'
                   for e in entries if e['status'] == 'passed'), (role, 'missing version')
        for entry in entries:
            if entry['status'] == 'not-tested':
                continue
            for value in entry['evidence_paths']:
                path = Path(value)
                candidates = [path] if path.is_absolute() else [source.parent / path, here / path, repo / path]
                resolved = next(p.resolve() for p in candidates if p.is_file())
                assert resolved.is_relative_to(here), (role, 'evidence remains outside report', value)
        for name in ['diary.md', 'feedback.md', 'REBUILD.md']:
            assert (source.parent / name).is_file(), (role, name)
        archives = list((here / role).rglob('*source*.zip'))
        assert archives, (role, 'missing source archive')
        for archive in archives:
            with zipfile.ZipFile(archive) as z:
                assert z.testzip() is None, archive
                for name in z.namelist():
                    path = Path(name)
                    assert not path.is_absolute() and '..' not in path.parts, (archive, name)
                    assert not {'sdk', 'binaries', 'intermediate', '.painengine', 'build', '.git'}.intersection(
                        part.lower() for part in path.parts), (archive, name)
                    # .obj is also a supported source mesh format.
                    assert path.suffix.lower() not in {'.exe', '.dll', '.lib', '.pdb', '.pch', '.ilk', '.exp', '.res'}, (archive, name)
                    if path.suffix.lower() == '.obj':
                        # Keep Wavefront source meshes, reject compiled COFF objects.
                        assert b'\0' not in z.read(name), (archive, 'binary .obj', name)
                artifact_audit.append(dict(role=role, path=str(archive.relative_to(here)),
                    bytes=archive.stat().st_size, entries=len(z.namelist()),
                    sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), crc='passed'))
    (here / 'artifact-audit.json').write_text(json.dumps(dict(
        validation='Source archive structure, forbidden-artifact exclusion and CRC. Fresh build/run evidence is linked separately in each role REBUILD.md.',
        archives=artifact_audit), indent=2)+'\n', encoding='utf-8')

"""Record old/new locks and source changes without modifying either project."""
import hashlib
import json
from pathlib import Path

report = Path(__file__).resolve().parent
repo = report.parents[2]
old_root = repo / 'Engine/build/playtests/20261107-r5'
new_root = repo / 'Engine/build/playtests/20261114-r6'
projects = [
    ('calm-city', 'CalmCityFinal', 'CalmCityUpgraded'),
    ('curious-city', 'final-city', 'upgraded-city'),
    ('tired-bomber', 'BomberFinalR5', 'BomberUpgrade'),
    ('ai-trash', 'TrashGame', 'TrashGameUpgraded'),
]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def sources(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in (root / 'Source').rglob('*') if p.is_file()}
results = []
for role, old_name, new_name in projects:
    old, new = old_root / role / old_name, new_root / role / new_name
    old_sources, new_sources = sources(old), sources(new)
    old_manifest, new_manifest = read(old / 'Project.json'), read(new / 'Project.json')
    item = dict(role=role, oldProject=str(old), newProject=str(new),
                oldLock=read(old / 'Engine.lock.json'), newLock=read(new / 'Engine.lock.json'),
                oldModuleSha256=digest(old / old_manifest['module']),
                newModuleSha256=digest(new / new_manifest['module']),
                oldSourceFiles=len(old_sources), newSourceFiles=len(new_sources),
                changedSource=[name for name in sorted(set(old_sources) | set(new_sources))
                               if old_sources.get(name) != new_sources.get(name)])
    if (new / 'UpgradeReport.json').exists():
        item['upgradeReport'] = read(new / 'UpgradeReport.json')
    results.append(item)
(report / 'project-comparison.json').write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps([dict(role=r['role'], old=r['oldLock']['engineVersion'], new=r['newLock']['engineVersion'],
                       changedSource=r['changedSource']) for r in results], ensure_ascii=False, indent=2))

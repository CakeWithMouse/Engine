"""Preserve small coordinator artifacts without copying builds or persona files."""
import hashlib
import json
import shutil
from pathlib import Path

here = Path(__file__).resolve().parent
repo = here.parents[3]
scratch = repo / 'Engine/build/playtests/20261114-r7'
destination = here / 'evidence'
destination.mkdir(exist_ok=True)
manifest = []
for source in sorted(scratch.iterdir()):
    if not source.is_file() or source.suffix not in {'.log', '.txt', '.json', '.png'}:
        continue
    assert source.stat().st_size < 2_000_000, source
    target = destination / source.name
    shutil.copy2(source, target)
    manifest.append(dict(source=str(source.relative_to(repo)), saved=source.name,
                         sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(here / 'evidence-manifest.json').write_text(
    json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
sdk = repo / 'Game/build/x64/Release/Runtime/SDK/Sdk.json'
if not sdk.exists():
    candidates = list((repo / 'Game/build/x64/Release/Runtime').rglob('Sdk.json'))
    assert len(candidates) == 1, candidates
    sdk = candidates[0]
shutil.copy2(sdk, here / 'sdk-manifest.json')
print(f'Preserved {len(manifest)} coordinator artifacts; SDK {sdk}')

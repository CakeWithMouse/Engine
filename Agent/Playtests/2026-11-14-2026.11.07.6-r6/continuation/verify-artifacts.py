"""Preserve compact coordinator evidence and verify supplementary source archives."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

here = Path(__file__).resolve().parent
report = here.parent
repo = report.parents[2]
scratch = repo / 'Engine/build/playtests/20261114-r6'
evidence = here / 'evidence'
evidence.mkdir(exist_ok=True)

logs = [
    'continuation-authoring-before-tests.log', 'continuation-authoring-after-tests.log',
    'continuation-normal-before-tests-host.log', 'continuation-normal-after-tests.log',
    'continuation-register-before-tests.log', 'continuation-register-after-tests.log',
    'continuation-register-integration-03.log', 'continuation-sky-before-tests-auto.log',
    'continuation-sky-after-tests.log', 'continuation-authoring-final.log',
    'continuation-integer-final.log', 'continuation-mesh-final.log',
    'continuation-particles-final.log', 'continuation-material-regression.log',
    'continuation-all-build.log', 'continuation-editor-final-build.log',
    'continuation-integer-build.log',
]
for name in logs:
    shutil.copy2(scratch / name, evidence / name)
for directory in ['continuation-authoring-regression-05', 'continuation-integer-final',
                  'continuation-mesh-final', 'continuation-particles-final',
                  'continuation-material-regression', 'continuation-point-integers']:
    destination = evidence / directory
    destination.mkdir(exist_ok=True)
    # Requests/results and logs are compact; representative images are in each role's report.
    for path in (scratch / directory).glob('*'):
        if path.is_file() and path.suffix in {'.json', '.log'}:
            shutil.copy2(path, destination / path.name)
graph = repo / 'Engine/build/graph-commandlets-2b2d3b46c48941d7aee67ba4814f02ff'
destination = evidence / 'graph-boundary'
destination.mkdir(exist_ok=True)
for name in ['request.json', 'result.json', 'run.log', 'graph.png']:
    shutil.copy2(graph / name, destination / name)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

archives = []
for path in sorted(report.glob('*/continuation/*.zip')):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None, path
        names = archive.namelist()
        assert not any(set(name.lower().replace('\\', '/').split('/')) &
                       {'sdk', 'binaries', 'intermediate', 'enginestore'} for name in names), path
        assert not any(Path(name).is_absolute() or '..' in Path(name).parts for name in names), path
        archives.append(dict(path=path.relative_to(report).as_posix(), entries=len(names),
                             bytes=path.stat().st_size, sha256=sha(path), sourceOnly=True))
assert len(archives) >= 3, 'All continuation roles must provide a supplementary source archive'
files = ['Engine/build/x64/Release/Engine.lib', 'Game/build/x64/Release/Runtime/SDK/Lib/Engine.lib',
         'Game/build/x64/Release/Runtime/EngineRuntime.exe', 'Game/build/x64/Release/Runtime/Editor.dll',
         'Game/build/x64/Release/Runtime/EngineCommandlets.exe',
         'Engine/Content/Shaders/BasicSky.hlsl',
         'Game/build/x64/Release/Runtime/EngineContent/Shaders/BasicSky.hlsl',
         'Engine/Content/Shaders/Material/MaterialTemplate.hlsl',
         'Game/build/x64/Release/Runtime/EngineContent/Shaders/Material/MaterialTemplate.hlsl']
fingerprints = {name: sha(repo / name) for name in files}
assert fingerprints[files[0]] == fingerprints[files[1]], 'SDK Engine.lib is stale'
assert fingerprints[files[5]] == fingerprints[files[6]], 'Runtime sky shader is stale'
assert fingerprints[files[7]] == fingerprints[files[8]], 'Runtime material shader is stale'
result = dict(archives=archives, fingerprints=fingerprints, preservedLogs=logs,
              note='Current continuation artifacts. Earlier r6 archives and first-phase fingerprints remain historical.')
(here / 'artifact-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(dict(archives=len(archives), fingerprints=len(fingerprints), copiedLogs=len(logs))))

"""Sequential shared builds; private test artifacts, complete per-suite results."""
import json
import os
from pathlib import Path
import subprocess
import time

repo = Path(__file__).resolve().parents[4]
report = Path(__file__).resolve().parent
scratch = repo / 'Engine/build/playtests/20261114-r6/regressions'
scratch.mkdir(parents=True, exist_ok=True)
logs = report / 'logs'
logs.mkdir(exist_ok=True)
env = dict(os.environ, PAINENGINE_ENGINE_STORE=str(scratch / 'EngineStore'))
tool = str(repo / 'Game/build/x64/Release/Runtime/EngineCommandlets.exe')
def ps(path, *args):
    return ['pwsh', '-NoProfile', '-File', path, *args]

cases = [
    ('engine', ps('Engine/tests/run.ps1', '-Configuration', 'Release', '-PlatformToolset', 'auto')),
    ('savegame', ps('Engine/tests/savegame/run.ps1')),
    ('assets', ps('Engine/tests/assets/run.ps1', '-Configuration', 'Release', '-EnableAssimp')),
    ('edit-session', ps('Engine/tests/edit-session/run.ps1', '-Configuration', 'Release')),
    ('input', ps('Engine/tests/input/run.ps1', '-Configuration', 'Release')),
    ('subsystems', ps('Engine/tests/subsystems/run.ps1')),
    ('skeletal', ps('Engine/tests/skeletal/run.ps1', '-Configuration', 'Release', '-EnableAssimp')),
    ('compatibility', ps('Engine/tests/compatibility/run.ps1')),
    ('component-generation', ps('Engine/tests/component-generation/run.ps1', '-Configuration', 'Release')),
    ('scene-module', ps('Engine/tests/scene-module/run.ps1', '-Configuration', 'Release')),
    ('render-diagnostics', ps('Engine/tests/render-diagnostics/run.ps1', '-Configuration', 'Release')),
    ('commandlets', ps('Editor/tests/CommandletTests.ps1', '-Output', str(scratch / 'commands'))),
    ('savegame-commandlets', ps('Editor/tests/SaveGameCommandletTests.ps1', '-Output', str(scratch / 'savegame'))),
    ('tangent-commandlets', ps('Editor/tests/TangentCommandletTests.ps1', '-Output', str(scratch / 'tangents'))),
    ('material-graph', ps('Editor/tests/MaterialGraphCommandletTests.ps1', '-Project', str(repo / 'Engine/build/playtests/20261114-r6/import-regression/Project/Project.json'))),
    ('material-render', ['python', 'Editor/tests/MaterialRenderTests.py', '--tool', tool, '--output', str(scratch / 'materials')]),
    ('point-motion', ['python', 'Editor/tests/PointShadowMotionTests.py', '--tool', tool, '--output', str(scratch / 'point'), '--model', str(repo / 'Game/Battle/Content/Models/CesiumMan.glb')]),
    ('point-slots-lod', ['python', 'Editor/tests/PointShadowSlotsLodTests.py', '--tool', tool, '--output', str(scratch / 'slots'), '--model', str(repo / 'Game/Battle/Content/Models/CesiumMan.glb')]),
    ('transparency', ['python', 'Editor/tests/TransparencyTests.py', '--tool', tool, '--output', str(scratch / 'transparency')]),
    ('intersections', ['python', 'Editor/tests/TransparencyIntersectionTests.py', '--tool', tool, '--output', str(scratch / 'intersections')]),
    ('api-reference', ['node', 'Engine/docs/api-reference/build.mjs', '--check']),
]
results = []
for name, command in cases:
    start = time.monotonic()
    with (logs / f'{name}.log').open('wb') as log:
        try:
            completed = subprocess.run(command, cwd=repo, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=900)
            result = {'name': name, 'exit': completed.returncode, 'passed': completed.returncode == 0}
        except (OSError, subprocess.TimeoutExpired) as error:
            result = {'name': name, 'passed': False, 'error': str(error)}
    result.update(command=command, seconds=round(time.monotonic() - start, 1), log=f'logs/{name}.log')
    results.append(result)
    (report / 'regressions.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(f'{name}: {result}', flush=True)
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

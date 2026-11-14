"""Exercise actual editor import callbacks, placement, reimport and move."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def suite(tool, launcher, output):
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, PAINENGINE_ENGINE_STORE=str(output / 'EngineStore'),
               TEMP=str(output), TMP=str(output))
    project = output / 'Project'
    subprocess.run([str(launcher), '--create-project', str(project), 'ImportScale'],
                   env=env, check=True, capture_output=True, timeout=120)
    source = output / 'Hero..v2.obj'
    source.write_text('v -1 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n', encoding='utf-8')
    cases = []

    def run(name, **kwargs):
        request = dict(project=str(project / 'Project.json'), source=str(source), **kwargs)
        inp, result = output / f'{name}.request.json', output / f'{name}.result.json'
        inp.write_text(json.dumps(request), encoding='utf-8')
        p = subprocess.run([str(tool), 'project.import', str(inp), str(result)],
                           env=env, capture_output=True, timeout=120)
        (output / f'{name}.log').write_bytes(p.stdout + p.stderr)
        data = json.loads(result.read_text(encoding='utf-8-sig'))
        assert p.returncode == 0 and data['ok'], (name, p.returncode, data)
        cases.append(name)
        return data

    # Invalid input cannot leave an imported bundle behind.
    for index, scale in enumerate([-1, 0, 10001, 0.00001, 'invalid', None, True]):
        before = sorted(str(p.relative_to(project)) for p in project.rglob('*'))
        data = run(f'invalid-{index}', scale=scale)
        assert not data['accepted'] and data['before'] == data['after'], data
        after = sorted(str(p.relative_to(project)) for p in project.rglob('*'))
        assert not [p for p in after if p not in before and 'Hero' in p], after

    for index, options in enumerate(['{', '[]']):
        data = run(f'invalid-options-{index}', options=options)
        assert not data['accepted'] and data['before'] == data['after'], data

    data = run('dotted-reimport-move', scale=0.0254, reimportSource=str(source),
               moveTo='/Game/Models/Moved/Hero..v2.obj', image=str(output / 'editor-layout.png'))
    assert data['accepted'] and data['sidecarExists'] and not data['dialogs'], data
    assert data['sidecar']['version'] == 1, data
    assert abs(data['sidecar']['model']['scale'] - 0.0254) < 1e-6, data
    before_ids = {o['id'] for o in data['before']}
    for stage, count in [('spawned', 1), ('afterReimport', 2), ('afterMove', 3)]:
        placed = [o for o in data[stage] if o['id'] not in before_ids]
        assert len(placed) == count, (stage, placed)
        # The editor's v1 sidecar retains placement scaling; v2 geometry baking
        # is covered independently by the engine import/reimport tests.
        assert all(all(abs(v - 0.0254) < 1e-6 for v in o['scale']) for o in placed), (stage, placed)
    data = run('default-scale')
    placed = [o for o in data['after'] if o['id'] not in {x['id'] for x in data['before']}]
    assert len(placed) == 1 and placed[0]['scale'] == [1, 1, 1], data
    summary = {'passed': len(cases), 'cases': cases,
               'visual': 'editor-layout.png is Qt layout only; D3D viewport needs separate observation'}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--launcher', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    suite(args.tool.resolve(strict=True), args.launcher.resolve(strict=True), args.output.resolve())

"""Real D3D11 frames and cube depth readback; run against a built or packaged commandlet."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run_suite(tool, output, model):
    output.mkdir(parents=True, exist_ok=False)
    cases = []

    def run(name, request):
        for i, frame in enumerate(request['frames']):
            frame['image'] = str(output / f'{name}-{i:02}.bmp')
        source, result = output / f'{name}.json', output / f'{name}-result.json'
        source.write_text(json.dumps(request), encoding='utf-8')
        with (output / f'{name}.log').open('w', encoding='utf-8') as log:
            process = subprocess.run([str(tool), 'render.point-shadows', str(source), str(result)],
                                     stdout=log, stderr=subprocess.STDOUT, timeout=120)
        data = json.loads(result.read_text(encoding='utf-8-sig'))
        assert process.returncode == 0 and data.get('ok'), (name, data)
        assert len(data['frames']) == len(request['frames'])
        for frame in data['frames']:
            assert Path(frame['image']).stat().st_size > 54
        cases.append({'name': name, 'frames': len(data['frames']), 'adapter': data['adapter']})
        return data['frames']

    def depth(frame):
        return [face['sha256'] for face in frame['faces']]

    def pixels(frame):
        return hashlib.sha256(Path(frame['image']).read_bytes()).hexdigest()

    for rendering in (0, 1):
        for scene_lighting in (False, True):
            reference = None
            for caster, manual in (('mesh', False), ('skeletal', False), ('crowd', False), ('crowd', True)):
                name = f'motion-r{rendering}-scene{int(scene_lighting)}-{caster}-manual{int(manual)}'
                states = [(0, 0, True), (.5, 0, True), (.5, 6, True), (0, 6, True),
                          (0, 0, True), (0, 0, False), (0, 0, True), (0, 0, True)]
                frames = []
                for time, light_x, shadows in states:
                    frames.append({'time': time, 'position': [6 if time == .5 and caster == 'mesh' else 0, 1, 0],
                                   'light': [light_x, 4, 0], 'shadows': shadows})
                data = run(name, {'caster': caster, 'manualTexture': manual, 'rendering': rendering,
                                 'sceneLighting': scene_lighting, 'intensity': .7,
                                 'probes': [[0, 0, 0], [8, 0, 0], [-2, 0, 0], [6, 0, 0]], 'frames': frames})
                for i, probe in enumerate((0, 1, 3, 2)):
                    assert data[i]['count'] == 1 and data[i]['probes'][probe]['occluded'], (name, i)
                if scene_lighting:
                    assert not any(f['directionalShadows'] for f in data), (name, 'sun shadows unexpectedly enabled')
                assert not data[1]['probes'][0]['occluded'], (name, 'old shadow persists')
                assert data[5]['count'] == 0, (name, 'disable ignored')
                assert depth(data[0]) == depth(data[4]) == depth(data[6]) == depth(data[7]), (name, 'stale cube')
                assert pixels(data[0]) == pixels(data[4]) == pixels(data[6]) == pixels(data[7]), (name, 'nondeterministic image')
                assert pixels(data[5]) != pixels(data[6]), (name, 'shadow not visible in colour pass')
                actual = [depth(frame) for frame in data if frame['count']]
                if reference is None:
                    reference = actual
                else:
                    assert actual == reference, (name, 'animated shadow differs from rigid reference')

        for scene_lighting in (False, True):
            name = f'global-off-r{rendering}-scene{int(scene_lighting)}'
            data = run(name, {'caster': 'crowd', 'sceneLighting': scene_lighting, 'globalShadows': False,
                              'rendering': rendering, 'frames': [{'time': .5}], 'probes': [[8, 0, 0]]})
            assert data[0]['count'] == int(scene_lighting), (name, 'global/scene switch contract')
            assert not data[0]['directionalShadows']
            if scene_lighting:
                assert data[0]['probes'][0]['occluded']

        # Continuous joint light/bone motion through the -Y/+X seam.
        for manual in (False, True):
            frames = []
            for i in range(31):
                time = i / 60
                light_x = -4 + i * 8 / 30
                center_x = 12 * time
                receiver_x = light_x + (center_x - light_x) * 4 / 3
                frames.append({'time': time, 'light': [light_x, 4, 0], 'probes': [[receiver_x, 0, 0]]})
            name = f'seam-r{rendering}-manual{int(manual)}'
            data = run(name, {'caster': 'crowd', 'manualTexture': manual, 'sceneLighting': True,
                              'rendering': rendering, 'intensity': .7, 'frames': frames})
            assert all(f['count'] == 1 and f['probes'][0]['occluded'] for f in data), (name, 'seam dropout')
            assert len({tuple(depth(f)) for f in data}) == len(data), (name, 'stale moving-light cube')

        # Bind origin lies outside the lamp; its animated hull is inside.
        for manual in (False, True):
            name = f'radius-r{rendering}-manual{int(manual)}'
            data = run(name, {'caster': 'crowd', 'manualTexture': manual, 'sceneLighting': True,
                              'rendering': rendering, 'radius': 5, 'intensity': .7,
                              'probes': [[0, 0, 0]], 'frames': [{'time': .5, 'position': [-6, 1, 0]}]})
            assert data[0]['probes'][0]['occluded'], (name, 'animated caster radius culled')

        if model:
            for caster in ('skeletal', 'crowd'):
                name = f'import-r{rendering}-{caster}'
                data = run(name, {'caster': caster, 'model': str(model), 'sceneLighting': True,
                                  'rendering': rendering, 'scale': 3, 'intensity': .7,
                                  'frames': [{'time': t, 'position': [0, .05, 0], 'light': [x, 5, -2]}
                                             for t, x in ((0, 0), (.25, 0), (.5, 2), (.75, -2), (0, 0))]})
                assert all(f['count'] == 1 for f in data)
                assert depth(data[0]) != depth(data[1]), (name, 'animation has no shadow change')
                assert depth(data[0]) == depth(data[-1]) and pixels(data[0]) == pixels(data[-1]), (name, 'return failed')

        name = f'replace-lod-r{rendering}'
        data = run(name, {'caster': 'crowd', 'replaceLodOffset': [6, 0, 0], 'sceneLighting': True,
                          'rendering': rendering, 'radius': 5, 'intensity': .7,
                          'probes': [[0, 0, 0]], 'frames': [{'time': .5, 'position': [-12, 1, 0]}]})
        assert data[0]['probes'][0]['occluded'], (name, 'stale envelope after LOD replacement')

    summary = {'passed': len(cases), 'frames': sum(c['frames'] for c in cases), 'cases': cases}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New directory; never overwrites captures')
    parser.add_argument('--model', type=Path, help='Optional CesiumMan.glb for imported skeletal/VAT smoke')
    args = parser.parse_args()
    run_suite(args.tool.resolve(strict=True), args.output.resolve(), args.model.resolve(strict=True) if args.model else None)

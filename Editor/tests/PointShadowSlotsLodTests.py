"""Two shadow cubes, slot reuse and animated LOD transitions on the real D3D11 renderer."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess


def suite(tool, output, model):
    output.mkdir(parents=True, exist_ok=False)
    cases = []
    def run(name, request, verify):
        for i, frame in enumerate(request['frames']):
            frame['image'] = str(output / f'{name}-{i:02}.bmp')
        source, result = output / f'{name}.json', output / f'{name}-result.json'
        source.write_text(json.dumps(request), encoding='utf-8')
        with (output / f'{name}.log').open('w', encoding='utf-8') as log:
            process = subprocess.run([str(tool), 'render.point-shadows', str(source), str(result)],
                                     stdout=log, stderr=subprocess.STDOUT, timeout=150)
        data = json.loads(result.read_text(encoding='utf-8-sig'))
        errors = []
        def check(condition, message):
            if not condition:
                errors.append(message)
        check(process.returncode == 0 and data.get('ok'), f'command failed: {data}')
        if not errors:
            verify(data['frames'], check)
        cases.append({'name': name, 'frames': len(data.get('frames', [])), 'errors': errors,
                      'adapter': data.get('adapter')})
        print(f'{name}: ' + ('PASS' if not errors else '; '.join(errors)), flush=True)

    def hashes(cube):
        return tuple(f['sha256'] for f in cube['faces'])
    def pixels(frame):
        return hashlib.sha256(Path(frame['image']).read_bytes()).hexdigest()

    base = [dict(position=[-3, 4, -2], color=[1, .45, .2], intensity=.7, radius=30, enabled=True, shadows=True),
            dict(position=[3, 4, 2], color=[.25, .5, 1], intensity=.7, radius=30, enabled=True, shadows=True),
            dict(position=[0, 5, 4], color=[.3, 1, .35], intensity=.7, radius=30, enabled=True, shadows=True)]
    for rendering in (0, 1):
        for caster in ('mesh', 'skeletal', 'crowd'):
            states = []
            changes = [[], [(0, 'shadows', False)], [], [(1, 'enabled', False)],
                       [(0, 'shadows', False), (1, 'shadows', False)],
                       [(i, 'shadows', False) for i in range(3)], [], [(0, 'intensity', 0)],
                       [(0, 'intensity', 0), (1, 'intensity', 0)], [(0, 'radius', 1)], [],
                       [(0, 'position', [4, 4, -2]), (1, 'position', [-4, 4, 2])], []]
            for change in changes:
                state = copy.deepcopy(base)
                for i, key, value in change:
                    state[i][key] = value
                states.append(state)
            frames = []
            for state in states:
                probes = []
                for light in state:
                    x, y, z = light['position']
                    probes.append([x + (1-x)*y/(y-1), 0, z-z*y/(y-1)])
                frames.append({'lights': state, 'position': [1 if caster == 'mesh' else 0, 1, 0],
                               'time': .25, 'probes': probes})
            def verify_slots(data, check, states=states):
                seen = {}
                for index, (frame, state) in enumerate(zip(data, states)):
                    selected = [i for i, l in enumerate(state) if l['enabled'] and l['shadows'] and l['intensity'] > 0 and l['radius'] > 1.5][:2]
                    check(frame['count'] == len(selected), f'frame {index}: count {frame["count"]} != {len(selected)}')
                    actual = [l['slot'] for l in frame['lights']]
                    expected = [selected.index(i) if i in selected else -1 for i in range(3)]
                    check(actual == expected, f'frame {index}: slots {actual} != {expected}')
                    for slot, owner in enumerate(selected):
                        if slot >= len(frame['cubes']):
                            continue
                        cube = frame['cubes'][slot]
                        light = state[owner]
                        check(cube['light'] == light['position'] + [light['radius']], f'frame {index}: cube {slot} wrong owner')
                        check(cube['probes'][owner]['occluded'], f'frame {index}: owner {owner} lost shadow')
                        key = (owner, tuple(light['position']), light['radius'])
                        if key in seen:
                            check(hashes(cube) == seen[key], f'frame {index}: owner {owner} stale/cross-wired cube')
                        else:
                            seen[key] = hashes(cube)
                check(all(pixels(data[i]) == pixels(data[0]) for i in (2, 6, 10, 12)), 'return image changed')
                check(pixels(data[0]) != pixels(data[5]), 'shadow on/off colour pass identical')
            run(f'slots-r{rendering}-{caster}', {'caster': caster, 'travel': 2, 'rendering': rendering,
                'sceneLighting': True, 'lights': base, 'frames': frames}, verify_slots)

        # Fixed pose/lights: only camera distance changes. A colour cross-fade must also change
        # point depth gradually, instead of holding the fine silhouette to the last frame.
        for transition in (0, 1):
            threshold = (40, 20)[transition]
            heights = [threshold*x for x in (1.21, 1.15, 1.10, 1.05, 1, .95, .90, .85, .79)]
            # viewportHeight * cot(FOV/2) * bind radius = 720 for the unit cube.
            frames = []
            direction = [10, 11, -16]
            length = math.sqrt(sum(x*x for x in direction))
            for height in heights + heights[::-1]:
                distance = 720 / height
                camera = [direction[0]*distance/length, 1+direction[1]*distance/length, direction[2]*distance/length]
                frames.append({'camera': camera, 'target': [0, 1, 0], 'time': .25})
            def verify_fade(data, check, transition=transition):
                check(all(f['count'] == 2 for f in data), 'both lights must cast throughout LOD fade')
                for i in range(9):
                    check([hashes(c) for c in data[i]['cubes']] == [hashes(c) for c in data[-1-i]['cubes']], f'fade return depth differs at {i}')
                    check(pixels(data[i]) == pixels(data[-1-i]), f'fade return image differs at {i}')
                for i in range(1, 8):
                    lod = data[i]['lod']['perLod']
                    check(lod[transition] == 1 and lod[transition+1] == 1, f'frame {i}: expected two colour LODs, got {lod}')
                for slot in (0, 1):
                    distinct = len({hashes(f['cubes'][slot]) for f in data[:9]})
                    check(distinct >= 6, f'cube {slot}: only {distinct} depth states across 9 LOD-fade frames (hard pop)')
                    check(hashes(data[0]['cubes'][slot]) != hashes(data[8]['cubes'][slot]), f'cube {slot}: fixture LOD silhouettes identical')
            for material in (False, True):
                run(f'fade-r{rendering}-{transition}' + ('-material' if material else ''),
                    {'caster': 'crowd', 'travel': 2, 'rendering': rendering, 'material': material,
                     'sceneLighting': True, 'lights': base[:2], 'lodScales': [1, .6, .3],
                     'lodScreenHeights': [40, 20], 'lodFade': .2, 'frames': frames}, verify_fade)

        for imported in (False, True):
            if imported and not model:
                continue
            frames = []
            distances = [12, 19.9, 20.1, 29.9, 30.1, 40, 12]
            for distance in distances:
                frames.append({'camera': [0, 1+distance*.6, -distance*.8], 'target': [0, 1, 0],
                               'time': .25, 'position': [0, 1, 0]})
            def verify_distance(data, check):
                expected = [0, 0, 1, 1, 2, 2, 0]
                for i, (frame, level) in enumerate(zip(data, expected)):
                    check(frame['count'] == 2, f'frame {i}: missing cube')
                    check(frame['lod']['perLod'] == [int(j == level) for j in range(3)], f'frame {i}: wrong LOD {frame["lod"]}')
                for a, b in ((0, 1), (2, 3), (4, 5), (0, 6)):
                    check([hashes(c) for c in data[a]['cubes']] == [hashes(c) for c in data[b]['cubes']], f'camera changed shadow within LOD {a}/{b}')
                check(pixels(data[0]) == pixels(data[-1]), 'distance return image differs')
            request = {'caster': 'crowd', 'travel': 2, 'rendering': rendering, 'sceneLighting': True,
                       'lights': base[:2], 'lodDistances': [20, 30], 'frames': frames}
            if imported:
                request['model'] = str(model)
                request['scale'] = 2
            else:
                request['lodScales'] = [1, .6, .3]
            run(f'distance-r{rendering}-import{int(imported)}', request, verify_distance)

        if model:
            for caster in ('skeletal', 'crowd'):
                frames = []
                for i in list(range(15)) + list(range(13, -1, -1)):
                    distance = 12 + i*2
                    lights = copy.deepcopy(base)
                    lights[0]['position'][0] += i*.1
                    lights[1]['position'][2] -= i*.1
                    if 5 <= i < 10:
                        lights[0]['shadows'] = False
                    if 10 <= i < 14:
                        lights[1]['enabled'] = False
                    frames.append({'camera': [0, 1+distance*.6, -distance*.8], 'target': [0, 1, 0],
                                   'time': i/30, 'position': [0, .05, 0], 'lights': lights})
                def verify_joint(data, check, caster=caster, states=frames):
                    seen_lods = set()
                    for index, (frame, state) in enumerate(zip(data, states)):
                        selected = [i for i, l in enumerate(state['lights']) if l['enabled'] and l['shadows']][:2]
                        check(frame['count'] == 2, f'frame {index}: missing cube during joint motion')
                        check([l['slot'] for l in frame['lights']] == [selected.index(i) if i in selected else -1 for i in range(3)],
                              f'frame {index}: wrong slots during joint motion')
                        for slot, owner in enumerate(selected):
                            cube = frame['cubes'][slot]
                            check(all(abs(a-b)<1e-5 for a,b in zip(cube['light'][:3], state['lights'][owner]['position'])),
                                  f'frame {index}: stale cube position')
                            check(any(face['occupied'] for face in cube['faces']), f'frame {index}: empty cube')
                        if caster == 'crowd':
                            seen_lods.update(i for i, count in enumerate(frame['lod']['perLod']) if count)
                    if caster == 'crowd':
                        check(seen_lods == {0, 1, 2}, f'joint motion missed LODs: {seen_lods}')
                    for i in range(14):
                        check([hashes(c) for c in data[i]['cubes']] == [hashes(c) for c in data[-1-i]['cubes']], f'joint motion return depth differs at {i}')
                        check(pixels(data[i]) == pixels(data[-1-i]), f'joint motion return image differs at {i}')
                run(f'joint-r{rendering}-{caster}', {'caster': caster, 'model': str(model), 'scale': 2,
                    'rendering': rendering, 'sceneLighting': True, 'lights': base,
                    'lodDistances': [20, 30], 'frames': frames}, verify_joint)

    summary = {'passed': sum(not c['errors'] for c in cases), 'total': len(cases),
               'frames': sum(c['frames'] for c in cases), 'cases': cases}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k != 'cases'}))
    return summary['passed'] == summary['total']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--model', type=Path)
    args = parser.parse_args()
    raise SystemExit(0 if suite(args.tool.resolve(strict=True), args.output.resolve(),
                               args.model.resolve(strict=True) if args.model else None) else 1)

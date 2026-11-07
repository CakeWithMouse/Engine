"""Layer compositing, camera motion, global instance sorting and transparent LOD on D3D11."""
import argparse
import copy
import json
import math
from pathlib import Path
import struct
import subprocess


def image(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert bits in (24, 32)
    stride = (w * bits + 31) // 32 * 4
    def pixel(x, y):
        row = h - 1 - y if h > 0 else y
        b, g, r = data[offset + row * stride + x * (bits // 8):][:3]
        return (r, g, b)
    return w, abs(h), pixel


def suite(tool, output):
    output.mkdir(parents=True, exist_ok=False)
    (output / 'Layer.hlsl').write_text('void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo=i.Color.rgb; o.Alpha=i.Color.a; }\n')
    materials = {}
    for blend in ('opaque', 'translucent', 'additive'):
        path = output / (blend + '.mat')
        path.write_text(json.dumps({'shader': 'Layer.hlsl', 'render': {'blend': blend, 'lighting': 'unlit', 'twoSided': True}}))
        materials[blend] = str(path)
    def layer(z, color, x=0):
        return dict(position=[x, 0, z], color=color, alpha=.5)
    red, green, blue = [1, 0, 0], [0, 1, 0], [0, 0, 1]
    def group(kind, layers, blend='translucent', **kw):
        return dict(kind=kind, layers=layers, material=materials[blend], width=8, height=4, **kw)
    def frame(eye=(0, 0, -8), target=(0, 0, 0), **kw):
        return dict(camera=eye, target=target, **kw)
    cases = []
    def run(name, groups, frames, mode, ortho=False, forbid_impostor=False):
        frames = copy.deepcopy(frames)
        for i, f in enumerate(frames):
            f['image'] = str(output / f'{name}-{i:02}.bmp')
        request = dict(groups=groups, frames=frames, rendering=mode, orthographic=ortho)
        source, result = output / (name + '.json'), output / (name + '-result.json')
        source.write_text(json.dumps(request))
        with (output / (name + '.log')).open('w') as log:
            process = subprocess.run([str(tool), 'render.transparency', str(source), str(result)], stdout=log, stderr=subprocess.STDOUT, timeout=180)
        data = json.loads(result.read_text(encoding='utf-8-sig'))
        errors = []
        if process.returncode or not data.get('ok'):
            errors.append(f'command failed: {data}')
        details = []
        for i, actual in enumerate(data.get('frames', [])):
            f = frames[i]
            eye = f['camera']; direction = [t-e for t, e in zip(f['target'], eye)]
            entries = []
            layer_index = 0
            for g in groups:
                blend = Path(g['material']).stem
                for l in g['layers']:
                    position = f['positions'][layer_index] if f.get('positions') else l['position']
                    layer_index += 1
                    # Independent ray/plane coverage check for layers outside the camera frustum.
                    if abs(direction[2]) < 1e-6:
                        continue
                    t = (position[2]-eye[2])/direction[2]
                    if t <= 0 or abs(eye[0]+t*direction[0]-position[0]) > g['width']/2 or abs(eye[1]+t*direction[1]-position[1]) > g['height']/2:
                        continue
                    depth = sum((p-e)*d for p, e, d in zip(position, eye, direction))
                    entries.append((depth, l['color'], l['alpha'], blend))
            w, h, pixel = image(actual['image'])
            expected = [c / 255 for c in pixel(0, 0)]
            opaque = [e for e in entries if e[3] == 'opaque']
            limit = min((e[0] for e in opaque), default=math.inf)
            if opaque:
                expected = list(min(opaque, key=lambda e: e[0])[1])
            for depth, color, alpha, blend in sorted(entries, key=lambda e: -e[0]):
                if depth >= limit or blend == 'opaque':
                    continue
                expected = [min(1, c*alpha + old*(1-alpha if blend == 'translucent' else 1)) for c, old in zip(color, expected)]
            expected = [round(c*255) for c in expected]
            samples = [pixel(w//2+x, h//2+y) for y in range(-8, 8) for x in range(-8, 8)]
            bad = sum(max(abs(c-e) for c, e in zip(p, expected)) > 3 for p in samples)
            details.append(dict(frame=i, expected=expected, center=pixel(w//2, h//2), wrongPixels=bad, lod=actual['lod'], drawCalls=actual['drawCalls']))
            if bad:
                errors.append(f'frame {i}: {bad}/256 pixels violate source-over/additive composition; center={pixel(w//2,h//2)}, expected={expected}')
            if forbid_impostor and any(l['impostor'] >= 0 for l in actual['lod']):
                errors.append(f'frame {i}: transparent surface entered opaque impostor atlas')
        cases.append(dict(name=name, frames=len(data.get('frames', [])), errors=errors, measurements=details, adapter=data.get('adapter')))
        print(name + ': ' + ('PASS' if not errors else '; '.join(errors[:3])), flush=True)

    for mode in (0, 1):
        for kind in ('mesh', 'copies', 'instances', 'skeletal', 'crowd'):
            groups = [group(kind, [layer(0, red, 3), layer(1.5, blue)])]
            groups[0]['width'] = 20
            run(f'{mode}-{kind}-pan', groups,
                [frame([x, 0, -8], [x, 0, 0]) for x in (0, -1, -2, -3, -4, -3, -2, -1, 0, 2, 4, 0)], mode)
            # All parallel layers overlap at the centre, from either side of the stack.
            frames = []
            for angle in (0, 25, 50, 25, 0, 155, 180, 205, 180, 0):
                a = math.radians(angle)
                frames.append(frame([8*math.sin(a), 0, -8*math.cos(a)], time=.25))
            run(f'{mode}-{kind}-orbit', [group(kind, [layer(0, red), layer(1, green), layer(2, blue)])], frames, mode)
        for kind in ('instances', 'crowd'):
            g = group(kind, [layer(0, red), layer(2, green), layer(4, blue)], lods=3, lodDistances=[9, 11])
            run(f'{mode}-{kind}-lod-order', [g], [frame([0, 0, z]) for z in (-8, -9, -10, -12, -10, -9, -8)], mode)
            groups = [group(kind, [layer(0, red), layer(3, blue)], lods=2, lodDistances=[9]), group('mesh', [layer(1.5, green)])]
            run(f'{mode}-{kind}-interleave', groups, [frame(), frame([0, 0, 8]), frame()], mode)
            g = group(kind, [layer(0, red)], lods=2, fade=.25)
            g['width'], g['height'] = 2.4, 3.2  # exact radius 2; orthographic pixel height 128
            run(f'{mode}-{kind}-fade', [g], [frame(fadeWeight=i/16) for i in range(17)] + [frame(fadeWeight=w) for w in (.5, .0001, .9999)], mode, ortho=True)
            g = group(kind, [layer(0, red), layer(1, blue)], lods=2, lodScreenHeights=[200], fade=.25)
            run(f'{mode}-{kind}-dolly-fade', [g], [frame([0, 0, -z]) for z in (8, 9, 10, 11, 12, 11, 10, 9, 8)], mode)
            growing = [layer(0, red), layer(2, blue)] + [layer(2+i*.001, blue, 1000) for i in range(298)]
            visible = [[0, 0, l['position'][2]] for l in growing]
            run(f'{mode}-{kind}-stream-growth', [group(kind, growing)], [frame(), frame(positions=visible), frame()], mode)
        for blend in ('translucent', 'additive'):
            run(f'{mode}-{blend}-impostor', [group('instances', [layer(0, red)], blend, impostor=True, lodDistances=[1])],
                [frame(), frame([0, 0, -12]), frame()], mode, forbid_impostor=True)
        run(f'{mode}-mixed-blends', [group('instances', [layer(0, red)]), group('copies', [layer(1, blue)], 'additive')], [frame(), frame([0, 0, 8])], mode)
        run(f'{mode}-opaque-occlusion', [group('instances', [layer(0, red), layer(2, blue)]), group('mesh', [layer(1, green)], 'opaque')], [frame(), frame([0, 0, 8])], mode)
        run(f'{mode}-moving-layers', [group('instances', [layer(0, red), layer(2, blue)])],
            [frame(positions=[[0,0,z], [0,0,2-z]]) for z in (0, .5, 1.5, 2, 1.5, .5, 0)], mode)
        run(f'{mode}-equal-depth', [group('mesh', [layer(0, red)]), group('instances', [layer(0, blue)])], [frame(), frame([1, 0, -8], [1, 0, 0]), frame()], mode)
    summary = dict(cases=cases, passed=sum(not c['errors'] for c in cases), frames=sum(c['frames'] for c in cases))
    (output / 'summary.json').write_text(json.dumps(summary, indent=2))
    return int(summary['passed'] != len(cases))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(suite(args.tool.resolve(), args.output.resolve()))

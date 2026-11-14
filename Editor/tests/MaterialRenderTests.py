"""Real D3D11 material passes: assets, parameters, texture t33, blending and animated shadows."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def pixels(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert bits in (24, 32), bits
    stride = ((width * bits + 31) // 32) * 4
    result = []
    for y in range(abs(height)):
        row = offset + y * stride
        for x in range(width):
            b, g, r = data[row + x * (bits // 8):row + x * (bits // 8) + 3]
            result.append((r, g, b))
    return result


def suite(tool, output):
    output.mkdir(parents=True, exist_ok=False)
    # White texture proves a real user SRV above the reserved point-shadow register is bound.
    header = bytearray(18)
    header[2] = 2
    struct.pack_into('<HH', header, 12, 1, 1)
    header[16:18] = bytes((32, 0x28))
    (output / 'white.tga').write_bytes(header + bytes((255, 255, 255, 255)))
    (output / 'Surface.hlsl').write_text('''
cbuffer MaterialParams : register(b1) {
    float4 Tint = float4(0.05, 0.8, 0.05, 1);
    float Opacity = 1;
};
Texture2D BaseMap : register(t33);
void Surface(MaterialInput i, inout MaterialOutput o) {
    o.Albedo = Tint.rgb * BaseMap.Sample(MaterialSampler, i.UV).rgb;
    o.Alpha = Opacity * (i.UV.x < 0.5 ? 0.25 : 1);
    o.SpecularWeight = 0;
}
''', encoding='utf-8')
    cases = []
    for rendering in (0, 1):
        for caster in ('mesh', 'skeletal', 'crowd'):
            for blend in ('opaque', 'masked', 'translucent', 'additive'):
                for lighting in ('lit', 'unlit'):
                    name = f'{rendering}-{caster}-{blend}-{lighting}'
                    material = output / f'{name}.mat'
                    material.write_text(json.dumps({
                        'version': 1, 'shader': 'Surface.hlsl',
                        'render': {'blend': blend, 'lighting': lighting, 'alphaClip': .5,
                                   'twoSided': blend == 'masked', 'castShadows': True},
                        'params': {'Tint': [.05, .8, .05, 1], 'Opacity': 1},
                        'textures': {'BaseMap': 'white.tga'},
                    }), encoding='utf-8')
                    frames = []
                    for alpha, tint, time in [(0, [.05, .8, .05, 1], 0),
                                              (.25, [.05, .8, .05, 1], 0),
                                              (1, [.05, .8, .05, 1], 0),
                                              (1, [.8, .05, .05, 1], 0),
                                              (1, [.05, .8, .05, 1], 0),
                                              (1, [.05, .8, .05, 1], .25)]:
                        frames.append({'time': time, 'materialParams': {'Opacity': alpha, 'Tint': tint},
                                       'image': str(output / f'{name}-{len(frames)}.bmp')})
                    request = {'rendering': rendering, 'caster': caster, 'width': 480, 'height': 360,
                               'camera': [5, 5, -7], 'target': [1, 1, 0], 'travel': 3,
                               'materialAsset': str(material), 'sceneLighting': True,
                               'lights': [dict(position=[-3, 4, -2], color=[1, 1, 1], intensity=.8, radius=30),
                                          dict(position=[3, 4, 2], color=[1, 1, 1], intensity=.8, radius=30)],
                               'frames': frames}
                    source, result = output / f'{name}.json', output / f'{name}-result.json'
                    source.write_text(json.dumps(request), encoding='utf-8')
                    with (output / f'{name}.log').open('w') as log:
                        process = subprocess.run([str(tool), 'render.point-shadows', str(source), str(result)],
                                                 stdout=log, stderr=subprocess.STDOUT, timeout=150)
                    data = json.loads(result.read_text(encoding='utf-8-sig'))
                    errors = []
                    def check(condition, message):
                        if not condition:
                            errors.append(message)
                    check(process.returncode == 0 and data.get('ok'), f'command failed: {data}')
                    if not errors:
                        actual = data['frames']
                        hashes = [hashlib.sha256(Path(f['image']).read_bytes()).hexdigest() for f in actual]
                        depth = [tuple(face['sha256'] for cube in f['cubes'] for face in cube['faces']) for f in actual]
                        # Additive blends into the bright neutral floor: use channel differences,
                        # since a ratio rejects correct pastel pixels near white.
                        green = [sum(g > r + 10 and g > b + 10 for r, g, b in pixels(f['image'])) for f in actual]
                        red = sum(r > g + 10 and r > b + 10 for r, g, b in pixels(actual[3]['image']))
                        check(all(f['count'] == 2 and not f['material']['errors'] for f in actual), 'two cubes or material compilation failed')
                        check(all(f['material']['gbuffer'] == (lighting == 'lit' and blend in ('opaque', 'masked')) for f in actual), 'incorrect G-buffer routing')
                        check(green[2] > 30 and red > 30, f'parameter/texture not visible: green={green[2]}, red={red}')
                        check(hashes[2] == hashes[4] and depth[2] == depth[4], 'restoring parameters did not restore the frame')
                        if blend == 'opaque':
                            check(hashes[0] == hashes[1] == hashes[2], 'opaque incorrectly uses alpha')
                        elif blend == 'masked':
                            check(green[0] == 0 and green[1] == 0, 'masked low alpha was not clipped')
                            check(depth[0] == depth[1] and depth[1] != depth[2], 'masked alpha did not clip shadow maps')
                        else:
                            check(green[0] == 0 and hashes[0] != hashes[1] != hashes[2], 'transparent/additive alpha not blended')
                            check(depth[0] == depth[1] == depth[2], 'transparent surface casts an opaque shadow')
                        if caster != 'mesh':
                            check(hashes[4] != hashes[5], 'animated surface did not move')
                            if blend in ('opaque', 'masked'):
                                check(depth[4] != depth[5], 'animated material shadow did not move')
                    cases.append({'name': name, 'frames': len(data.get('frames', [])), 'errors': errors,
                                  'adapter': data.get('adapter')})
                    print(name + ': ' + ('PASS' if not errors else '; '.join(errors)), flush=True)
    summary = {'cases': cases, 'passed': sum(not c['errors'] for c in cases),
               'frames': sum(c['frames'] for c in cases)}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return 0 if summary['passed'] == len(cases) else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(suite(args.tool.resolve(), args.output.resolve()))

"""Real normal-map tangent pixels, reflected transforms, SaveGame, and asset kinds."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave


def suite(tool, output):
    output.mkdir(parents=True, exist_ok=False)
    cases = []

    def run(name, command, request):
        inp, out = output / f'{name}.request.json', output / f'{name}.result.json'
        inp.write_text(json.dumps(request), encoding='utf-8')
        process = subprocess.run([str(tool), command, str(inp), str(out)], capture_output=True, timeout=120)
        (output / f'{name}.log').write_bytes(process.stdout + process.stderr)
        result = json.loads(out.read_text(encoding='utf-8-sig'))
        assert process.returncode == 0 and result['ok'], (name, result)
        cases.append(name)
        return result

    (output / 'Basis.hlsl').write_text('void Surface(MaterialInput i, inout MaterialOutput o) {'
        'o.Albedo=abs(i.Tangent.w)<0.5?float3(1,0,1):NormalFromMap(i,float3(.5,1,.5),1)*.5+.5;}', encoding='utf-8')
    material = output / 'Basis.mat'
    material.write_text(json.dumps(dict(shader='Basis.hlsl', render=dict(
        lighting='unlit', twoSided=True, castShadows=False))), encoding='utf-8')
    for mode in [0, 1]:
        for mirrored in [False, True]:
            name = f'mesh-{mode}-uv-{int(mirrored)}'
            result = run(name, 'render.mesh', dict(material=str(material), rendering=mode, mirrorUv=mirrored,
                frames=[dict(scale=scale, image=str(output / f'{name}-{index}.bmp'), saveRoundTrip=index==3)
                    for index, scale in enumerate([[1,1,1],[-2,1,.5],[1,1,-1],[1,1,1]])]))
            frames = result['frames']
            assert frames[0]['center'] != [255, 0, 255], result
            assert frames[1]['center'] == frames[0]['center'], result
            assert abs(frames[2]['center'][2] + frames[0]['center'][2] - 255) <= 1, result
            assert frames[3]['restored'] and frames[3]['sha256'] == frames[0]['sha256'], result
            assert frames[3]['basis'] == result['originalBasis'], result

    bmp = output / 'Imported.bmp'
    bmp.write_bytes(struct.pack('<2sIHHI', b'BM', 58, 0, 0, 54) +
        struct.pack('<IiiHHIIiiII', 40, 1, 1, 1, 24, 0, 4, 0, 0, 0, 0) + bytes([20, 70, 210, 0]))
    wav = output / 'Imported.wav'
    with wave.open(str(wav), 'wb') as audio:
        audio.setnchannels(1); audio.setsampwidth(2); audio.setframerate(8000)
        audio.writeframes(struct.pack('<16h', *range(16)))
    content = output / 'ImportedContent'
    for kind, source in [('texture', bmp), ('audio', wav)]:
        result = run('import-' + kind, 'asset.import', dict(source=str(source), content=str(content), kind=kind))
        assert not result['placed'] and result['kind'] == kind
        assert result['sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
        assert (content / result['asset'].removeprefix('/Game/')).read_bytes() == source.read_bytes()
    valid = run('simplify-grounded', 'mesh.simplify', dict(radius=2, slices=32, stacks=18,
        ratio=.35, error=.15, offset=[0,2,0]))
    assert valid['source']['usable'] and valid['reduced']['usable']
    assert 0 < valid['reduced']['triangles'] < valid['source']['triangles']
    invalid = run('simplify-pivot-warning', 'mesh.simplify', dict(radius=.7, slices=16, stacks=10, ratio=.55, error=.05))
    assert not invalid['source']['usable'] and any('Pivot' in s for s in invalid['source']['problems'])
    assert 0 < invalid['reduced']['triangles'] < invalid['source']['triangles']
    summary = dict(passed=len(cases), meshFrames=16,
        result='Both renderers preserve tangent pixels under X reflection and SaveGame; Z reflection reverses V. Texture/audio imports preserve bytes.')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    suite(args.tool.resolve(strict=True), args.output.resolve())

"""Observe real GPU particle collision against a surface in both renderers."""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def red_pixels(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert bits == 24
    stride = (width * 3 + 3) & ~3
    return sum(data[offset+y*stride+x*3+2] > 180 and
               data[offset+y*stride+x*3+1] < 80 and
               data[offset+y*stride+x*3] < 80
               for y in range(abs(height)) for x in range(width))


def suite(tool, output):
    output.mkdir(parents=True, exist_ok=False)
    results = {}
    for mode in [0, 1]:
        for surface in [True, False]:
            for collision in [False, True]:
                name = f'mode-{mode}-surface-{int(surface)}-collision-{int(collision)}'
                request = dict(rendering=mode, surface=surface, depthCollision=collision,
                    frames=[dict(steps=steps, image=str(output / f'{name}-{n}.bmp'))
                            for n, steps in enumerate([1, 10, 20, 30])])
                inp, out = output / f'{name}.request.json', output / f'{name}.result.json'
                inp.write_text(json.dumps(request), encoding='utf-8')
                process = subprocess.run([str(tool), 'render.particles', str(inp), str(out)],
                                         capture_output=True, timeout=120)
                (output / f'{name}.log').write_bytes(process.stdout + process.stderr)
                data = json.loads(out.read_text(encoding='utf-8-sig'))
                assert process.returncode == 0 and data['ok'], data
                assert all(frame['dispatches'] > 0 and frame['instances'] >= 128 for frame in data['frames'])
                results[mode, surface, collision] = data
        off, on = results[mode, True, False], results[mode, True, True]
        assert all(a['sha256'] == b['sha256'] for a, b in zip(off['frames'][:3], on['frames'][:3]))
        assert off['frames'][-1]['sha256'] != on['frames'][-1]['sha256']
        assert red_pixels(Path(on['frames'][-1]['image'])) > 10
        assert red_pixels(Path(off['frames'][-1]['image'])) == 0
        a, b = results[mode, False, False], results[mode, False, True]
        assert all(x['sha256'] == y['sha256'] for x, y in zip(a['frames'], b['frames']))
    summary = dict(passed=8, frames=32, renderers=['Forward', 'Deferred'],
        result='Before-contact frames match; collision retains visible particles above floor, disabled particles pass below; absent surface never changes trajectory.')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    suite(args.tool.resolve(strict=True), args.output.resolve())

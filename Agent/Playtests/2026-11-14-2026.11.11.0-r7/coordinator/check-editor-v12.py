"""Verify actual play backbuffer resize, camera projection and cursor path."""
import json
import math
import os
from pathlib import Path
import struct
import subprocess

root = Path('E:/Engine')
scratch = root / 'Engine/build/playtests/20261114-r7'
env = dict(os.environ)
for suffix, frame in [('', 2), ('_2', 4)]:
    env['PAINENGINE_D3D_SCREENSHOT' + suffix] = str(scratch / ('input-v12' + suffix + '.bmp'))
    env['PAINENGINE_D3D_SCREENSHOT_FRAME' + suffix] = str(frame)
steps = [
    {'kind': 'control', 'action': 'play'},
    {'kind': 'resize', 'width': 1280, 'height': 720},
    {'kind': 'focus', 'enabled': True},
    {'kind': 'cursor', 'x': 1150, 'y': 40, 'valid': True},
    {'kind': 'frames', 'count': 2},
    {'kind': 'resize', 'width': 640, 'height': 480},
    {'kind': 'frames', 'count': 2}]
inp, out = scratch / 'input-v12.request.json', scratch / 'input-v12.result.json'
inp.write_text(json.dumps({'project': str(scratch / 'editor-scene-smoke/Project.json'), 'steps': steps}), encoding='utf-8')
with (scratch / 'input-v12.log').open('w', encoding='utf-8') as log:
    subprocess.run([str(root / 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'),
                    'project.input', str(inp), str(out)], stdout=log, stderr=subprocess.STDOUT,
                   check=True, timeout=90, env=env)
r = json.loads(out.read_text(encoding='utf-8-sig'))
assert r['ok']
for index, suffix, expected in [(4, '', (1280, 720)), (6, '_2', (640, 480))]:
    data = (scratch / ('input-v12' + suffix + '.bmp')).read_bytes()
    width, height = struct.unpack_from('<ii', data, 18)
    assert (width, abs(height)) == expected
    camera = r['steps'][index]['camera']
    assert (camera['width'], camera['height']) == expected
    p = camera['projection']
    assert math.isclose(p[5] / p[0], expected[0] / expected[1], rel_tol=1e-6)
print('Actual Play backbuffers and camera aspect: 1280x720 -> 640x480 passed')

"""Exercise native camera drag and capture the material preview swapchain."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct

root = Path('E:/Engine')
scratch = root / 'Engine/build/playtests/20261114-r7'
project = scratch / 'editor-scene-smoke'
exe = root / 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'
def run(name, command, request, env=None):
    inp, out = scratch / (name + '.request.json'), scratch / (name + '.result.json')
    inp.write_text(json.dumps(request), encoding='utf-8')
    with (scratch / (name + '.log')).open('w', encoding='utf-8') as log:
        subprocess.run([str(exe), command, str(inp), str(out)], stdout=log,
                       stderr=subprocess.STDOUT, check=True, env=env, timeout=90)
    result = json.loads(out.read_text(encoding='utf-8-sig'))
    assert result['ok']
    return result

camera = run('editor-v10', 'editor.scene', {'project': str(project / 'Project.json'), 'steps': [
    {'action': 'frames', 'count': 1},
    {'action': 'viewport-drag', 'button': 'right', 'fromX': .5, 'fromY': .5, 'toX': .58, 'toY': .55},
    {'action': 'frames', 'count': 1}]})
before, after = camera['steps'][0]['snapshot'], camera['steps'][-1]['snapshot']
assert before['camera']['view'] != after['camera']['view']
assert before['objects'] == after['objects'], 'Camera drag changed scene objects'

materials = project / 'Content/Materials'
materials.mkdir(exist_ok=True)
for suffix in ('.mat', '.hlsl'):
    shutil.copy2(scratch / ('calm-city/CalmCityUpgraded/Content/Materials/ParameterProbe' + suffix), materials)
env = dict(os.environ)
frame = scratch / 'material-v10-native.bmp'
env['PAINENGINE_D3D_SCREENSHOT'] = str(frame)
env['PAINENGINE_D3D_SCREENSHOT_FRAME'] = '40'
preview = run('material-v10', 'material.graph', {
    'project': str(project / 'Project.json'), 'material': '/Game/Materials/ParameterProbe.mat',
    'previewOnly': True, 'steps': [{'action': 'save'} for _ in range(4)]}, env)
data = frame.read_bytes()
assert data[:2] == b'BM'
width, height = struct.unpack_from('<ii', data, 18)
size = [width, abs(height)]
assert size == preview['nativePreviewSize'], (size, preview['nativePreviewSize'])
assert size != [640, 480]
assert preview['projectViewportMinimized']
print('Native RMB camera drag preserves scene; captured actual material preview', preview['nativePreviewSize'])

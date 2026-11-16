"""Exercise actual UI Editor actions and HUD selection OK/Cancel persistence."""
import json
from pathlib import Path
import subprocess
import time

root = Path('E:/Engine')
scratch = root / 'Engine/build/playtests/20261114-r7'
project = scratch / 'editor-scene-smoke'
name = 'HudProbe' + str(time.time_ns())
ui = project / 'Content/UI' / (name + '.ui')
ui.parent.mkdir(exist_ok=True)
image = scratch / (name + '.png')
steps = [
    {'action': 'ui-create', 'path': str(ui)},
    {'action': 'ui-add', 'type': 'Label'},
    {'action': 'ui-field', 'name': 'UIName', 'value': 'Header'},
    {'action': 'ui-field', 'name': 'UIText', 'value': '\u0413\u043e\u0440\u043e\u0434 & HUD'},
    {'action': 'ui-field', 'name': 'UIAnchor', 'index': 2},
    {'action': 'ui-field', 'name': 'UIRect0', 'value': 24},
    {'action': 'ui-field', 'name': 'UIRect1', 'value': 18},
    {'action': 'ui-add', 'type': 'Button'},
    {'action': 'ui-field', 'name': 'UIName', 'value': 'Build'},
    {'action': 'ui-field', 'name': 'UIText', 'value': 'Build house'},
    {'action': 'ui-field', 'name': 'UIRect1', 'value': 80},
    {'action': 'ui-select', 'target': 'Header'},
    {'action': 'ui-action', 'name': 'UISave', 'image': str(image), 'scope': 'ui'},
    {'action': 'ui-close'},
    {'action': 'ui-open', 'path': str(ui)},
    {'action': 'hud-settings', 'choice': 'UI/' + name + '.ui'},
    {'action': 'hud-settings', 'choice': '', 'cancel': True},
    {'action': 'hud-settings', 'cancel': True},
    {'action': 'hud-settings', 'choice': ''},
    {'action': 'hud-settings', 'choice': 'UI/' + name + '.ui'},
    {'action': 'ui-close'}]
inp, out = scratch / 'editor-v11.request.json', scratch / 'editor-v11.result.json'
inp.write_text(json.dumps({'project': str(project / 'Project.json'), 'steps': steps}), encoding='utf-8')
with (scratch / 'editor-v11.log').open('w', encoding='utf-8') as log:
    subprocess.run([str(root / 'Game/build/x64/Release/Runtime/EngineCommandlets.exe'),
                    'editor.scene', str(inp), str(out)], stdout=log, stderr=subprocess.STDOUT,
                   check=True, timeout=90)
result = json.loads(out.read_text(encoding='utf-8-sig'))
assert result['ok'] and all(s['status'] == 0 for s in result['steps'])
saved = result['steps'][12]['snapshot']
assert saved['uiSelected'] == 'Header'
assert saved['uiMarkup'] == saved['uiSavedMarkup']
assert 'Header' in saved['uiMarkup'] and 'Build house' in saved['uiMarkup']
assert saved['uiMarkup'] == result['steps'][14]['snapshot']['uiMarkup']
choice = 'UI/' + name + '.ui'
for index in (15, 16, 17, 19):
    assert result['steps'][index]['snapshot']['hudChoice'] == choice
assert result['steps'][17]['hudDialog']['beforeChoice'] == choice
assert result['steps'][18]['snapshot']['hudChoice'] == ''
assert image.is_file()
print('UI Editor add/Inspector/hierarchy/save/reopen and HUD OK/Cancel/reopen passed')

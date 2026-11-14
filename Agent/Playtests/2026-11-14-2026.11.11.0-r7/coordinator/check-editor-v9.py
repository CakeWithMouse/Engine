"""Check the recorded native interaction scenario, not Qt widget snapshots alone."""
import json
import math
from pathlib import Path

source = Path(__file__).resolve().parent / 'evidence/editor-v9.result.json'
result = json.loads(source.read_text(encoding='utf-8-sig'))
steps = result['steps']
assert result['ok'] and all(step['status'] == 0 for step in steps)
assert steps[2]['snapshot']['coordinateSpaceLocal']
assert not steps[3]['snapshot']['coordinateSpaceLocal']

def position(step):
    return next(o['position'] for o in step['snapshot']['objects'] if o['name'] == 'Cube')

assert position(steps[4]) != position(steps[3]), 'Native drag did not move the selected cube'
assert position(steps[5]) == position(steps[3]), 'Undo failed to restore the position'
sizes = []
for step in steps:
    if step['action'] != 'resize-window':
        continue
    camera = step['snapshot']['camera']
    size = camera['width'], camera['height']
    assert list(size) == step['snapshot']['viewportSize']
    sizes.append(size)
    projection = camera['projection']
    assert math.isclose(projection[5] / projection[0], size[0] / size[1], rel_tol=1e-6)
assert len(set(sizes)) == 2, 'Actual camera/backbuffer dimensions did not change'
print('Native World/Local, drag, Undo and camera resize/projection checks passed')

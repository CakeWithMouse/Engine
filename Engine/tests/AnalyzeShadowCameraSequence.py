"""Measure world-space texel-grid drift in Battle --camera-sequence output.

Whole-texel translations are expected. A fractional translation of a fixed world
point means the shadow sampling lattice moved. Pixel differences alone cannot
distinguish that from the camera's intended motion.
"""
import csv
import json
import math
import pathlib
import sys


def analyze(folder):
    with (folder / 'matrices.csv').open(encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 121 * 3 or len(list(folder.glob('frame-*.bmp'))) != 121:
        raise ValueError(f'{folder}: expected 121 complete frames and 363 matrix samples')
    result = []
    for cascade in range(3):
        selected = [row for row in rows if int(row['cascade']) == cascade]
        if [int(row['frame']) for row in selected] != list(range(121)):
            raise ValueError(f'{folder}: missing or unordered cascade {cascade} samples')
        matrices = [[float(row[f'm{r}{c}']) for r in range(4) for c in range(4)]
                    for row in selected]
        if any(not all(math.isfinite(value) for value in m) or
               any(sum(m[axis*4+i]**2 for i in range(3)) < 1e-12 for axis in (0, 1))
               for m in matrices):
            raise ValueError(f'{folder}: invalid or disabled shadow matrices')
        scale_changes = 0
        max_phase_drift = 0.0
        worst_frame = 0
        for frame, (before, after) in enumerate(zip(matrices, matrices[1:]), 1):
            scale_changes += any(abs(after[i] - before[i]) > 1e-7 * max(abs(before[i]), 1e-8)
                                 for i in (0, 1, 2, 4, 5, 6))
            for point in ((0, 0, 0), (17, 0, 31), (-43, 12, -29)):
                for axis in (0, 1):
                    base = axis * 4
                    delta = 1024 * (sum((after[base+i]-before[base+i])*point[i]
                                       for i in range(3)) + after[base+3]-before[base+3])
                    drift = abs(delta - round(delta))
                    if drift > max_phase_drift:
                        max_phase_drift, worst_frame = drift, frame
        result.append(dict(cascade=cascade, frames=len(matrices), scaleChanges=scale_changes,
                           maxFractionalTexelDrift=max_phase_drift, worstFrame=worst_frame))
    return dict(sequence=str(folder), cascades=result)


if __name__ == '__main__':
    check = '--check' in sys.argv
    results = [analyze(pathlib.Path(arg)) for arg in sys.argv[1:] if arg != '--check']
    if not results:
        raise SystemExit('Usage: AnalyzeShadowCameraSequence.py [--check] DIR [DIR ...]')
    print(json.dumps(results, indent=2))
    # Matrix serialization/float precision gets 0.002 texel tolerance, far below a visible shift.
    if check and any(c['maxFractionalTexelDrift'] > .002 or c['scaleChanges'] != 0
                     for result in results for c in result['cascades']):
        raise SystemExit(1)

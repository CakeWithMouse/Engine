import json, re, sys
from pathlib import Path

log = Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace')
samples = {}
for line in log.splitlines():
    if '[CascadeProbe] sample=' not in line:
        continue
    name = re.search(r'sample=([^ ]+)', line).group(1)
    fields = {}
    for key in ('camera', 'sphere', 'matrix', 'shadowParams', 'splits'):
        match = re.search(rf'{key}=([^ ]+)', line)
        fields[key] = [float(x) for x in match.group(1).split(',')]
    fields['texelWorld'] = float(re.search(r'texelWorldFromSphere=([^ ]+)', line).group(1))
    samples[name] = fields
required = ('base', 'subtexel', 'large', 'return-subtexel')
assert all(k in samples for k in required), f'missing samples: {samples.keys()}'

def max_diff(a,b):
    return max(abs(x-y) for x,y in zip(samples[a]['matrix'], samples[b]['matrix']))

base, sub, large, back = (samples[k] for k in required)
small_delta = abs(sub['camera'][0] - base['camera'][0])
large_delta = abs(large['camera'][0] - sub['camera'][0])
half_texel = sub['texelWorld'] * 0.5
small_matrix_diff = max_diff('base','subtexel')
large_matrix_diff = max_diff('subtexel','large')
return_matrix_diff = max_diff('subtexel','return-subtexel')
assert small_delta < half_texel, (small_delta, half_texel)
assert large_delta > 64 * sub['texelWorld'], (large_delta, sub['texelWorld'])
assert small_matrix_diff <= 1e-7, small_matrix_diff
assert large_matrix_diff > 0.05, large_matrix_diff
assert return_matrix_diff <= 1e-7, return_matrix_diff
assert max(abs(base['sphere'][3] - s['sphere'][3]) for s in (sub,large,back)) < 1e-6
result = {
 'status':'passed', 'cascade':0, 'radius':sub['sphere'][3], 'shadowParams':sub['shadowParams'], 'splits':sub['splits'],
 'shadowMapPixels': round(1.0/sub['shadowParams'][3]), 'estimatedWorldTexel':sub['texelWorld'], 'halfTexel':half_texel,
 'smallWorldMove':small_delta, 'largeWorldMove':large_delta, 'largeMoveInEstimatedTexels':large_delta/sub['texelWorld'],
 'maxMatrixDifferenceBaseToSubtexel':small_matrix_diff, 'maxMatrixDifferenceSubToLarge':large_matrix_diff,
 'maxMatrixDifferenceSubToReturn':return_matrix_diff, 'samples':samples,
 'assertions':['subtexel move is below half estimated world texel and matrix is unchanged','large move exceeds 64 estimated texels and snapped matrix changes','returning to prior subtexel position restores matrix','cascade radius remains stable']
}
Path(sys.argv[2]).write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='samples'}, indent=2))

import json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
m=json.loads((R/'transparency-oracle-measurements.json').read_text())
checked=0
for mode,frames in m['modes'].items():
 for f in frames:
  assert f['samples']>=50 and f['wrongPixels']==0 and f['maxError']<=5
  raw=(R/f['image']).read_bytes();w,h=struct.unpack_from('<ii',raw,18)
  assert (w,abs(h))==tuple(f['resolution'])
  checked+=1
assert checked==6
print('PASS: all 6 Forward/Deferred crossing-plane oracle frames, sampled, within RGB tolerance')

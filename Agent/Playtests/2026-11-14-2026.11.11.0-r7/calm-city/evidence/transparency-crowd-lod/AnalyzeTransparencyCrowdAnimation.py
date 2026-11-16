import hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
rows=[]
for mode in (0,1):
 d=json.loads((R/f'transparency-crowd-animation-m{mode}-v2.json').read_text())
 assert d['ok'] and len(d['frames'])==6
 data=[]
 for i,f in enumerate(d['frames']):
  p=R/Path(f['image']).name; h=hashlib.sha256(p.read_bytes()).hexdigest(); levels=f['lod'][0]['levels']
  assert levels==[4,2,0],(mode,i,levels)
  w,height=struct.unpack_from('<ii',p.read_bytes(),18);assert (w,abs(height))==(320,240)
  data.append({'index':i,'time':[0,.25,.5,.75,1,0][i],'lodCounts':levels,'sha256':h,'image':p.name})
 assert data[0]['sha256']!=data[1]['sha256'] and data[0]['sha256']!=data[2]['sha256']
 assert data[0]['sha256']==data[4]['sha256']==data[5]['sha256'],(mode,'cycle did not return')
 rows.append({'rendering':mode,'frames':data})
out={'scenario':'VAT time A/B/return with fixed camera, viewport and LOD','results':rows}
(R/'transparency-crowd-animation-analysis.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))

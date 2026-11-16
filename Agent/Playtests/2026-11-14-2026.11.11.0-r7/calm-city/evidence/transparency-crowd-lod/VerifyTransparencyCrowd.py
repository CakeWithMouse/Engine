import hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
expected_dims=[(320,240),(480,320),(400,240)]
expected_counts=[[4,2,0],[0,4,0],[0,0,4]]
rows=[]
for mode in (0,1):
 data=json.loads((R/f'transparency-crowd-lod-m{mode}.json').read_text())
 assert data['ok'] and len(data['frames'])==3
 frames=[]
 for i,(f,dims,counts) in enumerate(zip(data['frames'],expected_dims,expected_counts)):
  name=Path(f['image']).name;image=R/name;raw=image.read_bytes()
  width,height=struct.unpack_from('<ii',raw,18)
  assert f['lod'][0]['levels']==counts and (width,abs(height))==dims
  frames.append({'frame':i,'lodCounts':counts,'dimensions':[width,abs(height)],'sha256':hashlib.sha256(raw).hexdigest(),'image':name})
 rows.append({'rendering':mode,'frames':frames})
for mode in (0,1):
 data=json.loads((R/f'transparency-crowd-animation-m{mode}-v2.json').read_text());frames=[]
 assert data['ok'] and len(data['frames'])==6
 for i,f in enumerate(data['frames']):
  name=Path(f['image']).name;raw=(R/name).read_bytes();lod=f['lod'][0]['levels']
  assert lod==[4,2,0]
  width,height=struct.unpack_from('<ii',raw,18);assert (width,abs(height))==(320,240)
  frames.append({'time':[0,.25,.5,.75,1,0][i],'lodCounts':lod,'dimensions':[width,abs(height)],'sha256':hashlib.sha256(raw).hexdigest(),'image':name})
 assert frames[0]['sha256']!=frames[1]['sha256'] and frames[0]['sha256']!=frames[2]['sha256']
 assert frames[0]['sha256']==frames[4]['sha256']==frames[5]['sha256']
 rows.append({'rendering':mode,'animationFrames':frames})
(R/'transparency-crowd-verified.json').write_text(json.dumps({'results':rows},indent=2)+'\n')
print('PASS: 2 render modes; resized 3-LOD sequence; animated VAT pixel A/B and exact cycle return')

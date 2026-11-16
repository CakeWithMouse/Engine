import hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parent
x=json.loads((R/'skeletal-animation.json').read_text()); assert x['ok'] and len(x['frames'])==5
states=[]
for t,f in zip([0,.25,.5,.75,1],x['frames']):
 assert f['count']==1 and f['slot']==0 and len(f['faces'])==6
 image=Path(f['image']).name;p=R/image;assert p.is_file() and p.stat().st_size>54
 states.append({'time':t,'faceHashes':[a['sha256'] for a in f['faces']],'occupiedTexels':[a['occupied'] for a in f['faces']],'imageSha256':hashlib.sha256(p.read_bytes()).hexdigest(),'image':image})
assert len({tuple(s['faceHashes']) for s in states})>=3
(R/'skeletal-animation-verified.json').write_text(json.dumps({'model':'CesiumMan.glb','states':states},indent=2)+'\n')
print('PASS: imported skeletal animation produced at least 3 point-shadow map states.')

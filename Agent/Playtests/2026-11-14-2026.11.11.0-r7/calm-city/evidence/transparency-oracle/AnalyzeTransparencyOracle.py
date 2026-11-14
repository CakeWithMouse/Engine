"""Independent pixel oracle for the Calm City translucent crossing-plane probe."""
import json, math, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parent
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def sub(a,b): return [x-y for x,y in zip(a,b)]
def cross(a,b): return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def unit(a):
 d=math.sqrt(dot(a,a)); return [x/d for x in a]
def bmp(path):
 b=Path(path).read_bytes(); off=struct.unpack_from('<I',b,10)[0]; w,h=struct.unpack_from('<ii',b,18); bits=struct.unpack_from('<H',b,28)[0]; stride=((w*bits+31)//32)*4
 def pixel(x,y):
  i=off+(h-1-y)*stride+x*(bits//8); return tuple(reversed(b[i:i+3]))
 return w,abs(h),pixel
def measure(mode):
 req=json.loads((ROOT/f'transparency-oracle-{mode}.request.json').read_text()); result=json.loads((ROOT/f'transparency-oracle-{mode}.json').read_text()); rows=[]
 assert result['ok'] and len(result['frames'])==len(req['frames'])
 for frame,out in zip(req['frames'],result['frames']):
  image=ROOT/Path(out['image']).name; w,h,pixel=bmp(image); eye=frame['camera']; target=frame['target']; forward=unit(sub(target,eye)); right=unit(cross([0,1,0],forward)); up=cross(forward,right); aspect=w/h; ortho_height=8; errors=[]; wrong=0
  for y in range(40,h-40,6):
   for x in range(40,w-40,6):
    sx=(2*(x+.5)/w-1)*ortho_height*aspect/2; sy=(1-2*(y+.5)/h)*ortho_height/2
    origin=[eye[k]+right[k]*sx+up[k]*sy for k in range(3)]; hits=[]; edge=False
    for material,yaw,color in [(0,-35,[1,.05,.05]),(1,35,[.05,.1,1])]:
     a=math.radians(yaw); tangent=[math.cos(a),0,-math.sin(a)]; normal=[math.sin(a),0,math.cos(a)]; denom=dot(forward,normal)
     if abs(denom)<1e-7: continue
     distance=-dot(origin,normal)/denom; point=[origin[k]+distance*forward[k] for k in range(3)]; u=dot(point,tangent); v=point[1]
     if distance<=0: continue
     if min(abs(abs(u)-2),abs(abs(v)-2))<.04: edge=True
     if abs(u)<2 and abs(v)<2: hits.append((distance,color,.5,material))
    if edge or len(hits)!=2 or abs(hits[0][0]-hits[1][0])<.003: continue
    expected=[0,0,.2]
    for _,color,alpha,_ in sorted(hits,reverse=True): expected=[color[k]*alpha+expected[k]*(1-alpha) for k in range(3)]
    expected=tuple(round(min(1,c)*255) for c in expected); actual=pixel(x,y); err=max(abs(a-b) for a,b in zip(expected,actual)); errors.append(err); wrong+=err>5
  assert len(errors)>=50, f'{image.name}: insufficient oracle samples={len(errors)}'
  assert wrong==0, f'{image.name}: {wrong}/{len(errors)} pixels differ by >5'
  rows.append({'image':image.name,'resolution':[w,h],'samples':len(errors),'wrongPixels':wrong,'maxError':max(errors),'meanError':round(sum(errors)/len(errors),2)})
 return rows
modes={name:measure(name) for name in ('forward','deferred')}
out={'expected':'Independent orthographic ray/plane intersections for two custom crossing translucent yaw planes. Pixel RGB expected from source-over alpha=.5 over sampled clear RGB=(0,0,51). Silhouette and near-equal depth samples excluded; tolerance <=5 RGB levels; requires >=50 valid pixels per frame.','modes':modes}
(ROOT/'transparency-oracle-measurements.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))

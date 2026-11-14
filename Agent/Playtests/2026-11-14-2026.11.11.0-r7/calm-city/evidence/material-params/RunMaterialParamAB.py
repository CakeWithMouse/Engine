"""Render a material file before, after a real parameter edit, then after revert."""
import json,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]; TOOL=Path(r'E:\Engine\Game\build\x64\Release\Runtime\EngineCommandlets.exe'); mat=R/'CalmCityUpgraded/Content/Materials/ParameterProbe.mat'; shader=mat.with_name('ParameterProbe.hlsl')
shader.write_text('cbuffer MaterialParams : register(b1) { float4 Tint = float4(1,0.1,0.05,1); };\nvoid Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo=Tint.rgb; o.Alpha=Tint.a; }\n')
base={'version':1,'shader':shader.name,'render':{'blend':'opaque','lighting':'unlit','castShadows':False,'twoSided':True}}
def run(label,color):
 doc=dict(base,params={'Tint':color});mat.write_text(json.dumps(doc,indent=2)+'\n'); req={'material':str(mat.resolve()),'shape':'cube','size':2,'camera':[5,4,7],'target':[0,0,0],'rendering':0,'width':320,'height':240,'frames':[{'image':str((R/'results'/f'material-param-{label}.bmp').resolve())}]}; rp=R/'requests'/f'material-param-{label}.json'; op=R/'results'/f'material-param-{label}.json'; log=R/'results'/f'material-param-{label}.log';rp.write_text(json.dumps(req,indent=2)+'\n')
 with log.open('w') as f: code=subprocess.run([str(TOOL),'render.mesh',str(rp),str(op)],stdout=f,stderr=subprocess.STDOUT,timeout=60).returncode
 data=json.loads(op.read_text()); assert code==0 and data['ok'],(label,code,data)
 image=Path(data['frames'][0]['image']); import hashlib
 return {'label':label,'tint':color,'image':image.name,'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'center':data['frames'][0]['center'],'exit':code}
red=[1,.1,.05,1]; blue=[.05,.2,1,1]
out={'steps':[run('before',red),run('edited',blue),run('reverted',red)]}
assert out['steps'][0]['sha256']!=out['steps'][1]['sha256'],'parameter edit did not change pixels'
assert out['steps'][0]['sha256']==out['steps'][2]['sha256'],'revert did not restore pixels'
(R/'results'/'material-param-analysis.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))

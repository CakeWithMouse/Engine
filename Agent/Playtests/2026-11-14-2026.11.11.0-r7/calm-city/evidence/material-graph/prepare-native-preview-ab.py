import json
from pathlib import Path
R=Path(r'E:\Engine\Engine\build\playtests\20261114-r7\calm-city')
materials=R/'CalmCityUpgraded'/'Content'/'Materials'
base=json.loads((materials/'ParameterProbe.mat.graph.json').read_text())
colors={'teal':[0.04,0.82,0.68,1.0],'amber':[1.0,0.36,0.035,1.0]}
for label,color in colors.items():
 name='NativePreview'+label.title()
 graph=json.loads(json.dumps(base))
 node=next(n for n in graph['nodes'] if n['type']=='ColorParam')
 node['properties']['name']=name
 node['properties']['color']=color
 node['title']=name
 (materials/f'{name}.mat.graph.json').write_text(json.dumps(graph,indent=2)+'\n')
 shader=f'''// Saved graph source for native preview A/B.\ncbuffer MaterialParams : register(b1)\n{{\n    float4 {name} = float4({color[0]:.4f}f, {color[1]:.4f}f, {color[2]:.4f}f, 1.0f);\n    float Gloss = 0.25f;\n}};\nvoid Surface(MaterialInput i, inout MaterialOutput o)\n{{\n    o.Albedo = saturate({name}.rgb);\n    o.SpecularWeight = saturate(Gloss);\n}}\n'''
 (materials/f'{name}.hlsl').write_text(shader)
 (materials/f'{name}.mat').write_text(json.dumps({'version':1,'shader':f'{name}.hlsl','render':{'blend':'opaque','lighting':'unlit','castShadows':False,'twoSided':True},'params':{name:color,'Gloss':0.25}},indent=2)+'\n')
for label in colors:
 name='NativePreview'+label.title();mat='/Game/Materials/'+name+'.mat'
 req={'project':str((R/'CalmCityUpgraded'/'Project.json').resolve()),'material':mat,'previewOnly':True,'image':str((R/'results'/f'{name}-form.png').resolve()),'steps':[{'action':'enable'},{'action':'apply'}]}
 (R/'requests'/f'{name}-preview.json').write_text(json.dumps(req,indent=2)+'\n')
print('Prepared Teal and Amber saved materials, graph sidecars, and previewOnly requests.')

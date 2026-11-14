import json,struct,hashlib
from pathlib import Path
R=Path(__file__).resolve().parent
def center(p):
 b=p.read_bytes();off=struct.unpack_from('<I',b,10)[0];w,h=struct.unpack_from('<ii',b,18);bits=struct.unpack_from('<H',b,28)[0];stride=((w*bits+31)//32)*4;i=off+(h-1-h//2)*stride+(w//2)*(bits//8);return list(reversed(b[i:i+3]))
rows=[]
for name,want in [('material-param-before.bmp',[255,25,13]),('material-param-edited.bmp',[13,51,255]),('material-param-reverted.bmp',[255,25,13])]:
 p=R/name;actual=center(p);assert actual==want,(name,actual,want);rows.append({'image':name,'center':actual,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
assert rows[0]['sha256']==rows[2]['sha256'] and rows[0]['sha256']!=rows[1]['sha256']
(R/'material-param-verified.json').write_text(json.dumps({'steps':rows},indent=2)+'\n')
print('PASS: material parameter edit changed GPU pixels; revert restored identical image hash.')

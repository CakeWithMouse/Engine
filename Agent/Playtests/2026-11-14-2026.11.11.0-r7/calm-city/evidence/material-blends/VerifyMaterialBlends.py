import hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
expected={'hole':[134,32,134],'covered':[134,134,19]}
def pixel(p,x,y):
 b=p.read_bytes();offset=struct.unpack_from('<I',b,10)[0];w,h=struct.unpack_from('<ii',b,18);bits=struct.unpack_from('<H',b,28)[0];stride=((w*bits+31)//32)*4;i=offset+(abs(h)-1-y)*stride+x*(bits//8);return list(reversed(b[i:i+3]))
out={'expected':expected,'modes':{}}
for mode in (0,1):
 name=f'material-blends-mode{mode}.bmp';p=R/name;assert p.is_file()
 a=pixel(p,120,120);b=pixel(p,200,120);assert a==expected['hole'] and b==expected['covered'],(mode,a,b)
 out['modes'][str(mode)]={'image':name,'maskedHole':a,'maskedCovered':b,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
(R/'material-blends-verified.json').write_text(json.dumps(out,indent=2)+'\n')
print('PASS: masked alpha and translucent source-over pixels match in Forward and Deferred.')

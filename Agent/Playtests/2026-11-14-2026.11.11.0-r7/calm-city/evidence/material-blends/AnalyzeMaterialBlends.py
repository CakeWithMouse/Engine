"""Pixel oracle for opaque, masked-alpha and translucent overlap of own planes."""
import json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
def sample(path,x,y):
 b=Path(path).read_bytes();off=struct.unpack_from('<I',b,10)[0];w,h=struct.unpack_from('<ii',b,18);bits=struct.unpack_from('<H',b,28)[0];stride=((w*bits+31)//32)*4;i=off+(h-1-y)*stride+x*(bits//8);return tuple(reversed(b[i:i+3]))
result={'expected':{'maskedHole':[134,32,134],'maskedCovered':[134,134,19]},'modes':{}}
for mode in (0,1):
 path=R/f'material-blends-mode{mode}.bmp'; hole=sample(path,120,120);covered=sample(path,200,120)
 assert max(abs(a-b) for a,b in zip(hole,result['expected']['maskedHole']))<=5,(mode,hole)
 assert max(abs(a-b) for a,b in zip(covered,result['expected']['maskedCovered']))<=5,(mode,covered)
 result['modes'][str(mode)]={'image':path.name,'maskedHoleActual':hole,'maskedHoleError':max(abs(a-b) for a,b in zip(hole,result['expected']['maskedHole'])),'maskedCoveredActual':covered,'maskedCoveredError':max(abs(a-b) for a,b in zip(covered,result['expected']['maskedCovered']))}
(R/'material-blends-analysis.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

import hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parent
expected={'Teal':(10,209,173),'Amber':(255,92,9)}
rows={}
def readjson(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def pixel(data,x,y):
 offset=struct.unpack_from('<I',data,10)[0];w,h=struct.unpack_from('<ii',data,18);bits=struct.unpack_from('<H',data,28)[0];stride=((w*bits+31)//32)*4;i=offset+(abs(h)-1-y)*stride+x*(bits//8);return tuple(reversed(data[i:i+3]))
for label,want in expected.items():
 result=readjson(R/f'NativePreview{label}-preview-v2.json')
 image=R/f'NativePreview{label}-native-v2.bmp';data=image.read_bytes();w,h=struct.unpack_from('<ii',data,18);size=result['nativePreviewSize']
 assert result['ok'] and result['projectViewportMinimized'] and result['nativePreviewWindow']
 assert [w,abs(h)]==size,(label,(w,h),size)
 center=pixel(data,w//2,abs(h)//2);assert center==want,(label,center,want)
 rows[label]={'nativePreviewSize':size,'bmpDimensions':[w,abs(h)],'centerRgb':list(center),'sha256':hashlib.sha256(data).hexdigest(),'image':image.name,'steps':len(result['steps'])}
assert rows['Teal']['sha256']!=rows['Amber']['sha256']
out={'expected':'Native D3D Material Editor preview captured in separate processes after graph enable/apply/save; project viewport minimized.','results':rows}
(R/'native-material-preview-ab-analysis.json').write_text(json.dumps(out,indent=2)+'\n')
print('PASS: native preview BMP dimensions match nativePreviewSize; Teal and Amber pixels differ.')

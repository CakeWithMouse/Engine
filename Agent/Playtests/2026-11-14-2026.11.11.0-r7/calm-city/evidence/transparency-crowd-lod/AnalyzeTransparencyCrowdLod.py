import json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parent
expected_dims=[(320,240),(480,320),(400,240)]
expected_counts=[[4,2,0],[0,4,0],[0,0,4]]
summary=[]
for mode in (0,1):
    data=json.loads((ROOT/f'transparency-crowd-lod-m{mode}.json').read_text())
    assert data['ok'] and len(data['frames'])==3
    frames=[]
    for i,(frame,dims,counts) in enumerate(zip(data['frames'],expected_dims,expected_counts)):
        levels=frame['lod'][0]['levels']
        assert levels==counts,(mode,i,levels,counts)
        img=Path(frame['image'])
        raw=img.read_bytes()
        assert raw[:2]==b'BM'
        width,height=struct.unpack_from('<ii',raw,18)
        assert (width,abs(height))==dims,(mode,i,width,height,dims)
        frames.append({'index':i,'lodCounts':levels,'dimensions':[width,abs(height)],'image':img.name,'bytes':len(raw)})
    summary.append({'rendering':mode,'frames':frames})
out={'scenario':'moving transparent crowd with three pixel-height LOD thresholds and live resize','expectedLodCounts':expected_counts,'results':summary}
(ROOT/'transparency-crowd-lod-analysis.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))

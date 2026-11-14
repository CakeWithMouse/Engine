import json, struct, sys
from pathlib import Path

def bmp(path):
    b=Path(path).read_bytes(); assert b[:2]==b'BM'
    off=struct.unpack_from('<I',b,10)[0]; w=struct.unpack_from('<i',b,18)[0]; h=struct.unpack_from('<i',b,22)[0]; bpp=struct.unpack_from('<H',b,28)[0]
    assert bpp==24 and w>0 and h>0, (path,w,h,bpp)
    stride=(w*3+3)&~3
    rows=[]
    for y in range(h):
        src=h-1-y; row=[]
        for x in range(w):
            p=off+src*stride+x*3; blue,green,red=b[p:p+3]; row.append((red,green,blue))
        rows.append(row)
    return w,h,rows

def stats(path):
    w,h,p=bmp(path); bg=p[0][0]
    pts=[(x,y) for y in range(h) for x in range(w) if p[y][x]!=bg]
    box=[min(x for x,y in pts),min(y for x,y in pts),max(x for x,y in pts)+1,max(y for x,y in pts)+1] if pts else None
    return {'width':w,'height':h,'backgroundRGB':bg,'nonBackgroundPixels':len(pts),'bbox':box,'centerRGB':p[h//2][w//2],'rows':p}
base=Path(sys.argv[1]); names=['near-640-t0.bmp','near-640-t06.bmp','near-1280-t06.bmp','far-640-t12.bmp','far-1280-t18.bmp']
s={n:stats(base/n) for n in names}
a,b=s[names[0]],s[names[1]]
changed=sum(1 for y in range(a['height']) for x in range(a['width']) if a['rows'][y][x]!=b['rows'][y][x])
# Observed render order: red front over blue rear, source-over on the near-black background.
bg=a['backgroundRGB']; expected=tuple(round(.5*front + .25*back + .25*bgc) for front,back,bgc in zip((255,0,0),(0,0,255),bg))
center=a['centerRGB']
result={'frames':{n:{k:v for k,v in x.items() if k!='rows'} for n,x in s.items()},'near640AnimationChangedPixels':changed,'near640T0CenterExpectedRedOverBlue':expected,'near640T0CenterActual':center,'orderColorErrorMax':max(abs(x-y) for x,y in zip(expected,center)),'visibleColorOrdering':'red front over blue rear; channels show red>blue' if center[0]>center[2] else 'blue over red or no dominant order'}
assert all(x['nonBackgroundPixels']>0 for x in s.values())
assert changed>0, 'same-size same-camera animation did not change pixels'
assert result['orderColorErrorMax']<=24, result
assert s[names[0]]['width']==640 and s[names[0]]['height']==480
assert s[names[2]]['width']==1280 and s[names[2]]['height']==720
assert s[names[3]]['width']==640 and s[names[4]]['width']==1280
Path(base/'image-analysis.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='frames'},indent=2));print(json.dumps(result['frames'],indent=2))

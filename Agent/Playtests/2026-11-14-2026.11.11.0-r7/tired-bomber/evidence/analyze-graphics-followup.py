from pathlib import Path
import struct, json
root=Path(__file__).resolve().parent

def read_bmp(name):
    b=(root/name).read_bytes()
    assert b[:2] == b'BM', f'{name}: invalid BMP signature'
    offset=struct.unpack_from('<I',b,10)[0]
    width,height=struct.unpack_from('<ii',b,18)
    bits=struct.unpack_from('<H',b,28)[0]
    assert bits==24, f'{name}: expected 24-bit BMP, got {bits}'
    stride=((width*3+3)//4)*4
    rows=[]
    for y in range(abs(height)):
        src_y=y if height<0 else abs(height)-1-y
        row=[]
        for x in range(width):
            p=offset+src_y*stride+x*3
            blue,green,red=b[p:p+3]
            row.append((red,green,blue))
        rows.append(row)
    return rows,width,abs(height)

def compare(a,b,box,threshold=6):
    ra,wa,ha=read_bmp(a); rb,wb,hb=read_bmp(b)
    assert (wa,ha)==(wb,hb)==(1000,500), f'{a}/{b}: unexpected size {(wa,ha)}/{(wb,hb)}'
    x0,y0,x1,y1=box; changed=0; total=0; points=[]
    for y in range(y0,y1):
        for x in range(x0,x1):
            aa,bb=ra[y][x],rb[y][x]
            ds=[abs(aa[i]-bb[i]) for i in range(3)]
            total+=sum(ds)
            if max(ds)>threshold:
                changed+=1; points.append((x,y))
    bbox=None
    if points: bbox=[min(p[0] for p in points),min(p[1] for p in points),max(p[0] for p in points),max(p[1] for p in points)]
    return {'a':a,'b':b,'box':box,'pixelsCompared':(x1-x0)*(y1-y0),'pixelsChangedAboveThreshold':changed,
            'meanAbsoluteChannelDifference':round(total/((x1-x0)*(y1-y0)*3),4),'changedBBox':bbox}

results={
 'skeletal':[
  compare('skeletal-followup-baseline-t0.bmp','skeletal-followup-pose-t025.bmp',(450,220,570,390)),
  compare('skeletal-followup-baseline-t0.bmp','skeletal-followup-pose-t050.bmp',(450,220,570,390)),
  compare('skeletal-followup-baseline-t0.bmp','skeletal-followup-reset-t0.bmp',(450,220,570,390))],
 'instances':[
  compare('instances-followup-baseline.bmp','instances-followup-moved-one.bmp',(275,80,560,440)),
  compare('instances-followup-moved-one.bmp','instances-followup-moved-and-hidden.bmp',(275,80,560,440)),
  compare('instances-followup-baseline.bmp','instances-followup-restored.bmp',(275,80,560,440))],
 'lod':[
  compare('lod-followup-far-y24.bmp','lod-followup-mid-y8.bmp',(275,80,560,440)),
  compare('lod-followup-far-y24.bmp','lod-followup-near-y3.bmp',(275,80,560,440)),
  compare('lod-followup-far-y24.bmp','lod-followup-return-y24.bmp',(275,80,560,440))]
}
assert results['skeletal'][0]['pixelsChangedAboveThreshold']>2000
assert results['skeletal'][1]['pixelsChangedAboveThreshold']>2000
assert results['skeletal'][2]['pixelsChangedAboveThreshold']==0
assert results['instances'][0]['pixelsChangedAboveThreshold']>100
assert results['instances'][1]['pixelsChangedAboveThreshold']>100
assert results['instances'][2]['pixelsChangedAboveThreshold']==0
assert results['lod'][0]['pixelsChangedAboveThreshold']>10000
assert results['lod'][1]['pixelsChangedAboveThreshold']>10000
assert results['lod'][2]['pixelsChangedAboveThreshold']==0
out=root/'graphics-followup-analysis.json'
out.write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print(json.dumps(results,indent=2))

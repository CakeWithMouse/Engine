"""Independent ray/plane oracle for intersecting transparency during camera motion."""
import argparse
import copy
import json
import math
from pathlib import Path
import subprocess
from TransparencyTests import image


def dot(a, b): return sum(x*y for x, y in zip(a, b))
def sub(a, b): return [x-y for x, y in zip(a, b)]
def cross(a, b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
def unit(a): return [x/math.sqrt(dot(a,a)) for x in a]


def suite(tool, output):
    output.mkdir(parents=True, exist_ok=False)
    (output/'Layer.hlsl').write_text('void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo=i.Color.rgb; o.Alpha=i.Color.a; }\n')
    (output/'Cross.hlsl').write_text('void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo=i.UV.x>1.5 ? float3(0,0,1) : float3(1,0,0); o.Alpha=i.Color.a; }\n')
    cross_material=output/'cross.mat'
    cross_material.write_text(json.dumps(dict(shader='Cross.hlsl',render=dict(blend='translucent',lighting='unlit',twoSided=True))))
    materials = {}
    for blend in ('translucent', 'additive', 'opaque'):
        p=output/(blend+'.mat')
        p.write_text(json.dumps(dict(shader='Layer.hlsl',render=dict(blend=blend,lighting='unlit',twoSided=True))))
        materials[blend]=str(p)
    cases=[]
    def run(name, groups, frames, mode):
        frames=copy.deepcopy(frames)
        for i,f in enumerate(frames): f['image']=str(output/f'{name}-{i:02}.bmp')
        request=dict(groups=groups,frames=frames,rendering=mode,width=320,height=256)
        src,dst=output/(name+'.json'),output/(name+'-result.json')
        src.write_text(json.dumps(request))
        with (output/(name+'.log')).open('w') as log:
            proc=subprocess.run([str(tool),'render.transparency',str(src),str(dst)],stdout=log,stderr=subprocess.STDOUT,timeout=240)
        data=json.loads(dst.read_text(encoding='utf-8-sig')); errors=[]; measurements=[]
        if proc.returncode or not data.get('ok'): errors.append(str(data))
        for f,actual in zip(frames,data.get('frames',[])):
            w,h,pixel=image(actual['image']); forward=unit(sub(f['target'],f['camera']))
            right=unit(cross([0,1,0],forward)); up=cross(forward,right)
            bad=total=0; max_error=0
            for y in range(h//2-45,h//2+46,5):
                for x in range(w//2-70,w//2+71,5):
                    sx=(2*(x+.5)/w-1)*w/h*math.tan(math.pi/6)
                    sy=(1-2*(y+.5)/h)*math.tan(math.pi/6)
                    ray=[forward[k]+sx*right[k]+sy*up[k] for k in range(3)]
                    hits=[]; edge=False
                    for g in groups:
                        oracle_layers=[]
                        for original in g['layers']:
                            if g.get('geometry')=='cross':
                                for angle,color in ((-45,[1,0,0]),(45,[0,0,1])):
                                    l=copy.deepcopy(original);l['animationYaw']=l['rotation'][1];l['rotation'][1]+=angle;l['color']=color;oracle_layers.append(l)
                            else: oracle_layers.append(original)
                        for l in oracle_layers:
                            a=math.radians(l['rotation'][1]); tangent=[math.cos(a),0,-math.sin(a)]; normal=[math.sin(a),0,math.cos(a)]
                            center=list(l['position'])
                            if g['kind'] in ('skeletal','crowd'):
                                animation_angle=math.radians(l.get('animationYaw',l['rotation'][1]))
                                phase=f.get('time',0)%1;offset=.6*(phase if phase<=.5 else 1-phase)
                                animation=[math.cos(animation_angle),0,-math.sin(animation_angle)]
                                center=[center[k]+offset*animation[k] for k in range(3)]
                            denom=dot(ray,normal)
                            if abs(denom)<1e-7: continue
                            distance=dot(sub(center,f['camera']),normal)/denom
                            local=sub([f['camera'][k]+distance*ray[k] for k in range(3)],center)
                            u,v=dot(local,tangent),local[1]
                            if min(abs(abs(u)-g['width']/2),abs(abs(v)-g['height']/2))<.04: edge=True
                            if distance>0 and abs(u)<g['width']/2 and abs(v)<g['height']/2:
                                hits.append((distance,l['color'],l['alpha'],Path(g['material']).stem))
                    if edge or any(abs(a[0]-b[0])<.003 for j,a in enumerate(hits) for b in hits[j+1:]): continue
                    expected=[0,0,.2]
                    for _,c,a,blend in sorted(hits,reverse=True):
                        if blend=='opaque': expected=list(c)
                        else: expected=[v*a+old*(1-a if blend!='additive' else 1) for v,old in zip(c,expected)]
                    expected=[min(1,v) for v in expected]
                    error=max(abs(round(v*255)-p) for v,p in zip(expected,pixel(x,y)))
                    max_error=max(max_error,error); total+=1; bad+=error>4
            measurements.append(dict(image=actual['image'],samples=total,wrongPixels=bad,maxError=max_error,drawCalls=actual['drawCalls'],lod=actual['lod']))
            if bad: errors.append(f'{Path(actual["image"]).name}: {bad}/{total} wrong pixels, max error {max_error}')
            if total<100: errors.append('insufficient ray/plane samples')
        if len(data.get('frames',[]))!=len(frames): errors.append('missing frames')
        cases.append(dict(name=name,frames=len(data.get('frames',[])),errors=errors,measurements=measurements,adapter=data.get('adapter')))
        print(name+': '+('PASS' if not errors else '; '.join(errors[:2])),flush=True)

    def layer(yaw=0,color=(1,0,0),z=0,alpha=.5):
        return dict(position=[0,0,z],rotation=[0,yaw,0],color=[float(c) for c in color],alpha=alpha)
    def group(kind,layers,blend='translucent',**kw):
        return dict(kind=kind,width=6,height=4,layers=layers,material=materials[blend],**kw)
    def orbit(angles,animated=False):
        frames=[]
        for i,angle in enumerate(angles):
            a=math.radians(angle)
            frames.append(dict(camera=[8*math.sin(a),0,-8*math.cos(a)],target=[0,0,0],time=(i%5)*.2 if animated else .25))
        return frames
    angles=(-60,-30,-5,0,5,30,60,120,175,180,185,240,300,0)
    for mode in (0,1):
        for kind in ('mesh','copies','instances','skeletal','crowd'):
            for reverse in (False,True):
                layers=[layer(-45),layer(45,(0,0,1)),layer(0,(0,1,0))]
                if reverse: layers.reverse()
                g=group(kind,layers)
                if kind in ('instances','crowd'): g.update(lods=2,lodScreenHeights=[170],fade=.25)
                run(f'{mode}-{kind}-'+('reverse' if reverse else 'normal'),[g],orbit(angles),mode)
            g=group(kind,[layer(color=(1,1,1))],geometry='cross');g['material']=str(cross_material)
            if kind in ('instances','crowd'):g.update(lods=2,lodScreenHeights=[170],fade=.25)
            run(f'{mode}-{kind}-self-cross',[g],orbit(angles,True),mode)
            if kind in ('instances','crowd'):
                frames=[dict(camera=[0,0,-z],target=[0,0,0],time=i*.15) for i,z in enumerate((6,8,12,16,12,8,6))]
                run(f'{mode}-{kind}-cross-lod-dolly',[g],frames,mode)
        base=[group('mesh',[layer(-45)]),group('instances',[layer(45,(0,0,1))],lods=2,lodDistances=[8])]
        run(f'{mode}-mixed-additive',base+[group('crowd',[layer(0,(0,1,0))],'additive')],orbit(angles,True),mode)
        run(f'{mode}-opaque-occlusion',base+[group('mesh',[layer(10,(.3,.3,.3))],'opaque')],orbit(angles),mode)
        run(f'{mode}-zero-alpha',base+[group('mesh',[layer(0,(0,1,0),alpha=0)])],orbit(angles),mode)
        resize=orbit((0,15,180,0))
        for f,(w,h) in zip(resize,((320,256),(640,360),(192,320),(320,256))):f.update(width=w,height=h)
        run(f'{mode}-resize',base,resize,mode)
        # At the exact-layer budget use low alpha, so a missing/reordered deep layer is visible.
        layers=[layer(-35+70*i/15,((i%3)==0,(i%3)==1,(i%3)==2),z=(i-7.5)*.025,alpha=.08) for i in range(16)]
        run(f'{mode}-sixteen-depths',[group('mesh',layers)],orbit((0,30,180,210)),mode)
        # Tail must remain present rather than silently dropping layers beyond the peel budget.
        layers=[layer(0,((i%3)==0,(i%3)==1,(i%3)==2),z=i*.08,alpha=.08) for i in range(24)]
        run(f'{mode}-deep-parallel-tail',[group('instances',layers)],orbit((0,30,180,210)),mode)
    summary=dict(passed=sum(not c['errors'] for c in cases),frames=sum(c['frames'] for c in cases),cases=cases)
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    return summary['passed']==len(cases)


if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--tool',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    args=p.parse_args(); raise SystemExit(0 if suite(args.tool.resolve(),args.output.resolve()) else 1)

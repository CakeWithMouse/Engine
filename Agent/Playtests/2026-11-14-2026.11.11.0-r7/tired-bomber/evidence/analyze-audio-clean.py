import json, math, struct, wave
from pathlib import Path
root=Path(r'Engine/build/playtests/20261114-r7/tired-bomber')
for tag in ('play3d-near','play3d-far','stream'):
    wav=root/f'audio-{tag}-clean.wav'; summary=root/f'audio-{tag}-clean.capture.json'
    with wave.open(str(wav),'rb') as w: ch,width,rate,n=w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes();raw=w.readframes(n)
    assert(ch,width,rate)==(1,2,48000)
    data=[x[0] for x in struct.iter_unpack('<h',raw)];block=4800;rows=[]
    for i in range(0,len(data)-block+1,block):
        c=s=power=0.0
        for n,x in enumerate(data[i:i+block]):
            ph=2*math.pi*997*n/rate;c+=x*math.cos(ph);s+=x*math.sin(ph);power+=x*x
        rows.append({'index':i//block,'start':round(i/rate,1),'rms':math.sqrt(power/block)/32768,'corr':2*math.hypot(c,s)/block/32768})
    base=[x for x in rows if x['start']<2.0]; active=[x for x in rows if 2.3<=x['start']<8 and x['corr']>0.01];tail=[x for x in rows if x['start']>=9]
    out={'tag':tag,'device':json.loads(summary.read_text())['deviceName'],'sample_rate':rate,'channels':ch,'bits':16,'window_count':len(rows),'baseline_max_corr':max(x['corr'] for x in base),'active_window_count':len(active),'active_mean_corr':sum(x['corr'] for x in active)/len(active) if active else 0,'active_mean_rms':sum(x['rms'] for x in active)/len(active) if active else 0,'tail_max_rms':max(x['rms'] for x in tail),'active_windows':active}
    (root/f'audio-{tag}-clean.analysis.json').write_text(json.dumps(out,indent=2));print(tag, 'corr',round(out['active_mean_corr'],5),'rms',round(out['active_mean_rms'],5),'count',out['active_window_count'],'tail',round(out['tail_max_rms'],5))

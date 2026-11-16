import json,math,struct,wave
from pathlib import Path
root=Path(r'Engine/build/playtests/20261114-r7/tired-bomber')
for trial in (1,2):
 for mode in ('play3d-near','play3d-far'):
  tag=f'{mode}-postfix-{trial}'
  with wave.open(str(root/f'audio-{tag}.wav'),'rb') as w: rate=w.getframerate(); data=[x[0] for x in struct.iter_unpack('<h',w.readframes(w.getnframes()))]
  rows=[]
  for i in range(0,len(data)-4799,4800):
   c=s=p=0.
   for n,x in enumerate(data[i:i+4800]):
    ph=2*math.pi*997*n/rate;c+=x*math.cos(ph);s+=x*math.sin(ph);p+=x*x
   rows.append({'start':round(i/rate,1),'rms':math.sqrt(p/4800)/32768,'corr':2*math.hypot(c,s)/4800/32768})
  base=[x['corr'] for x in rows if x['start']<2]; active=[x for x in rows if 2<=x['start']<8 and x['corr']>.01]; tail=[x['rms'] for x in rows if x['start']>=9]
  out={'tag':tag,'baseline_max_corr':max(base),'active_windows':len(active),'active_mean_corr':sum(x['corr'] for x in active)/len(active) if active else 0,'active_mean_rms':sum(x['rms'] for x in active)/len(active) if active else 0,'active_peak_corr':max((x['corr'] for x in active),default=0),'tail_max_rms':max(tail)}
  (root/f'audio-{tag}.analysis.json').write_text(json.dumps(out,indent=2));print(tag,'baseline',out['baseline_max_corr'],'active',out['active_windows'],'mean_corr',round(out['active_mean_corr'],5),'rms',round(out['active_mean_rms'],5),'peak',round(out['active_peak_corr'],5),'tail',out['tail_max_rms'])

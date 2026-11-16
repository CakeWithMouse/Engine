import wave,struct,math,json
from pathlib import Path
for tag in ['play3d-near','play3d-far','play3d-far2000']:
 p=Path(fr'Engine/build/playtests/20261114-r7/tired-bomber/audio-{tag}-clean.wav')
 with wave.open(str(p),'rb') as w: d=[x[0] for x in struct.iter_unpack('<h',w.readframes(w.getnframes()))];sr=w.getframerate()
 vals=[]
 for i in range(0,len(d)-4800+1,4800):
  x=d[i:i+4800];c=s=0
  for n,v in enumerate(x):
   ph=2*math.pi*997*n/sr;c+=v*math.cos(ph);s+=v*math.sin(ph)
  corr=2*math.hypot(c,s)/4800/32768; rms=math.sqrt(sum(v*v for v in x)/4800)/32768
  if i/48000>=2.3 and i/48000<8: vals.append((round(i/48000,1),round(corr,4),round(rms,4)))
 print(tag,vals[:8], '...', vals[-4:])

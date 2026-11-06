import json,csv
from pathlib import Path
root=Path(__file__).parent
repo=root.parents[2]
build=repo/'Engine/build/playtests/20261104-r1'
inventory=json.loads((root/'feature-inventory.json').read_text(encoding='utf-8-sig'))
records=[]
def add(role,feature,status,action,actual,evidence):
 status={'passed-with-bug':'partial','blocked-ui-pending':'not-tested','procedure-error-recovered':'partial','probable-unconfirmed':'partial'}.get(status,status)
 if isinstance(evidence,str):evidence=[evidence]
 records.append(dict(role=role,feature=feature,status=status,action=action,actual=actual,evidence=evidence or []))
for role,directory in enumerate(['calm-city','curious-city','tired-bomber','ai-trash'],1):
 data=json.loads((root/directory/'evidence.json').read_text(encoding='utf-8-sig'))
 entries=data if isinstance(data,list) else data.get('features',data.get('evidence',[]))
 for e in entries:
  fid=e.get('feature_id',e.get('feature'))
  if fid:add(role,fid,e.get('status','not-tested'),e.get('action',''),e.get('actual',e.get('fact','')),e.get('evidence',e.get('log',[])))
for e in json.loads((root/'ui-evidence.json').read_text(encoding='utf-8-sig')):
 add(e['role'],e['feature'],e['status'],e['action'],e['actual'],e.get('evidence',[]))
extra=root/'coordinator-evidence.json'
if extra.exists():records+=json.loads(extra.read_text(encoding='utf-8-sig'))
for role,log in [(1,'calm-city/authoring-probe-02.log'),(4,'ai-trash/authoring-probe.log')]:
 for line in (build/log).read_text(encoding='utf-8-sig',errors='replace').splitlines():
  if not line.startswith('{'):continue
  e=json.loads(line)
  if 'feature' not in e:continue
  status=e['status']
  if 'Assimp' in e.get('detail',''):status='blocked'
  if e['feature']=='material-editor':status='partial'
  add(role,e['feature'],status,e['action'],e.get('detail','')+'; public authoring API, no visual assertion',str(build/log))
rank={'passed':5,'passed-after-command-correction':5,'partial':4,'failed':3,'blocked':2,'blocked-environment':2,'not-tested':0}
out=[]
for feature in inventory:
 row={'feature_id':feature['id'],'name':feature['name']}
 attempted=successful=0
 for role in range(1,5):
  entries=[r for r in records if r['role']==role and r['feature']==feature['id']]
  active=[e for e in entries if rank.get(e['status'],0)>0]
  status=max(active,key=lambda e:rank.get(e['status'],0))['status'] if active else 'not-tested'
  if status=='blocked-environment':status='blocked'
  attempted+=bool(active);successful+=status=='passed'
  row[f'developer_{role}']=status
  row[f'evidence_{role}']=' | '.join(dict.fromkeys(str(p) for e in active for p in e.get('evidence',[])))
 row['attempted_developers']=attempted;row['successful_developers']=successful
 row['coverage']='two-attempts' if attempted>=2 else 'gap'
 out.append(row)
with (root/'feature-matrix.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(out[0]));w.writeheader();w.writerows(out)
(root/'all-evidence.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Features',len(out),'two attempts',sum(r['attempted_developers']>=2 for r in out),'two passed',sum(r['successful_developers']>=2 for r in out))
for r in out:
 if r['attempted_developers']<2:print(r['feature_id'],*[r[f'developer_{i}'] for i in range(1,5)])

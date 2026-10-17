import json,sys,collections
rows=[]
for l in open(sys.argv[1],errors='replace'):
    l=l.strip()
    if not l.startswith('{'): continue
    try: r=json.loads(l)
    except Exception: continue
    rows.append(r)
pids=collections.Counter(r.get('pid') for r in rows)
print('pids',pids.most_common(10))
R=[r for r in rows if r.get('pid')==113]
print('pid113 rows',len(R),'t_ms',R[0]['t_ms'],R[-1]['t_ms'],'seq',R[0]['seq'],R[-1]['seq'])
c=collections.Counter(r['tag'] for r in R)
for k,v in c.most_common(80): print(v,k)
json.dump(R,open(sys.argv[2],'w'))

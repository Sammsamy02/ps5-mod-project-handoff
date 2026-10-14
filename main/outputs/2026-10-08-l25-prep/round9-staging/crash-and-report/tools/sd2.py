import json,sys,re
R=json.load(open(sys.argv[1]))
log=open(sys.argv[2],errors='replace').read().split('\n')
dl=[(i+1,re.match(r'\[2026-10-12 (\S+)\]',l).group(1),int(re.search(r'depth=(\d+)',l).group(1))) for i,l in enumerate(log) if 'queued depth=' in l]
de=[r for r in R if r['tag']=='eigt_queue_depth']
print(len(dl),len(de))
def s(t):h,m,x=map(int,t.split(':'));return h*3600+m*60+x
offs=[]
for (ln,t,v),r in zip(dl,de):
    if v!=r['value']: print('MISMATCH',ln,t,v,r['value'])
    offs.append(s(t)-r['t_ms']/1000.0)
print('offset (wallsec - t_ms/1000) min %.3f max %.3f'%(min(offs),max(offs)))
off=max(offs)  # wall >= t; floor
def w(tms):
    x=tms/1000.0+off; h=int(x//3600); m=int(x//60%60); sec=x%60
    return '%02d:%02d:%06.3f'%(h,m,sec)
P=[r for r in R if r['tag']=='r3_marshal_pumped']
print('pumped events',len(P),'first',w(P[0]['t_ms']),'last',w(P[-1]['t_ms']))
gaps=[(P[i]['value']-P[i-1]['value'],P[i-1],P[i]) for i in range(1,len(P))]
import collections
h=collections.Counter(min(g//100*100,5000) for g,_,_ in gaps)
print('gap hist (ms bucket):',sorted(h.items()))
for g,a,b in gaps:
    if g>1300: print('GAP %d ms  %s -> %s'%(g,w(a['t_ms']),w(b['t_ms'])))
# pad edges
for r in R:
    if r['tag'] in ('pad_key_edge','pad_l3r3_f7_dispatched','pad_l3r3_edge','pad_retry','pad_retry_failed','l25_dl_first_notify_shutdown','pad_open_class'):
        print(w(r['t_ms']),r['tag'],r['value'])
print('last event',w(R[-1]['t_ms']),R[-1]['tag'])

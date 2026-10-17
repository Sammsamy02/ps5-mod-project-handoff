import re,sys,collections
L=open(sys.argv[1],encoding='utf-8',errors='replace').read().split('\n')
cats=collections.OrderedDict()
def cat(m):
    if 'ExecuteInGameThread queued method' in m: return 'q'
    if 'ExecuteInGameThread drained' in m: return 'd'
    if 'ExecuteInGameThread callback returned' in m: return 'r'
    if 'queued depth' in m: return 'depth'
    if 'keybind callback entry' in m: return 'kb'
    if m.startswith('Error: std::ba'): return 'ERR'
    if '[MEM]' in m: return 'mem'
    if 'widget/object' in m: return 'w'
    if '[zz] T' in m: return 'zzT'
    return None
per=collections.defaultdict(collections.Counter)
depth={}
states=collections.defaultdict(collections.Counter)
for i,l in enumerate(L,1):
    mm=re.match(r'\[2026-10-12 (\d\d:\d\d:\d\d)\] (.*)',l)
    if not mm: continue
    t,m=mm.groups()
    c=cat(m)
    if c: per[t][c]+=1
    dm=re.search(r'queued depth=(\d+)',m)
    if dm: depth.setdefault(t,[]).append(int(dm.group(1)))
    sm=re.search(r'ExecuteInGameThread (queued method=\d+|drained|callback returned) state=(0x[0-9a-f]+)',m)
    if sm: states[sm.group(2)][sm.group(1).split()[0]]+=1
# fill seconds
def s2(t):h,mi,se=map(int,t.split(':'));return h*3600+mi*60+se
ts=sorted(per,key=s2)
a,b=s2(ts[0]),s2(ts[-1])
print('sec q d r depth kb ERR mem w')
for x in range(a,b+1):
    t='%02d:%02d:%02d'%(x//3600,x//60%60,x%60)
    c=per.get(t,collections.Counter())
    print(t,c['q'],c['d'],c['r'],depth.get(t,''),c['kb'],c['ERR'],c['mem'],c['w'])
print(dict(states))

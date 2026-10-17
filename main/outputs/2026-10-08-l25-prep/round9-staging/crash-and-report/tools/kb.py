import json,sys,re,collections
R=json.load(open(sys.argv[1])); log=open(sys.argv[2],errors='replace').read().split('\n')
def s(t):h,m,x=map(int,t.split(':'));return h*3600+m*60+x
ents=[]
for i,l in enumerate(log,1):
    m=re.match(r'\[2026-10-12 (\S+)\] \[l22\] keybind callback entry key=(\d+)',l)
    if m: ents.append([i,s(m.group(1)),int(m.group(2)),False])
E=[r for r in R if r['tag']=='pad_key_edge']
for off in (23754.66,23755.65):
    diffs=collections.Counter(); unmatched=0
    used=set()
    for r in E:
        tw=r['t_ms']/1000.0+off
        for e in ents:
            if e[2]==r['value'] and e[0] not in used and e[1]>=int(tw)-1:
                used.add(e[0]); diffs[e[1]-int(tw)]+=1
                # F7 has two entries
                if r['value']==118:
                    for e2 in ents:
                        if e2[2]==118 and e2[0] not in used and e2[0]>e[0]:
                            used.add(e2[0]); break
                break
        else: unmatched+=1
    print('offset',off,'entry_sec - edge_sec histogram',dict(diffs),'unmatched',unmatched,'entries used',len(used),'of',len(ents))

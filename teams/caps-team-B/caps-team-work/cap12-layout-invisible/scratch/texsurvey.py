import sys, os, struct, collections, json
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
roots=sys.argv[1:]
seen={}
def prof(d):
    try: z=uf.parse_zen_package(d)
    except Exception as ex: return None
    pfs=[x for x in z.names if x.startswith('PF_')]
    if not pfs: return None
    rows=[]
    for e in z.exports:
        lo=e.get('chunk_offset',0); hi=min(lo+e['serial_size'],len(d))
        x=d[lo:hi]
        for pf in pfs:
            needle=struct.pack('<i',len(pf)+1)+pf.encode()+b'\0'
            at=x.find(needle)
            if at<12: continue
            sx,sy,packed=struct.unpack_from('<iiI',x,at-12)
            q=at+len(needle)
            opt=bool(packed&(1<<30)); ext=nt=None
            if opt: ext,nt=struct.unpack_from('<II',x,q); q+=8
            fm,nm=struct.unpack_from('<ii',x,q) if q+8<=len(x) else (None,None)
            rows.append(dict(pf=pf,size=(sx,sy),packed=hex(packed),opt=opt,ext=ext,tail=nt,first=fm,nmips=nm,cpu=bool(packed&(1<<29))))
    return z.name,rows,[b[2] for b in z.bulk_data],[b[3] for b in z.bulk_data]
for root in roots:
    for dp,dn,fn in os.walk(root):
        for f in fn:
            if not f.endswith('.bin'): continue
            p=os.path.join(dp,f)
            if os.path.getsize(p)<64: continue
            d=open(p,'rb').read()
            r=prof(d)
            if r and r[0] not in seen: seen[r[0]]=(p,)+r
# pakchunk10 full
M='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013/'
t=uf.parse_toc(open(M+'pakchunk10-ps5.utoc','rb').read()); rd=uf.ChunkReader(t,M+'pakchunk10-ps5.ucas')
for row,cid in enumerate(t.chunk_ids):
    if cid[11]==1:
        r=prof(rd.read_row(row))
        if r and r[0] not in seen: seen[r[0]]=('pakchunk10 row %d'%row,)+r
c=collections.Counter()
for k,(p,name,rows,bs,bf) in sorted(seen.items()):
    for rr in rows:
        c[(rr['pf'],rr['opt'],rr['ext'],rr['tail'],rr['cpu'])]+=1
        print(name, rr, 'bulk', bs[:14])
print('TOTAL textures', len(seen))
for k,v in sorted(c.items(), key=lambda kv:-kv[1]): print(v,k)

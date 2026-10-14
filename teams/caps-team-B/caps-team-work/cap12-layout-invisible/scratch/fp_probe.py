import sys, collections
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
U='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'
t=uf.parse_toc(open(U,'rb').read())
print(t.header, t.mount_point, len(t.files))
low={p.lower():r for r,p in t.files.items()}
n=0; rows=[]
for p,r in low.items():
    nm=p.rsplit('/',1)[-1]
    if not (p.endswith('.uasset') and nm.startswith('t_')): continue
    b=low.get(p[:-7]+'.ubulk')
    rows.append((p, r, t.lengths[r], t.lengths[b] if b is not None else None))
print(len(rows))
small=[x for x in rows if x[3] is None and x[2]<61440]
print('no-bulk <60KiB', len(small), sorted(x[2] for x in small)[:20], sorted(x[2] for x in small)[-10:])
c=collections.Counter(x[3] for x in rows if x[3] and 61440<=x[2]<71680)
for k,v in c.most_common(25): print(k,v)

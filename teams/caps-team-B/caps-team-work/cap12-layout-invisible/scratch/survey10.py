import sys, struct, collections
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
M='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013/'
t=uf.parse_toc(open(M+'pakchunk10-ps5.utoc','rb').read())
r=uf.ChunkReader(t,M+'pakchunk10-ps5.ucas')
types=collections.Counter(c[11] for c in t.chunk_ids)
print('types',types, 'mount', t.mount_point)
n=0
for row,cid in enumerate(t.chunk_ids):
    if cid[11]!=1: continue
    d=r.read_row(row)
    try: z=uf.parse_zen_package(d)
    except Exception as ex: continue
    pfs=[x for x in z.names if x.startswith('PF_')]
    if pfs:
        n+=1
        if n<=400: print(row, z.name, pfs, len(d), [b[2] for b in z.bulk_data], [hex(b[3]) for b in z.bulk_data])
print('textures',n)

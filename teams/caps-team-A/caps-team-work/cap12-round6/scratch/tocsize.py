import sys,os
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools')
import utoc, cityhash
W='/WORKSPACE/.codex/worktrees/translation-core/what'
M=W+'/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013'
print(os.listdir(M))
for c in sorted(f for f in os.listdir(M) if f.endswith('.utoc')):
    t=utoc.load(M+'/'+c)
    ids={bytes(x[:8]):i for i,x in enumerate(t.chunk_ids) if t.chunk_type(x)==1}
    for n in sys.argv[1:]:
        h=cityhash.ue_name_hash(n).to_bytes(8,'little')
        if h in ids:
            i=ids[h]; print(c, n, 'export chunk len', t.offlen[i][1])

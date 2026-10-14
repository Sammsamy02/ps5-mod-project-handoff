import sys, os, json
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools'
sys.path.insert(0,T)
import utoc, zen
M='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P'
toc=utoc.load(M+'.utoc')
uc=open(M+'.ucas','rb')
def rd(o,n): uc.seek(o); return uc.read(n)
EV='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/'
so=zen.ScriptObjects(open(EV+'console-pulls-20261013-paks2/global.utoc','rb').read() and b'') if False else None
print(toc.entry_count, 'entries')
from collections import Counter
c=Counter(toc.chunk_type(x) for x in toc.chunk_ids); print({utoc.TYPE_NAMES[k]:v for k,v in c.items()})
for row in toc.rows_of_type(1):
    path=toc.files.get(row)
    d=toc.read_chunk(row, rd)
    z=zen.ZenPackage(d)
    ex=[(e['name'], e['serial_size'], e['class']) for e in z.exports if e['outer'][0]=='Null']
    print(path, len(d), [(n,s) for n,s,c in ex][:4], len(z.exports))

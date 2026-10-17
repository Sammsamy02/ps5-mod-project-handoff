import sys,os,time
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pk import toc, rd, bypath
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools')
import mesh_slot_repoint as msr, mesh_sections as ms
H='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/headers-r2/ad7b3742feeb3d06.hdr.bin'
d=toc.read_chunk(bypath['Townfall/Characters/Simon/Meshes/SK_Simon_FirstStage'], rd)
t=time.time()
out,rec=msr.repoint(d, range(8), '/Game/MetaHumans/Common/Materials/M_Hide', 'M_Hide', open(H,'rb').read())
print('ok %.1fs'%(time.time()-t), {k:v for k,v in rec.items() if k not in ('untouched_slots',)})
m=ms.decode(out)
print([ (s['index'], (s['material'] or '').split('/')[-1]) for s in m['slots']][:10])
try:
    msr.repoint(d, [0], '/Game/MetaHumans/Common/Materials/M_Hide', 'M_Hide', open(H,'rb').read()); print('single ok')
except msr.RepointError as e: print('refused', e)

import sys,os,time,json
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pk import toc, rd, bypath
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/PATCH/tools')
import mesh_sections as ms
for n in sys.argv[1:]:
    t=time.time()
    m=ms.decode(toc.read_chunk(bypath[n], rd))
    print('==',n, 'slots',len(m['slots']),'lods',len(m['lods']),'t=%.1fs'%(time.time()-t))
    for s in m['slots']: print('   slot',s['index'],s['slot_name'],(s['material'] or '-').split('/')[-1],[round(x,1) for x in s['uv_densities']])
    for l in m['lods']:
        for r in l['sections']:
            ph,why=ms.section_placeholder(m,l,r)
            print('   L%d s%d'%(l['lod'],r['section']), r['slot_name'], r['triangles'], 'area %.4f'%r['area_cm2'], 'ext %.2f'%r['extent_cm'], 'enc',r.get('enclosed_rays'), 'bones',r['bones'][:3], 'PH' if ph else '', why[:80])

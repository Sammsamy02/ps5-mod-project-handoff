import sys,os,collections,json
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools')
import cityhash, container_header, asset_registry
W='/WORKSPACE/.codex/worktrees/translation-core/what'
F=W+'/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013'
X=W+'/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
store={}
for c in ('0','1','10','11','12'):
    store.update(container_header.parse(open(F+'/manifests/pakchunk%s-ps5.container-header.bin'%c,'rb').read())[1])
reg=asset_registry.load(X+'/AssetRegistry.bin')
by_id={cityhash.ue_name_hash(a['package']):a for a in reg.assets}
imp=collections.defaultdict(set); impby=collections.defaultdict(set)
for q,v in store.items():
    qa=by_id.get(q)
    if not qa: continue
    for i in v['imports']:
        ia=by_id.get(i)
        if ia: imp[qa['package']].add(ia['package']); impby[ia['package']].add(qa['package'])
def cls(p):
    a=reg.asset(p); return a['class'].rsplit('.',1)[-1] if a else '?'
if __name__=='__main__':
    for q in sys.argv[1:]:
        hits=[a['package'] for a in reg.assets if q.lower() in a['package'].lower()]
        for p in hits[:40]:
            print('==',p,cls(p))
            print('   imports:',sorted((x.split('/')[-1],cls(x)) for x in imp.get(p,())))
            print('   imported by:',sorted((x.split('/')[-1],cls(x)) for x in impby.get(p,())))

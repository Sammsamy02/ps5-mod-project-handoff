import sys, collections, re
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools')
import asset_registry as ar
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
r=ar.load(X+'/AssetRegistry.bin')
print('assets', len(r.assets))
cls=collections.Counter(a['class'] for a in r.assets)
print([c for c in cls.most_common(40)])
pat=re.compile(r'(invis|hide|hidden|transparen|empty|null|nothing|clear|cutout|blank)', re.I)
for a in r.assets:
    if 'Material' in a['class'] and pat.search(a['package'].rsplit('/',1)[-1]):
        print(a['class'], a['package'], {k:v for k,v in a['tags'].items() if k in ('BlendMode','ShadingModel','ShadingModels','MaterialDomain','Parent','bHasStaticPermutationResource','TwoSided')})
mat=[a for a in r.assets if a['class']=='/Script/Engine.Material']
print('materials', len(mat)); print(sorted(set(k for a in mat for k in a['tags'])))

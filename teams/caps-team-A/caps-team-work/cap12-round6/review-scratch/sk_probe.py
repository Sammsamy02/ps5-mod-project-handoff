import sys, collections
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
for x in registry.assets:
    n = x['package'].rsplit('/', 1)[-1]
    if n in ('SK_SimonGlasses', 'SK_TheWall_SimonHead', 'SK_Canula'):
        t = x.get('tags') or {}
        print(n, x['class'], {k: v for k, v in t.items() if 'anite' in k or k in ('MorphTargets', 'NumMorphTargets', 'Clothing', 'ClothingAssets')})
c = collections.Counter((x.get('tags') or {}).get('NaniteEnabled') for x in registry.assets if x['class'].endswith('.SkeletalMesh'))
print('skeletal NaniteEnabled tags:', dict(c))

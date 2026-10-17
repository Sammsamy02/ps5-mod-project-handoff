import json, sys
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
st = json.load(open(sys.argv[2]))
def short(p): return p.rsplit('/', 1)[-1]
for k, v in sorted(st.items()):
    n = short(k)
    if n in ('M_Hide', 'M_skin_unified_baked', 'M_MRO_TranslucentGlass_OneSided_ThinTranslusent', 'M_MRO_TranslucentGlass_OneSided',
             'MM_Prop_Basic', 'MM_Prop_Glass_Cutscenes', 'M_MRO_Cloth_NonVT', 'M_MRO_Dither_Glass', 'M_IVBag_Liquid'):
        print(n, 'TBasePassCS' if any(s.startswith('TBasePassCS') for s in v) else 'no-CS', [s for s in v if 'Nanite' in s or s.startswith('TBasePassCS')][:3])
cs = [short(k) for k, v in st.items() if any(s.startswith('TBasePassCS') for s in v)]
print('materials with TBasePassCS:', len(cs), 'of', len(st))
names = ['SM_Glasses_Lenses', 'SM_Glasses_Frame', 'SM_Glasses_Leg_L', 'Simon_FirstStage_IVBagFront', 'Simon_FirstStage_IVBagBack', 'Simon_Bodywarmer_Bed', 'SM_Canula']
for x in registry.assets:
    if short(x['package']) in names:
        print(short(x['package']), x['class'], {k: v for k, v in (x.get('tags') or {}).items()})

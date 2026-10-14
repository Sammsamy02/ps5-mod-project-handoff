import struct, sys
sys.dont_write_bytecode = True
T = '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools'
sys.path.insert(0, T)
import mod_material_audit as mma, mesh_sections, zen, material_props as mprops
OBS = '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations'
EV = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence'
pk, _ = mma.load_mod(OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc',
                     OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas')
script = mma.load_script_objects(EV + '/console-pulls-20261013-paks2/global.utoc', EV + '/console-pulls-20261013-paks2/global.ucas')
n = '/Game/Townfall/Characters/Simon/Meshes/JacketVariation/SK_Simon_FirstStage_Jacket'
z = pk[n]; d = z.data; expo = struct.unpack_from('<i', d, 32)[0]
print('header_size', z.header_size, 'len', len(d), 'cooked_header_size', z.cooked_header_size)
for e in z.exports:
    soff, ss = struct.unpack_from('<QQ', d, expo + 72 * e['index'])
    c = z.describe_ref(e['class'], script)
    print(e['index'], e['name'], c.get('path') if c else None, 'outer', e['outer'], 'cso', soff, 'size', ss, 'zen data_offset-hs', e.get('data_offset') - z.header_size)
    print('   bytes@cso   ', d[z.header_size + soff:z.header_size + soff + 40].hex())
    print('   bytes@zen   ', d[e['data_offset']:e['data_offset'] + 40].hex())
    print('   tail@cso    ', d[z.header_size + soff + ss - 24:z.header_size + soff + ss].hex())
    print('   tail@zen    ', d[e['data_offset'] + ss - 24:e['data_offset'] + ss].hex())
print('last 8', d[-8:].hex())
# decode under zen.py slicing to see whether the self-checks catch it
orig = mesh_sections.export_slices
mesh_sections.export_slices = lambda zz: [(e, zz.export_bytes(e)) for e in zz.exports]
try:
    m = mesh_sections.decode(d)
    print('zen.py slicing decode OK: material table at', m['material_table_offset'], 'lods', len(m['lods']))
except mesh_sections.MeshError as x:
    print('zen.py slicing decode FAIL', x)
mesh_sections.export_slices = orig
m = mesh_sections.decode(d)
print('cso decode OK: material table at', m['material_table_offset'])

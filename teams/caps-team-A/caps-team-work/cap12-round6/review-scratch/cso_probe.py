"""Reviewer probe: zen.py bundle-order export offsets vs CookedSerialOffset, on the mod's packages (read-only)."""
import collections, struct, sys
sys.dont_write_bytecode = True
T = '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools'
sys.path.insert(0, T)
import mod_material_audit as mma, mesh_sections, zen, material_props as mprops
OBS = '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations'
pk, _ = mma.load_mod(OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc',
                     OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas')
st = collections.Counter(); ex = []
for n, z in sorted(pk.items()):
    try:
        sl = mesh_sections.export_slices(z)
    except mesh_sections.MeshError as e:
        st['tiling_fail'] += 1; ex.append((n, 'tiling', str(e))); continue
    multi = len(z.exports) > 1
    d = z.data; expo = struct.unpack_from('<i', d, 32)[0]
    mism = 0
    for e, b in sl:
        if z.export_bytes(e) != b:
            mism += 1
    # bundle order vs cso order
    bundle = struct.unpack_from('<i', d, 24 + 12)[0]; dep_h = struct.unpack_from('<i', d, 24 + 16)[0]
    ent = [struct.unpack_from('<II', d, bundle + 8 * i) for i in range((dep_h - bundle) // 8)]
    ser = [ei for ei, c in ent if c == 1]
    cso = [e['index'] for e, b in sorted(sl, key=lambda r: struct.unpack_from('<Q', d, expo + 72 * r[0]['index'])[0])]
    st['multi=%s mismatched=%s order_eq=%s' % (multi, mism > 0, ser == cso)] += 1
    if mism and len(ex) < 6:
        top = [e for e, b in sl if e['outer'][0] == 'Null']
        tb_cso = [b for e, b in sl if e['outer'][0] == 'Null'][0]
        tb_zen = mprops.top_export_bytes(z)
        def hdr_ok(b):
            try:
                mprops.parse_header(b); return True
            except Exception as x:
                return 'ERR ' + str(x)[:50]
        ex.append((n, len(z.exports), 'bundle', ser[:6], 'cso', cso[:6], 'top_hdr_cso', hdr_ok(tb_cso), 'top_hdr_zen', hdr_ok(tb_zen)))
print(dict(st))
for e in ex:
    print(e)

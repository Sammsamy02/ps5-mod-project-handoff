"""Reviewer probe: length-prefixed strings in game material exports (read-only)."""
import collections, json, re, struct, sys
sys.dont_write_bytecode = True
sys.argv = sys.argv
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])   # reuse loaders from vf_probe.py (up to the analysis)
LP = re.compile(rb'(?=([\x04-\x7f]\x00\x00\x00))')
def strings(b):
    out = []
    for m in LP.finditer(b):
        o = m.start(); n = struct.unpack_from('<i', b, o)[0]
        s = b[o+4:o+4+n]
        if len(s) == n and s.endswith(b'\x00') and re.fullmatch(rb'[A-Za-z_][A-Za-z0-9_<>:, ]*\x00', s):
            out.append(s[:-1].decode())
    return out
cnt = collections.Counter(); per = {}
for pkg, z in sorted(whole.items()):
    a_ = registry.asset(pkg)
    if not a_ or a_['class'] != mc.MATERIAL_AR_CLASS:
        continue
    tops = [b for e, b in mesh_sections.export_slices(z) if e['outer'][0] == 'Null']
    ss = set(strings(tops[0]))
    per[pkg] = sorted(ss)
    cnt.update(ss)
mh = [k for k in per if k.endswith('/M_Hide')][0]
print('M_Hide strings:', per[mh])
print('nanite-ish:', {k: v for k, v in cnt.items() if 'anite' in k})
print('VF-ish:', {k: v for k, v in cnt.items() if 'Factory' in k})
print('n distinct', len(cnt))
json.dump(per, open(sys.argv[2], 'w'), indent=0)

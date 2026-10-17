#!/usr/bin/env python3
"""Generates 0001-v2-l26-lua-as-cpp-c-abi-opt-in.patch (Mac-only, read-only on
the real tree). Reads outputs/2026-10-08-l26-runtime/build_variants.py, checks
its base sha, applies exact-count text edits in memory, writes the unified diff
here. Every edit must match exactly once or the script fails. Prints how many
edits it applied; exits non-zero if zero or if any edit failed."""
import difflib, hashlib, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[5]
REL = 'outputs/2026-10-08-l26-runtime/build_variants.py'
BASE_SHA = '60408ceff2dc340fc126bb4fde2fa69c742f9253dbb0d916625735a3f638f0db'
PREFIX_SRC = HERE.parents[1] / 'host-test-v2/lua_c_abi_prefix.h'
PREFIX_REL = 'outputs/2026-10-08-l26-runtime/lua-c-abi/lua_c_abi_prefix.h'
src = (WHAT / REL).read_text()
if hashlib.sha256(src.encode()).hexdigest() != BASE_SHA: sys.exit('FAIL: build_variants.py base sha changed')
prefix_sha = hashlib.sha256(PREFIX_SRC.read_bytes()).hexdigest()
new = src; applied = 0
def edit(old, rep):
    global new, applied
    n = new.count(old)
    if n != 1: sys.exit(f'FAIL: edit anchor found {n} times: {old[:80]!r}')
    new = new.replace(old, rep); applied += 1

edit("CTOKEN='MODBRIDGE_L25_ENGINE_ADD_LISTENER'\n", "CTOKEN='MODBRIDGE_L25_ENGINE_ADD_LISTENER'\n" + f'''# [LUA-AS-CPP v2] STAGED (lua-as-cpp track 2026-10-12, round 2; needs the
# user's OK and its own labelled build). Opt-in env L26_LUA_AS_CPP=1; unset =
# today's build, byte-for-byte the same selection.
# Compiles ONLY the 33 luaraw-* units as C++ so lua_error / coroutine.yield
# become C++ throws (ldo.c:55-61) and C++ destructors run in every frame a Lua
# error crosses (PS5/FreeBSD longjmp skips them). Every public Lua symbol keeps
# its C name: lua-c-abi/lua_c_abi_prefix.h is force-included (-include) and
# sets LUA_API to extern "C" (Lua's own customisation point, luaconf.h:282-
# 306). Upstream's LUA_COMPILED_AS_CPP (C++ linkage) is NOT used: the frozen
# plain-C packaging unit lineage/probe/memory_lua.c calls 8 Lua functions by
# C name and would no longer link. So no other unit gets a new define and no
# caller is recompiled for this change.
#  * luaraw-* argv: prospero-clang -> prospero-clang++ (adds -frtti
#    -fexceptions, prospero-clang++:83), -std=c11 -> -std=c++23
#    -Wno-deprecated-enum-enum-conversion -include <prefix>, '-x c++' before
#    -c, and NO -resource-dir (no C++ unit has one).
#  * include-graph gate: for these 33 units a NEW dependency row is also
#    approved when the same resolved path with the same sha256 is already a
#    pinned dependency of the BASE build; every such row is recorded (gate
#    G-LUACPP-include-exemption). The prefix lives under this directory, so
#    the existing HERE rule covers it; its sha is pinned below.
#  * G7: the raw import delta must equal LUA_CPP_IMPORT_DELTA exactly
#    (Mac probe v2: only luaraw-ldo.o used setjmp/longjmp; the 7 new C++ EH
#    references resolve inside the runtime). The FINAL PRX still imports both
#    (the frozen lineage import table import_references.S lists them), so the
#    packaging gates P7/P11 stay at 207 and l26_packaging_driver.py is NOT
#    changed.
LUA_AS_CPP=os.environ.get('L26_LUA_AS_CPP')=='1'
LUA_C_ABI_PREFIX=HERE/'lua-c-abi'/'lua_c_abi_prefix.h'
LUA_C_ABI_PREFIX_SHA256='{prefix_sha}'
LUA_CPP_EXPECTED_LUARAW_UNITS=33
LUA_CPP_IMPORT_DELTA={{'removed':[('U','longjmp'),('U','setjmp')],'added':[]}}
def lua_cpp_argv(argv):
    if sha(LUA_C_ABI_PREFIX)!=LUA_C_ABI_PREFIX_SHA256:raise RuntimeError('lua_c_abi_prefix.h sha drift')
    a=list(argv)
    if not a[0].endswith('/prospero-clang'):raise RuntimeError('luaraw unit not compiled by prospero-clang: '+a[0])
    a[0]=a[0]+'++'
    if a.count('-std=c11')!=1 or a.count('-resource-dir')!=1 or a.count('-c')!=1:raise RuntimeError('unexpected luaraw argv shape')
    i=a.index('-std=c11');a[i:i+1]=['-std=c++23','-Wno-deprecated-enum-enum-conversion','-include',str(LUA_C_ABI_PREFIX)]
    j=a.index('-resource-dir');del a[j:j+2]
    a[a.index('-c'):a.index('-c')]=['-x','c++']
    return a
''')

edit("""    return {'oldsel':oldsel,'approved_header_identities':approved_header_identities,'changes':changes,'added':added,
""", """    lua_cpp_units=[];base_dep_pins={}
    if LUA_AS_CPP:
        lua_cpp_units=sorted(u['name'] for u in base['selected_units'] if u['name'].startswith('luaraw-'))
        pinned={str(Path(p['path']).resolve()):p['sha256'] for p in oldsel['source_pins']}
        base_dep_pins={rp:pinned[rp] for rp in universe_res if rp in pinned}
        prefix_ok=LUA_C_ABI_PREFIX.exists() and sha(LUA_C_ABI_PREFIX)==LUA_C_ABI_PREFIX_SHA256
        ok=len(lua_cpp_units)==LUA_CPP_EXPECTED_LUARAW_UNITS and len(base_dep_pins)>0 and prefix_ok
        record('G-LUACPP-scope',ok,{'luaraw_units':len(lua_cpp_units),'expected_luaraw_units':LUA_CPP_EXPECTED_LUARAW_UNITS,
               'base_pinned_dependencies':len(base_dep_pins),'prefix':str(LUA_C_ABI_PREFIX),'prefix_sha256_ok':prefix_ok,
               'other_units_changed':0,'declared_import_delta':{k:[list(t) for t in v] for k,v in LUA_CPP_IMPORT_DELTA.items()}})
        if not ok:raise RuntimeError('G-LUACPP-scope: unexpected Lua-as-C++ scope (zero count, not 33 luaraw units, or prefix sha drift)')
        affected=affected+[n for n in lua_cpp_units if n not in affected]
    return {'oldsel':oldsel,'approved_header_identities':approved_header_identities,'changes':changes,'added':added,
            'lua_cpp_units':set(lua_cpp_units),'base_dep_pins':base_dep_pins,
""")

edit("""        if u['name'] in define_units:argv[1:1]=[variant['define']]
        routes,route_violations""", """        if u['name'] in define_units:argv[1:1]=[variant['define']]
        if u['name'] in sel.get('lua_cpp_units',()):argv=lua_cpp_argv(argv)
        routes,route_violations""")

edit("""            staged=sel.get('driver_staged_sources') or {}
            extra=[str(p) for p in ds if str(p.resolve()) not in oldset
                   and not p.resolve().is_relative_to(HERE)
                   and (p.name,sha(p)) not in sel['approved_header_identities']
                   and staged.get(str(p.resolve()))!=sha(p)]
            if extra:raise RuntimeError('unapproved include graph additions '+u['name']+': '+repr(extra))""",
"""            staged=sel.get('driver_staged_sources') or {}
            extra=[str(p) for p in ds if str(p.resolve()) not in oldset
                   and not p.resolve().is_relative_to(HERE)
                   and (p.name,sha(p)) not in sel['approved_header_identities']
                   and staged.get(str(p.resolve()))!=sha(p)]
            if u['name'] in sel.get('lua_cpp_units',()):
                # [LUA-AS-CPP v2] approve ONLY rows identical to a BASE-pinned dependency; record each one.
                lua_exempt=[x for x in extra if sel['base_dep_pins'].get(str(Path(x).resolve()))==sha(x)]
                extra=[x for x in extra if x not in lua_exempt]
                new['lua_cpp_base_pinned_rows']=[{'path':x,'sha256':sha(x)} for x in lua_exempt]
            if extra:raise RuntimeError('unapproved include graph additions '+u['name']+': '+repr(extra))""")

edit("""    units=[done[u['name']] for u in units]
    record('G12-header-routing""", """    units=[done[u['name']] for u in units]
    if LUA_AS_CPP and execute:
        lua_rows={u['name']:len(u.get('lua_cpp_base_pinned_rows',[])) for u in units if u['name'] in sel['lua_cpp_units']}
        ok=len(lua_rows)==LUA_CPP_EXPECTED_LUARAW_UNITS and sum(lua_rows.values())>0
        record('G-LUACPP-include-exemption',ok,{'variant':variant['key'],'pass':passno,'units_checked':len(lua_rows),
               'approved_rows_total':sum(lua_rows.values()),'approved_rows_per_unit':lua_rows,
               'rule':'new dependency row approved only if same resolved path + same sha256 as a BASE-pinned dependency; rows listed per unit in COMPOSITION-RECEIPT selected_units[].lua_cpp_base_pinned_rows'})
        if not ok:raise RuntimeError('G-LUACPP-include-exemption: zero rows or not 33 Lua units checked')
    record('G12-header-routing""")

edit("""        record('G7-import-delta-none',symbols(after)==symbols(before),{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout')})
        if symbols(after)!=symbols(before):raise RuntimeError('raw import delta must be declared before packaging')""",
"""        if LUA_AS_CPP:
            expected=(symbols(before)-set(LUA_CPP_IMPORT_DELTA['removed']))|set(LUA_CPP_IMPORT_DELTA['added'])
            ok=symbols(after)==expected and len(symbols(after))>0 and len(symbols(before))>0
            record('G7-import-delta-declared-lua-as-cpp',ok,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),
                   'declared':{k:[list(t) for t in v] for k,v in LUA_CPP_IMPORT_DELTA.items()},
                   'actual_added':sorted([list(t) for t in symbols(after)-symbols(before)]),'actual_removed':sorted([list(t) for t in symbols(before)-symbols(after)]),
                   'unexpected_added':sorted([list(t) for t in symbols(after)-expected]),'unexpected_missing':sorted([list(t) for t in expected-symbols(after)]),
                   'counts':(len(symbols(before)),len(symbols(after)))})
            if not ok:raise RuntimeError('Lua-as-C++ raw import delta differs from the declared delta')
        else:
            record('G7-import-delta-none',symbols(after)==symbols(before),{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout')})
            if symbols(after)!=symbols(before):raise RuntimeError('raw import delta must be declared before packaging')""")

edit("""'added':[],'removed':[],'baseline_artifact':pin(BASE/f'pass{passno}/undefined.stdout'),""",
     """'added':sorted([list(t) for t in symbols(after)-symbols(before)]),'removed':sorted([list(t) for t in symbols(before)-symbols(after)]),'baseline_artifact':pin(BASE/f'pass{passno}/undefined.stdout'),""")
edit("""'runtime':pin(runtime),'imports_changed':False}),flush=True)""", """'runtime':pin(runtime),'imports_changed':symbols(after)!=symbols(before)}),flush=True)""")
edit("""                'undefined_counts':(len(symbols(before)),len(symbols(after)))}""",
     """                'undefined_counts':(len(symbols(before)),len(symbols(after))),
                'undefined_delta':(sorted([list(t) for t in symbols(after)-symbols(before)]),sorted([list(t) for t in symbols(before)-symbols(after)]))}""")
edit("""'actual_undefined_added':[],'actual_undefined_removed':[],""",
     """'actual_undefined_added':pp['undefined_delta'][0],'actual_undefined_removed':pp['undefined_delta'][1],""")
edit("""        record('G7-import-delta-none',None,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),'baseline_sha256':sha(BASE/f'pass{passno}/undefined.stdout'),'note':'llvm-nm -u equality with BASE enforced at execute'})""",
     """        record('G7-import-delta-declared-lua-as-cpp' if LUA_AS_CPP else 'G7-import-delta-none',None,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),'baseline_sha256':sha(BASE/f'pass{passno}/undefined.stdout'),
               'note':('llvm-nm -u must equal BASE minus exactly '+repr(LUA_CPP_IMPORT_DELTA['removed'])+' (enforced at execute)') if LUA_AS_CPP else 'llvm-nm -u equality with BASE enforced at execute'})""")

if applied == 0: sys.exit('FAIL: zero edits applied')
diff = ''.join(difflib.unified_diff(src.splitlines(True), new.splitlines(True), 'a/' + REL, 'b/' + REL, n=3))
pre = PREFIX_SRC.read_text().splitlines(True)
diff += f'--- /dev/null\n+++ b/{PREFIX_REL}\n@@ -0,0 +1,{len(pre)} @@\n' + ''.join('+' + l for l in pre)
out = HERE / '0001-v2-l26-lua-as-cpp-c-abi-opt-in.patch'
out.write_text(diff)
(HERE / 'build_variants.patched.py').write_text(new)
print(f'edits applied: {applied}; prefix sha256 {prefix_sha}; patch {out.name} sha256 {hashlib.sha256(diff.encode()).hexdigest()}')

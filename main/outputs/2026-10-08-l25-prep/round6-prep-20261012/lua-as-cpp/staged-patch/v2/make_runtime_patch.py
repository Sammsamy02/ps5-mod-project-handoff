#!/usr/bin/env python3
"""Generates 0002-runtime-catch-all-passes-lua-errors.patch (Mac-only; reads the
candidate tree, writes only here). Inserts, directly before each listed
`catch (...)`, a handler that re-throws a Lua error unchanged:

    catch (void*) { throw; }   // a Lua error (struct lua_longjmp*) passes through

Rule (generic, no mod/game names): in the C++ Lua build a Lua error must pass
every catch(...) exactly the way the C build's longjmp passes it. Lua throws
`struct lua_longjmp*` (ldo.c:57 `throw(c)`); a `void*` handler matches any
thrown object pointer ([except.handle]/3); lua_longjmp is incomplete outside
ldo.c, so it cannot be named. In today's C build nothing throws a pointer
(audit v2: 0 pointer throw-expressions), so the handler is dead code there and
behaviour is unchanged. Only sites whose Lua handler is reached WITHOUT
crossing game/engine frames are listed (see FEASIBILITY H4 for the others).
Each site is checked by base sha + exact line text; prints the count; exits
non-zero on zero or on any mismatch."""
import difflib, hashlib, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[5]
SRC = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/'
SITES = {  # file: (base sha256, [catch(...) line numbers])
    'LuaUObject.cpp': ('aa10e4eadabe2f324fce2f95487bff9c76c225dfab22e0809dfc440235e16a32', [142]),
    'LuaMadeSimple/LuaMadeSimple.cpp': ('418c19505c888abe6fce58ab49385c5d5a6bd56e7641385f8a03b28310d0d2a5', [1459, 1587]),
    'LuaMod.cpp': ('4d6bc63b7a90a8026a9c918ee6bee36c2d4eaa450220be82e8071f5427bfde9a', [1018, 1446]),
    'LuaPathShim.cpp': ('9aef55a109f9ae2e9c6e8344d8b258c14be6c4e20f4e2e0dc3b8df3edac37110', [1790]),
}
NOTE = {
    142: 'Lua-as-C++: a Lua error raised while marshalling goes to the inner lua_pcall, as with the C build\'s longjmp (was: stored and re-thrown outside it -> status -1, message lost).',
    1587: 'Lua-as-C++: LUA_ERRMEM from luaL_traceback inside handle_error must propagate (was: swallowed -> Lua\'s global lock left held).',
}
diff = ''; count = 0
for rel, (base, lines) in SITES.items():
    p = WHAT / SRC / rel
    data = p.read_bytes()
    if hashlib.sha256(data).hexdigest() != base: sys.exit(f'FAIL: base sha changed: {rel}')
    old = data.decode().splitlines(True); new = list(old)
    for ln in sorted(lines, reverse=True):
        line = old[ln - 1]
        if line.strip() != 'catch (...)': sys.exit(f'FAIL: {rel}:{ln} is not "catch (...)": {line!r}')
        ind = line[:len(line) - len(line.lstrip())]
        why = NOTE.get(ln, 'Lua-as-C++: a Lua error passes this handler unchanged, as the C build\'s longjmp does.')
        ins = [f'{ind}catch (void*)\n', f'{ind}{{\n',
               f'{ind}    // {why}\n',
               f'{ind}    // A Lua error is a thrown struct lua_longjmp* (ldo.c:57). Dead code with Lua built as C.\n',
               f'{ind}    throw;\n', f'{ind}}}\n']
        new[ln - 1:ln - 1] = ins; count += 1
    diff += ''.join(difflib.unified_diff(old, new, 'a/' + SRC + rel, 'b/' + SRC + rel, n=3))
if count == 0: sys.exit('FAIL: zero sites patched')
out = HERE / '0002-runtime-catch-all-passes-lua-errors.patch'
out.write_text(diff)
print(f'sites patched: {count} in {len(SITES)} files; patch sha256 {hashlib.sha256(diff.encode()).hexdigest()}')

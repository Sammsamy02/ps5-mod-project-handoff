from pathlib import Path
import hashlib, json, subprocess

# [LFB] dedicated suite (agent-EY 2026-10-09, Claude item 5): builds the
# -fshort-wchar host harness with the REAL Lua 5.5 interpreter and the REAL
# converter header (include/LuaFNameBridge.hpp included VERBATIM — the code
# under test), against a recording stub FName slot-2 ctor + a stub pool
# decoder that decodes the RECORDED ctor units back (the [LUA-FN] OK/MISMATCH
# verdict is a real round trip, not hardcoded).
#   green     : 'PlayerController' arrives as 16 UTF-16 code units at the
#               stub ctor; [LUA-FN] in/len/id/decoded/OK line; consumption
#               parity; 2-byte + astral UTF-8; reverse push_fname_to_lua /
#               push_wide_to_lua (lua_pushlstring); embedded-NUL/overlong/
#               invalid-UTF-8 refusals (NAME_None, ctor never called, seam
#               notices); memory-source ctor; first-20-uses log bound;
#               [LFB] composition marker exactly once.
#   mismatch  : the stub decoder lies by one 'X' — the bridge must log
#               MISMATCH (verdict logic exercised, not assumed).
# Same -fshort-wchar + REAL-Lua recipe as the EV string-seam suite.

p = Path(__file__).resolve().parent.parent
root = p.parents[2]  # what/
deps_string = root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include'
lua_inc = '/opt/homebrew/opt/lua/include'
lua_lib = '/opt/homebrew/opt/lua/lib'

manifest = {
    'converter_header': str(p / 'include/LuaFNameBridge.hpp'),
    'converter_header_sha256': hashlib.sha256((p / 'include/LuaFNameBridge.hpp').read_bytes()).hexdigest(),
    'fixture': str(p / 'tests/actual_lua_fname_bridge.cpp'),
    'stub_nametypes': str(p / 'tests/lfb-stubs/Unreal/NameTypes.hpp'),
    'stub_output': str(p / 'tests/lfb-stubs/DynamicOutput/Output.hpp'),
}
results = []
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'lua-fname-bridge-{mode}'
    a = ['clang++', '-std=c++20', '-fshort-wchar',
         '-I', str(p / 'tests/lfb-stubs'),   # shadow <Unreal/NameTypes.hpp>/<DynamicOutput/Output.hpp>
         '-I', str(p / 'include'),           # the REAL converter header
         '-I', str(deps_string),             # the REAL <String/StringType.hpp>
         '-I', lua_inc + '/lua', '-L', lua_lib, '-llua5.5',
         str(p / 'tests/actual_lua_fname_bridge.cpp'), '-o', str(binary)]
    if mode != 'plain':
        a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    x = subprocess.run(a, capture_output=True, text=True)
    if x.returncode != 0:
        print(x.stderr[-6000:])
    assert x.returncode == 0, f'compile failed ({mode})'
    for suite in ['green', 'mismatch']:
        x = subprocess.run([str(binary)] + ([] if suite == 'green' else ['mismatch']), capture_output=True, text=True)
        r = dict(name=f'lua-fname-bridge-{suite}', mode=mode, exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
        results.append(r)
        print(f'lua-fname-bridge-{suite}', mode, x.returncode, x.stdout.strip())
        assert x.returncode == 0, x.stderr + x.stdout

(p / 'results/LUA-FNAME-BRIDGE.json').write_text(json.dumps({'manifest': manifest, 'results': results}, indent=2))
print('lua-fname-bridge: ALL PASS')

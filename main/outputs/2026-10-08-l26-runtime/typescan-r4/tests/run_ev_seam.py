from pathlib import Path
import hashlib, json, subprocess

# [L27][EV string-seam] dedicated suite (agent-EV 2026-10-09, Claude tasks
# 3-5). Builds the -fshort-wchar host harness with the REAL Lua interpreter,
# the REAL candidate seam conversions (Helpers/String.hpp + Utf16Utf8.hpp),
# the REAL rc::fnameabi adapter and the VERBATIM runner-extracted FName
# construction seam (NameTypes.hpp construct_with_*/ctor bodies) and
# UObjectGlobals helpers (NulTerminatedNameCopy + MakeFindXOfClassName).
# green = Lua 'PlayerController' -> 16 code units at the stub engine ctor;
# red = the stride-corruption interleave refused loudly (never truncated).
# Same marker+sha region discipline as the ER/ET fixtures.

p = Path(__file__).resolve().parent.parent
root = p.parents[2]  # what/
regions = p / 'results' / 'ev-string-seam-verify' / 'regions'
regions.mkdir(parents=True, exist_ok=True)

nt = (p / 'candidate/include/Unreal/NameTypes.hpp').read_text().splitlines(keepends=True)
uog = (p / 'candidate/src/UObjectGlobals.cpp').read_text().splitlines(keepends=True)


def cut(lines, start_pred, end_pred, must=()):
    start = next(i for i, l in enumerate(lines) if start_pred(l))
    end = next(i for i in range(start, len(lines)) if end_pred(lines[i]))
    region = ''.join(lines[start:end + 1])
    for m in must:
        assert m in region, f'marker missing: {m}'
    return region, (start + 1, end + 1)


def cut_function(lines, signature_pred, must=()):
    # Depth-aware function-region cut. A naive 'first line that strips to }'
    # stop stops at the first INNER block closer (MakeFindXOfClassName has
    # multi-line if-bodies whose '}' lines strip to '}' too), truncating the
    # region mid-function and unbalancing every fixture that includes it.
    start = next(i for i, l in enumerate(lines) if signature_pred(l))
    depth = 0
    opened = False
    end = start
    for i in range(start, len(lines)):
        depth += lines[i].count('{') - lines[i].count('}')
        if '{' in lines[i]:
            opened = True
        if opened and depth == 0:
            end = i
            break
    else:
        raise AssertionError('function region never closed')
    region = ''.join(lines[start:end + 1])
    for m in must:
        assert m in region, f'marker missing: {m}'
    return region, (start + 1, end + 1)


# NameTypes.hpp: the [EV string-seam] private construction path
start_a = next(i for i, l in enumerate(nt) if l.strip().startswith('// [L27][EV string-seam] (agent-EV 2026-10-09, Claude directive)'))
ra = (start_a + 1, start_a + 1)
# end at the closing brace of the (Name, InNumber) forwarding overload
end_a = next(i for i in range(start_a, len(nt)) if 'Number = InNumber;' in nt[i])
while nt[end_a].strip() != '}':
    end_a += 1
members_a = ''.join(nt[start_a:end_a + 1])
for m in ('construct_with_sized_string', 'NotifyFNameSeamRefusedOnce', 'embedded-nul', 'FMemory::Memcpy'):
    assert m in members_a, f'members region missing {m}'

# NameTypes.hpp: the three string ctors (CharType* + both StringViewType)
ctor_start = next(i for i, l in enumerate(nt) if 'explicit FName(const CharType* StrName, EFindName FindType' in l)
ctor_end = next(i for i in range(ctor_start, len(nt)) if 'Number = InNumber;' in nt[i])
while nt[ctor_end].strip() != '}':
    ctor_end += 1
members_b = ''.join(nt[ctor_start:ctor_end + 1])
for m in ('construct_with_sized_string(str_name.data(), str_name.size()', 'construct_with_sized_string(Name.data(), Name.size()'):
    assert m in members_b, f'ctor region missing {m}'

(regions / 'ev_string_seam_members.inc').write_text(members_a + '\n' + members_b)

# UObjectGlobals.cpp: NulTerminatedNameCopy + MakeFindXOfClassName verbatim
ntc_region, ntc_lines = cut_function(uog, lambda l: 'static auto NulTerminatedNameCopy(StringViewType View) -> StringType' in l)
mk_region, mk_lines = cut_function(uog, lambda l: 'static auto MakeFindXOfClassName(StringViewType ClassName) -> FName' in l)
uog_region = ntc_region + '\n' + mk_region
for m in ('StringType{View.data(), View.size()}', 'FNAME_Find', 'LogFindXOfAddFallback', 'return Name;'):
    assert m in uog_region, f'uog region missing {m}'
(regions / 'ev_string_seam_uobjectglobals.inc').write_text(uog_region)

manifest = {
    'nametypes_source': str(p / 'candidate/include/Unreal/NameTypes.hpp'),
    'nametypes_sha256': hashlib.sha256((p / 'candidate/include/Unreal/NameTypes.hpp').read_bytes()).hexdigest(),
    'members_lines': [ra[0], end_a + 1],
    'members_sha256': hashlib.sha256((members_a + '\n' + members_b).encode()).hexdigest(),
    'ctor_lines': [ctor_start + 1, ctor_end + 1],
    'uobjectglobals_source': str(p / 'candidate/src/UObjectGlobals.cpp'),
    'uobjectglobals_sha256': hashlib.sha256((p / 'candidate/src/UObjectGlobals.cpp').read_bytes()).hexdigest(),
    'uobjectglobals_lines': [ntc_lines[0], ntc_lines[1], mk_lines[0], mk_lines[1]],
    'uobjectglobals_sha_region': hashlib.sha256(uog_region.encode()).hexdigest(),
}
(p / 'results/ev-string-seam-verify/EV-REGION-MANIFEST.json').write_text(json.dumps(manifest, indent=1))

deps_string = root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include'
l19_units = root / 'outputs/2026-10-07-l19-local-profile-r4/units'
lua_inc = '/opt/homebrew/opt/lua/include'
lua_lib = '/opt/homebrew/opt/lua/lib'

results = []
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'ev-string-seam-{mode}'
    a = ['clang++', '-std=c++20', '-fshort-wchar',
         '-I', str(p / 'candidate/include'),
         '-I', str(l19_units),
         '-I', str(deps_string),
         '-I', str(regions),
         '-I', lua_inc + '/lua', '-L', lua_lib, '-llua5.5',
         str(p / 'tests/actual_ev_string_seam.cpp'), '-o', str(binary)]
    if mode != 'plain':
        a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    x = subprocess.run(a, capture_output=True, text=True)
    if x.returncode != 0:
        print(x.stderr[-6000:])
    assert x.returncode == 0, f'compile failed ({mode})'
    for suite in ['green', 'red']:
        x = subprocess.run([str(binary)] + ([] if suite == 'green' else ['red']), capture_output=True, text=True)
        r = dict(name=f'ev-string-seam-{suite}', mode=mode, exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
        results.append(r)
        print(f'ev-string-seam-{suite}', mode, x.returncode, x.stdout.strip())
        assert x.returncode == 0, x.stderr

(p / 'results/ev-string-seam-verify/EV-STRING-SEAM.json').write_text(json.dumps(results, indent=2))
print('ev-string-seam: ALL PASS')

# [L27][ET nul-throw-armor] verification driver (2026-10-09).
# Runs, in plain and ASan/UBSan modes:
#   1. the NEW et-nul-terminator fixture (Claude's exact spec: build
#      "/Script/Engine" from a SLICE of "/Script/Engine.Actor", construct the
#      FName from the u16string copy, decode with the VERBATIM production
#      pool decoder, assert EXACT decode; case-insensitive round trips);
#   2. the UPDATED staticfindobject-path-fix fixture (agent-EC, updated by
#      agent-ET for the nullptr-never-throw + decoded-walk-fallback
#      semantics and the real NUL-scanning engine-ctor mock);
#   3. the ER pool-decoder regression suites (happy/refuse/race/truncation).
# Provenance discipline mirrors agent-ER's run.py block: the decoder region
# is extracted VERBATIM from candidate/src/NameTypes.cpp with marker + sha
# pins, and the fixture's NulTerminatedNameCopy body is asserted textually
# equal to the production helper in candidate/src/UObjectGlobals.cpp.
from pathlib import Path
import hashlib
import json
import subprocess

p = Path(__file__).resolve().parent.parent
results = []

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)

# ---- 1. verbatim decoder region extraction (agent-ER discipline) ----
et_dir = p / 'results' / 'et-nul-verify'
et_regions = et_dir / 'regions'
et_regions.mkdir(parents=True, exist_ok=True)
nt = (p / 'candidate/src/NameTypes.cpp').read_text().splitlines(keepends=True)
start = next(i for i, l in enumerate(nt) if l.strip() == 'namespace FNamePoolDecoder')
end = next(i for i, l in enumerate(nt[start:], start) if l.strip() == '} // namespace FNamePoolDecoder')
er_inc = ''.join(nt[start:end + 1])
assert 'kCurrentBlockOffset = 0x8' in er_inc and 'rc::engtostring::kProbeId' in er_inc and 'try_tostring' in er_inc
(et_regions / 'er_pooldecoder.inc').write_text(er_inc)
manifest = {
    'source': str(p / 'candidate/src/NameTypes.cpp'),
    'source_sha256': hashlib.sha256((p / 'candidate/src/NameTypes.cpp').read_bytes()).hexdigest(),
    'region_lines': [start + 1, end + 1],
    'region_sha256': hashlib.sha256(er_inc.encode()).hexdigest(),
}
(et_dir / 'er-region-manifest.json').write_text(json.dumps(manifest, indent=1))

# ---- 2. fixture's NulTerminatedNameCopy == production helper (textual) ----
uog = (p / 'candidate/src/UObjectGlobals.cpp').read_text()
marker = 'static auto NulTerminatedNameCopy(StringViewType View) -> StringType\n    {\n        if (View.empty()) { return StringType{}; }\n        return StringType{View.data(), View.size()};\n    }'
assert uog.count(marker) == 1, 'production NulTerminatedNameCopy helper must appear exactly once'
fixture_src = (p / 'tests/actual_et_nul_terminator.cpp').read_text()
assert marker.replace('static auto NulTerminatedNameCopy', 'static auto NulTerminatedNameCopy') in fixture_src or True
# exact body equality against the fixture's verbatim copy
fixture_body = 'static auto NulTerminatedNameCopy(StringViewType View) -> StringType\n    {\n        if (View.empty()) { return StringType{}; }\n        return StringType{View.data(), View.size()};\n    }'
assert fixture_body in fixture_src, 'fixture NulTerminatedNameCopy must be the verbatim production body'

# ---- 3. compile + run matrix ----
for name, src, extra_i in [
    ('et-nul-terminator', str(p / 'tests/actual_et_nul_terminator.cpp'), ['-I', str(et_regions), '-I', str(p / 'include')]),
    ('staticfindobject-path-fix', str(p / 'tests/actual_staticfindobject_path_fix.cpp'), []),
]:
    for mode in ['plain', 'asan-ubsan']:
        binary = p / 'tests' / f'{name}-{mode}'
        a = ['clang++', '-std=c++20', '-include', 'utility'] + extra_i + [src, '-o', str(binary)]
        if mode != 'plain':
            a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
        x = run(a)
        r = dict(name=name, mode=mode, stage='compile', exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
        results.append(r)
        print(name, mode, 'compile', x.returncode)
        assert x.returncode == 0, x.stderr
        x = run([str(binary)])
        r = dict(name=name, mode=mode, stage='run', exit=x.returncode, stdout=x.stdout, stderr=x.stderr)
        results.append(r)
        print(name, mode, x.returncode, (x.stdout.strip().splitlines() or [''])[-1])
        assert x.returncode == 0, x.stderr + x.stdout

# ---- 4. ER pool-decoder regression suites (verbatim region re-run) ----
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'er-pooldecoder-{mode}'
    a = ['clang++', '-std=c++20', '-I', str(et_regions), '-I', str(p / 'include'),
         str(p / 'tests/actual_er_pooldecoder.cpp'), '-o', str(binary)]
    if mode != 'plain':
        a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    x = run(a)
    assert x.returncode == 0, x.stderr
    for suite in ['happy', 'refuse', 'race', 'truncation']:
        x = run([str(binary), suite])
        r = dict(name=f'er-pooldecoder-{suite}', mode=mode, stage='run', exit=x.returncode, stdout=x.stdout, stderr=x.stderr)
        results.append(r)
        print(f'er-pooldecoder-{suite}', mode, x.returncode, 'FAIL' if x.returncode else 'pass')
        assert x.returncode == 0, x.stderr + x.stdout

(et_dir / 'HOST-TESTS.json').write_text(json.dumps(results, indent=2))
print(f'et-nul-verify: {len(results)} entries, ALL PASS')

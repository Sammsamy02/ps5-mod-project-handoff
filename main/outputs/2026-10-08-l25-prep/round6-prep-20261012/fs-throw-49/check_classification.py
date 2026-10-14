#!/usr/bin/env python3
"""check_classification.py -- proves classification.json is complete and its
citations point at real code (fs-throw-49 track, Mac-only, read-only).

Checks:
  1. COVERAGE: the 49 sites in classification.json (file, line, function) are
     exactly the throwing sites harvest/fs_throw_gate.py's OWN scanner finds in
     the current tree for the 11 baseline files, and their gate keys are exactly
     the baseline multiset (harvest/fs_throw_baseline-1e49188b.txt).
  2. VERDICTS: every site has a known verdict; any UNHANDLED site must name a
     patch (none expected).
  3. CITATIONS (bounds only): every 'X:N' / 'X:N-M' reference in the JSON names
     a file from the sources table that has at least that many lines. This does
     NOT check the code text at those lines; only the ANCHORS below do that.
  4. ANCHORS: the load-bearing citations contain the expected code text.
  5. NO-CALLER: the two factories that build a UE4SSProgram without a mod source
     (PrepareDeferredUE4SSRuntime, CreateNativeUE4SSProgram) appear only at their
     own declarations/definitions in the compiled set, the unscanned prebuilt
     sources and the package lineage (fact R0).
Prints what it checked; exit 0 = all pass, 1 = any failure, 2 = ZERO sites,
ZERO citations, ZERO anchors or ZERO no-caller files checked (a checker that
checks nothing fails).
"""
import collections
import json
import os
import re
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
WHAT = os.path.abspath(os.path.join(HERE, *(['..'] * 4)))
HARVEST = os.path.join(WHAT, 'outputs', '2026-10-08-l25-prep', 'harvest')
sys.path.insert(0, HARVEST)
import fs_throw_gate as G  # noqa: E402

BASELINE = os.path.join(HARVEST, 'fs_throw_baseline-1e49188b.txt')

# (source key, line, expected substring) for the load-bearing claims
ANCHORS = [
    ('U', 222, 'try'), ('U', 225, 'ValidateNativeProgramContext'), ('U', 488, 'catch (std::exception& e)'),
    ('U', 490, 'catch (std::runtime_error& e)'), ('U', 464, 'if (!m_game_path_and_exe_name.empty())'),
    ('U', 637, 'try'), ('U', 646, 'catch (...)'), ('U', 664, 'try'), ('U', 750, 'catch (std::exception& e)'),
    ('U', 872, 'if (m_native_context.mod_source)'), ('U', 930, 'if (m_native_context.mod_source)'),
    ('U', 1018, 'else if (m_native_context.mod_source)'), ('U', 1027, 'StageNativeLayoutFiles'),
    ('U', 1654, 'try'), ('U', 1659, 'catch (const std::exception& e)'), ('U', 1678, 'EnableDebugKeyBindings'),
    ('U', 1689, 'try'), ('U', 1894, 'catch (const std::exception& error)'),
    ('U', 1964, 'if (m_native_context.mod_source)'), ('U', 2018, 'make_unique<CppMod>'),
    ('U', 2174, 'native_mod_source()'), ('U', 2751, 'queue_start_lua_mod_by_path'),
    ('U', 2837, 'if (m_native_context.mod_source)'), ('U', 2941, 'if (m_native_context.mod_source)'),
    ('U', 2955, 'if (m_native_context.mod_source)'), ('U', 3427, 'NewFileDevice'),
    ('L', 1124, 'm_native_source(program.native_mod_source())'), ('L', 1128, 'if (m_native_source)'),
    ('L', 1435, 'native_mod && native_mod->m_native_source'), ('L', 2021, 'IsTrueMod::Yes'),
    ('L', 2441, '"DumpAllObjects"'), ('L', 2452, '"GenerateSDK"'), ('L', 2463, '"GenerateLuaTypes"'),
    ('L', 2474, '"GenerateUHTCompatibleHeaders"'), ('L', 3346, '"IterateGameDirectories"'),
    ('L', 3360, 'Self-test call 134'), ('L', 3537, 'return TRY([&]'), ('L', 3572, '"__files"'),
    ('L', 3592, 'catch (const std::exception&)'), ('L', 3742, '"CreateLogicModsDirectory"'),
    ('L', 3747, 'try'), ('L', 3805, 'catch (const std::exception& e)'), ('L', 3808, 'lua.throw_error(e.what())'),
    ('L', 8230, 'try'), ('L', 8235, 'if (m_native_source)'), ('L', 8290, 'catch (const std::exception& e)'),
    ('L', 8299, 'try'), ('L', 8318, 'm_native_source ||'), ('L', 8342, 'catch (const std::exception& e)'),
    ('LMS', 1726, 'return TRY(lua_state'), ('LMH', 843, 'auto constexpr TRY(lua_State* lua_state'),
    ('EH', 75, 'auto constexpr TRY(CodeToTry code_to_try)'),
    ('UPH', 281, 'return m_native_context.mod_source;'), ('UPH', 299, 'queue_start_lua_mod_by_path'),
    ('BK', 24, 'ValidateNativeProgramContext'), ('BK', 53, 'start_native_mods'), ('BK', 79, 'PrepareDeferredUE4SSRuntime'),
    ('BK', 90, 'context.mod_source = MakeActivatedUE4SSModSource'), ('BK', 100, 'try'), ('BK', 111, 'throw;'),
    ('AMS', 45, 'make_shared'), ('ACK', 87, 'PrepareActivatedUE4SSRuntime'),
    ('BR', 240, 'kConfiguredGameDirectory = "/app0"'), ('BR', 1942, 'configured_game_directory'),
    ('BR', 1981, 'try'), ('BR', 1995, 'StartActivatedRuntimeWithAck'), ('BR', 2007, 'catch (const std::exception &error)'), ('BR', 2010, 'MRS_ERR_START'),
    ('BR', 2018, 'catch (const std::exception &error)'), ('BR', 2021, 'MRS_ERR_CONTEXT'),
    ('RO', 99, 'try'), ('RO', 123, 'catch (const std::exception& error)'), ('RO', 131, 'catch (...)'),
    ('NLC', 20, 'ValidateNativeProgramContext'), ('CPP', 83, 'try'), ('CPP', 89, 'FindNativeCppModModule'), ('L', 1140, 'return;'), ('L', 3417, '#endif'),
    ('CPP', 107, 'catch (const std::exception& error)'), ('SIG', 43, 'native_mod_source()'),
    ('NLB', 139, 'StageNativeLayoutFiles'), ('PF', 247, 'PosixFile::open_file'), ('PF', 386, 'persist_cache'),
    ('FD', 71, 'File::open'), ('WF', 332, 'std::filesystem::create_directories'),
    ('WF', 334, 'catch (const std::filesystem::filesystem_error& e)'), ('FM', 6, 'internal_error = true'),
    ('NPC', 131, 'is_regular_file'), ('NPC', 133, 'invalid_argument'),
    # added after the adversarial review (R0 second factory, debug-key config)
    ('NPF', 8, 'auto CreateNativeUE4SSProgram('), ('NPF', 14, 'std::make_unique<UE4SSProgram>(std::move(context),options)'),
    ('NLC', 7, 'auto MakeNativeProgramContext('), ('BK', 47, 'program_ = std::make_unique<RC::UE4SSProgram>'),
    ('BK', 102, 'std::make_unique<UE4SSBackend>(std::move(context), true, ack)'),
    ('SMH', 38, 'bool EnableDebugKeyBindings{false};'), ('INI', 13, 'EnableDebugKeyBindings=false'),
    ('L', 3356, 'if (!std::filesystem::exists(game_content_dir))'),
]

GATE_DIR = os.path.join(HERE, 'gate')
NO_CALLER_LISTS = [os.path.join(GATE_DIR, 'compiled-set-current-l26A-pass1.list'),
                   os.path.join(GATE_DIR, 'unscanned-prebuilt-deps.list')]
LINEAGE = os.path.join(WHAT, 'outputs', '2026-10-08-l26-package-A', 'lineage')
# name -> allowed (path suffix, line) mentions: declarations, the definition and
# NPF's own overload delegating to itself. Any other mention is a caller.
NO_CALLER = {
    'PrepareDeferredUE4SSRuntime': {
        ('native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp', 79),
        ('2026-10-07-l19-r3-full-build/units/ue4ss_backend.hpp', 14),
        ('ue4ss-native-tick-file-owner-1/include/ue4ss_backend.hpp', 14),
        ('2026-10-08-l26-package-A/lineage/ue4ss_backend.hpp', 14),
    },
    'CreateNativeUE4SSProgram': {
        ('ue4ss-native-tick-file-owner-1/include/NativeProgramFactory.hpp', 14),
        ('ue4ss-native-tick-file-owner-1/include/NativeProgramFactory.hpp', 18),
        ('ue4ss-native-tick-file-owner-1/src/NativeProgramFactory.cpp', 5),
        ('ue4ss-native-tick-file-owner-1/src/NativeProgramFactory.cpp', 6),
        ('ue4ss-native-tick-file-owner-1/src/NativeProgramFactory.cpp', 8),
    },
}


def no_caller_scope():
    paths = []
    for lst in NO_CALLER_LISTS:
        for ln in open(lst, errors='replace'):
            ln = ln.strip()
            if ln and not ln.startswith('#'):
                paths.append(ln if os.path.isabs(ln) else os.path.join(WHAT, ln))
    for root, _dirs, names in os.walk(LINEAGE):
        for n in names:
            paths.append(os.path.join(root, n))
    return sorted(set(paths))


def lines_of(path):
    with open(path, errors='replace') as f:
        return f.read().split('\n')


def main():
    data = json.load(open(os.path.join(HERE, 'classification.json')))
    src = data['sources']
    sites = data['sites']
    fails = []
    if not sites:
        print('CHECK-CLASSIFICATION: FAIL -- ZERO sites in classification.json')
        return 2

    # 1. coverage against the gate's own scanner + the baseline
    scanned = collections.Counter()
    scanned_keys = collections.Counter()
    files = sorted({src[s['file']] for s in sites})
    for rel in files:
        stats = dict(files=0, live_lines=0, sites=0, ec=0, host_only_matches=0)
        issues, approved, keys = [], [], []
        G.scan_file(os.path.join(WHAT, rel), rel, stats, issues, approved, keys)
        for msg, key in zip(issues, keys):
            m = re.match(r'^(.*?):(\d+): (\w+) \(', msg)
            if not m:
                fails.append(f'unparsed gate message: {msg[:120]}')
                continue
            scanned[(m.group(1), int(m.group(2)), m.group(3))] += 1
            scanned_keys[key] += 1
    classified = collections.Counter((src[s['file']], s['line'], s['fn']) for s in sites)
    baseline = collections.Counter(G.read_baseline(BASELINE))
    for k in sorted(set(classified) | set(scanned)):
        if classified[k] != scanned[k]:
            fails.append(f'coverage mismatch {k[0]}:{k[1]} {k[2]}: classified {classified[k]} vs gate scan {scanned[k]}')
    if scanned_keys != baseline:
        fails.append(f'gate keys of the classified files != baseline ({sum(scanned_keys.values())} vs {sum(baseline.values())})')
    print(f'CHECK-CLASSIFICATION: coverage: {sum(classified.values())} classified sites, '
          f'{sum(scanned.values())} gate-scanned sites in {len(files)} files, {sum(baseline.values())} baseline keys')

    # 2. verdicts
    verdicts = collections.Counter(s['verdict'] for s in sites)
    for s in sites:
        if s['verdict'] not in data['verdicts']:
            fails.append(f"unknown verdict {s['verdict']} at {s['file']}:{s['line']}")
        if s['verdict'] == 'UNHANDLED' and s.get('patch', 'none') == 'none':
            fails.append(f"UNHANDLED without a patch at {s['file']}:{s['line']}")
    print('CHECK-CLASSIFICATION: verdicts: ' + ', '.join(f'{k}={v}' for k, v in sorted(verdicts.items())))

    # 3. every X:N citation is inside its file
    text = json.dumps(data)
    cites = re.findall(r'\b([A-Z]{1,4}):(\d+)(?:-(\d+))?', text)
    cache = {}
    ncite = 0
    for key, a, b in cites:
        if key not in src:
            continue
        ncite += 1
        if key not in cache:
            cache[key] = lines_of(os.path.join(WHAT, src[key]))
        hi = int(b or a)
        if hi > len(cache[key]):
            fails.append(f'citation {key}:{a}{"-" + b if b else ""} beyond end of {src[key]} ({len(cache[key])} lines)')
    print(f'CHECK-CLASSIFICATION: citations: {ncite} file:line references bounds-checked (line exists) in {len(cache)} files')

    # 4. anchors
    nanch = 0
    for key, ln, needle in ANCHORS:
        nanch += 1
        if key not in cache:
            cache[key] = lines_of(os.path.join(WHAT, src[key]))
        got = cache[key][ln - 1] if ln - 1 < len(cache[key]) else ''
        if needle not in got:
            fails.append(f'anchor {key}:{ln} expected {needle!r}, found {got.strip()[:100]!r}')
    print(f'CHECK-CLASSIFICATION: anchors: {nanch} load-bearing lines checked for their expected code text')

    # 5. no caller for the two no-mod-source factories (R0)
    nfiles = 0
    seen = collections.Counter()
    for path in no_caller_scope():
        if not os.path.isfile(path):
            continue
        nfiles += 1
        try:
            body = open(path, errors='replace').read()
        except OSError as e:
            fails.append(f'no-caller: cannot read {path}: {e}')
            continue
        if not any(n in body for n in NO_CALLER):
            continue
        for i, line in enumerate(body.split('\n'), 1):
            for name, allowed in NO_CALLER.items():
                if re.search(r'\b' + name + r'\b', line):
                    if any(path.endswith(sfx) and i == al for sfx, al in allowed):
                        seen[name] += 1
                    else:
                        fails.append(f'no-caller: unexpected mention of {name} at {path}:{i}: {line.strip()[:100]}')
    for name, allowed in NO_CALLER.items():
        if seen[name] != len(allowed):
            fails.append(f'no-caller: {name}: expected {len(allowed)} known mentions, found {seen[name]}')
    print(f'CHECK-CLASSIFICATION: no-caller: {nfiles} files scanned for {len(NO_CALLER)} factory names; '
          + ', '.join(f'{n}: {seen[n]} known mentions' for n in NO_CALLER))

    for f in fails:
        print('  FAIL', f)
    if not ncite or not nanch or not nfiles:
        print('CHECK-CLASSIFICATION: FAIL -- ZERO citations, anchors or no-caller files checked')
        return 2
    print(f'CHECK-CLASSIFICATION: {"PASS" if not fails else "FAIL"} ({len(fails)} problems)')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())

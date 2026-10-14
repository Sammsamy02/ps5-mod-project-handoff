#!/usr/bin/env python3
"""Bounded independent review; writes only this new review directory."""
from pathlib import Path
import hashlib, json, os, re, shutil, subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SEALED = ROOT / 'outputs/2026-10-08-menu-path-audit'
THUNK = SEALED / 'thunk-design'

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for part in iter(lambda: f.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()

def save(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + '\n')

selection = json.loads((SEALED / 'SOURCE-SELECTION.json').read_text())
pin_rows = []
for rel, expected in selection['files'].items():
    path = SEALED / rel
    actual = sha(path)
    pin_rows.append({'path': str(path), 'expected': expected, 'actual': actual, 'pass': actual == expected})
for row in selection['external_evidence']:
    actual = sha(row['path'])
    pin_rows.append({'path': row['path'], 'expected': row['sha256'], 'actual': actual, 'pass': actual == row['sha256']})
assert all(x['pass'] for x in pin_rows), 'Frozen source/evidence pin drift'
save('SOURCE-PINS.json', {'pass': True, 'source_selection_sha256': sha(SEALED / 'SOURCE-SELECTION.json'),
                         'final_receipt_sha256': sha(SEALED / 'FINAL-RECEIPT.json'), 'rows': pin_rows})

def namespace(path):
    text = path.read_text()
    return text[text.index('namespace RC::Unreal'):]

header = (SEALED / 'source-cache/UFunctionStructs.hpp').read_text()
header = '\n'.join(x for x in header.splitlines() if not x.startswith('#include <Unreal/'))
helper = (THUNK / 'candidate/ScriptHookThunks.hpp').read_text()
helper = '\n'.join(x for x in helper.splitlines() if not x.startswith('#include <Unreal/'))
uf = (THUNK / 'candidate/UFunction.cpp').read_text()
install = uf[uf.index('    auto UFunction::GetFunctionHookData()'):uf.index('    auto UFunction::RegisterPreHookForInstance')]
retire = uf[uf.index('    auto UFunction::UnregisterAllHooks()'):uf.index('    auto UFunction::GetFunctionHookCallbackData')]
prefix = '#include "../green_host_model.hpp"\n#include <array>\n#include <mutex>\n#include <algorithm>\n' + header + '\n' + helper + '\n' + namespace(THUNK / 'candidate/UFunctionStructs.cpp') + '\nnamespace RC::Unreal {\n' + install + retire + '\n}\n' + namespace(THUNK / 'candidate/ScriptHookThunks.cpp') + '\n'
frozen_unit = (THUNK / 'green-v3/actual_candidate.cpp').read_text()
assert frozen_unit.split('#include "../green_cases.cpp"')[0] == prefix, 'Actual-body extraction differs'
prefix = prefix.replace('#include "../green_host_model.hpp"', '#include "' + str(THUNK / 'green_host_model.hpp') + '"')

cases = r'''
#include <memory>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame&, void*) { ++native_calls; trace.emplace_back("original"); }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    UFunction function; function.func = original;
    UObject context, other; FFrame frame;
    std::string scenario = argv[1];
    if (scenario == "phase_snapshots") {
        bool added = false;
        function.RegisterPreHook([&](UnrealScriptFunctionCallableContext&, void*) {
            trace.emplace_back("pre");
            if (!added) {
                added = true;
                function.RegisterPreHook([](UnrealScriptFunctionCallableContext&, void*) { trace.emplace_back("new-pre"); }, nullptr);
                function.RegisterPostHook([](UnrealScriptFunctionCallableContext&, void*) { trace.emplace_back("new-post"); }, nullptr);
            }
        }, nullptr);
        auto thunk = function.func;
        thunk(&context, frame, nullptr);
        if (trace != std::vector<std::string>{"pre", "original", "new-post"}) return 1;
        trace.clear();
        thunk(&context, frame, nullptr);
        if (trace != std::vector<std::string>{"pre", "new-pre", "original", "new-post"}) return 1;
        std::cout << "PASS phase snapshot: pre-add-post same dispatch; pre-add-pre next Pre invocation\n";
    } else if (scenario == "retired_filtered_capture") {
        auto& data = function.GetFunctionHookData();
        auto token = std::make_shared<int>(42); std::weak_ptr<int> weak = token;
        int filtered_invocations = 0;
        auto id = data.AddPreCallback([token, &filtered_invocations](UnrealScriptFunctionCallableContext&, void*) { ++filtered_invocations; }, nullptr, &other);
        function.RegisterPreHook([&](UnrealScriptFunctionCallableContext&, void*) { function.UnregisterAllHooks(); }, nullptr);
        token.reset();
        auto thunk = function.func;
        thunk(&context, frame, nullptr);
        auto* retained = data.GetCallbackData(id);
        if (!retained || !retained->UnregistrationRequested || data.bIsMidExecution || weak.expired() || filtered_invocations || native_calls != 1 || Internal::GetHookedFunctionsMap().contains(&function)) return 1;
        std::cout << "OBSERVED filtered callback capture retained after active count zero and retirement\n";
        data.RemoveAllCallbacks();
        if (!weak.expired()) return 1;
        std::cout << "PASS quiescent explicit retained-data cleanup releases capture; thunk node remains\n";
    } else return 2;
}
'''
race = r'''
#include <thread>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame&, void*) { ++native_calls; }
int main() {
    UFunction function; function.func = original; function.GetFunctionHookData();
    UObject context; FFrame frame; std::atomic<bool> go{};
    std::thread writer([&] {
        while (!go.load()) {}
        for (int i = 0; i < 6; ++i) { function.UnregisterAllHooks(); function.GetFunctionHookData(); }
    });
    go.store(true);
    for (int i = 0; i < 100000; ++i) function.GetFuncPtr()(&context, frame, nullptr);
    writer.join();
    return native_calls == 100000 ? 0 : 1;
}
'''
(HERE / 'cases.cpp').write_text(cases)
(HERE / 'same_field_race.cpp').write_text(race)
compiler = shutil.which('clang++')
compiler_resolved = str(Path(compiler).resolve())
rows = []
for kind, body, sanitizer in [('policy', cases, 'address,undefined'), ('publication', race, 'thread')]:
    source = HERE / (kind + '-actual.cpp'); source.write_text(prefix + body)
    binary = HERE / (kind + '-tests')
    argv = [compiler, '-std=c++20', '-O1', '-g', '-fsanitize=' + sanitizer, '-fno-omit-frame-pointer', '-pthread', str(source), '-o', str(binary)]
    compile_result = subprocess.run(argv, capture_output=True, text=True, timeout=60)
    (HERE / (kind + '-compile.stdout')).write_text(compile_result.stdout)
    (HERE / (kind + '-compile.stderr')).write_text(compile_result.stderr)
    assert compile_result.returncode == 0, compile_result.stderr
    scenarios = ['phase_snapshots', 'retired_filtered_capture'] if kind == 'policy' else ['same_field_publication']
    for scenario in scenarios:
        command = [str(binary)] + ([scenario] if kind == 'policy' else [])
        env = dict(os.environ, ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1', TSAN_OPTIONS='halt_on_error=1:exitcode=66')
        result = subprocess.run(command, capture_output=True, text=True, env=env, timeout=30)
        (HERE / (scenario + '.stdout')).write_text(result.stdout)
        (HERE / (scenario + '.stderr')).write_text(result.stderr)
        race_seen = 'ThreadSanitizer: data race' in result.stderr
        expected = result.returncode == 0 if kind == 'policy' else race_seen
        rows.append({'scenario': scenario, 'scope': 'Exact prototype bodies with explicit host engine/storage seams', 'command': command, 'compile_command': argv, 'compile_returncode': compile_result.returncode, 'returncode': result.returncode, 'expected_observation': 'race on raw same-UFunction Func read/store outside registry mutex' if kind == 'publication' else 'documented actual behavior', 'thread_race_detected': race_seen, 'pass': expected, 'source_sha256': sha(source), 'binary_sha256': sha(binary), 'stdout_sha256': sha(HERE / (scenario + '.stdout')), 'stderr_sha256': sha(HERE / (scenario + '.stderr'))})
save('CONTROLS.json', {'pass': all(x['pass'] for x in rows), 'scope': 'Prototype policy/retained capture observations plus an intentionally unsupported same-field publication race; no target/game execution', 'compiler': compiler, 'compiler_resolved': compiler_resolved, 'compiler_sha256': sha(compiler_resolved), 'host_seam_sha256': sha(THUNK / 'green_host_model.hpp'), 'exact_saved_candidate_body_prefix_verified': True, 'results': rows})
assert all(x['pass'] for x in rows), rows

symbols = json.loads((THUNK / 'target/SYMBOL-RECEIPT.json').read_text())
tool = symbols['tool']
assert sha(tool) == symbols['tool_sha256']
captures = []
sets = []
for index, object_row in enumerate(symbols['objects']):
    obj = Path(object_row['path'])
    assert sha(obj) == object_row['sha256']
    command = object_row['command']
    llvm_config = json.loads((THUNK / 'target/TARGET-RECEIPT.json').read_text())['results'][0]['LLVM_CONFIG']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=30, env=dict(os.environ, LLVM_CONFIG=llvm_config))
    assert result.returncode == 0
    target = HERE / ('nm-' + str(index) + '.txt'); target.write_bytes(result.stdout)
    assert sha(target) == object_row['nm_sha256'], 'Fresh nm differs from sealed capture'
    assert sha(obj) == object_row['sha256']
    parsed = []
    for line in result.stdout.decode().splitlines():
        match = re.match(r'^\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z?])\s+(\S+)$', line)
        if match:
            parsed.append((match.group(2), match.group(1)))
    assert parsed
    sets.append(set(parsed))
    captures.append({'object': str(obj), 'object_sha256': sha(obj), 'command': command, 'LLVM_CONFIG': llvm_config, 'LLVM_CONFIG_sha256': sha(llvm_config), 'returncode': result.returncode, 'stdout_sha256': sha(target), 'sealed_capture_exact': True})
def external(objects):
    definitions = {name for obj in objects for name, kind in obj if kind.upper() != 'U'}
    undefined = {name for obj in objects for name, kind in obj if kind.upper() == 'U'}
    return undefined - definitions
baseline = external(sets[:2]); candidate = external(sets[2:])
added = sorted(candidate - baseline); removed = sorted(baseline - candidate)
assert added == symbols['new_external_undefined_vs_two_baselines']
assert removed == symbols['removed_external_undefined_vs_two_baselines']
private = sorted(name for name in candidate if 'ThunkDetail' in name)
assert not private
helper_checks = {name: sum((name, 'T') in obj for obj in sets[2:]) for name in symbols['helper_providers']}
assert all(count == 1 for count in helper_checks.values())
thunks = sorted(name for name, kind in sets[4] if '5ThunkILm' in name and kind == 't')
assert thunks == symbols['static_thunk_symbols']
save('TARGET-SYMBOL-AUDIT.json', {'pass': True, 'scope': 'Fresh SDK nm captures of three candidate and two baseline objects; not a full link/import admission', 'tool': tool, 'tool_sha256': sha(tool), 'captures': captures, 'helper_strong_provider_counts': helper_checks, 'thunk_count': len(thunks), 'unresolved_private_helpers': private, 'added_external_undefined_vs_two_baselines': added, 'removed_external_undefined_vs_two_baselines': removed})
print(json.dumps({'frozen_pins': len(pin_rows), 'bounded_controls': len(rows), 'expected_publication_race': rows[-1]['thread_race_detected'], 'target_objects': len(captures), 'added_standard_symbols': len(added)}))

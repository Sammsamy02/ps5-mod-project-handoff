#!/usr/bin/env python3
"""Link and call the genuine Mono dispatch function during the real owner lifetime."""
from pathlib import Path
import hashlib, json, os, resource, subprocess

BASE = Path(__file__).resolve().parents[1]
TEAM = BASE.parent
CAP = TEAM / 'cap10'
WORK = TEAM / 'cap10-mono-build/runtime-work'
STATIC = TEAM / 'cap10-static-corlib-host'
MDK = TEAM / 'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def main():
    receipt = BASE / 'evidence/dispatch-v2.json'
    assert not receipt.exists(), 'Preserve completed evidence.'
    runtime = json.loads((BASE / 'evidence/runtime-build-v2.json').read_text())
    objects = BASE / 'objects/dispatch-v2'
    objects.mkdir(parents=True, exist_ok=True)
    prefix = (CAP / 'host-check/MonoHarness.cpp').read_text().split('static void need(', 1)[0]
    prefix = 'static void test_direct_dispatch_once();\n' + prefix
    prefix = prefix.replace('        active = true; kind = requested; cookie = expected;', '        active = true; kind = requested; cookie = expected;\n        if (requested == WindowKind::RunImports) test_direct_dispatch_once();')
    harness = objects / 'DispatchHarness.cpp'
    harness.write_text(prefix + (BASE / 'tests/DispatchHarnessTail.cpp').read_text())
    archives = [BASE / 'runtime-host-v2/libmini.a', *[WORK / path for path in ['mono/mini/.libs/libmono-ee-interp.a', 'mono/mini/.libs/libmono-dbg.a', 'mono/metadata/.libs/libmonoruntimesgen.a', 'mono/sgen/.libs/libmonosgen.a', 'mono/utils/.libs/libmonoutils.a', 'mono/eglib/.libs/libeglib.a']]]
    assert sha(archives[0]) == runtime['outputs']['host']['archive_sha256']
    assert all(sha(path) == runtime['inputs_sha256'][str(path)] for path in archives[1:])
    corlib = STATIC / 'owner-runtime/lib/mono/4.5/mscorlib.dll'
    aot = STATIC / 'aot-build/mscorlib.dll.o'
    aot_receipt = json.loads((STATIC / 'evidence/STATIC-CORLIB-BUILD.json').read_text())
    assert sha(corlib) == aot_receipt['corlib_sha256'] and sha(aot) == aot_receipt['object_sha256']
    sources = [BASE / 'native/MonoOwner.cpp', CAP / 'native/InvocationBoundary.cpp', harness]
    native = CAP / 'host-check/native-mono-test.c'
    inputs = {str(path): sha(path) for path in [*sources, native, *archives, aot, corlib, BASE / 'native/MonoOwner.hpp', CAP / 'native/ProbeImage.hpp', BASE / 'tests/MonoGcRegionApi.hpp', WORK / 'config.h', WORK / 'mono/utils/mono-signal-handler.h']}
    env = {key: value for key, value in os.environ.items() if not key.startswith(('MONO_', 'DYLD_'))}
    env.update(MONO_CFG_DIR=str(MDK / 'etc'), MONO_CRASH_NOFILE='1', TMPDIR=str(BASE / 'tmp'))
    commands = []
    log = BASE / 'logs/dispatch-v2-build.log'
    log.write_text('')
    def run(command):
        result = subprocess.run(command, cwd=BASE, env=env, capture_output=True, text=True)
        commands.append({'command': command, 'exit_code': result.returncode})
        with log.open('a') as output: output.write(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    flags = ['/usr/bin/clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-c', '-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1', '-DMODBRIDGE_CAP10_STATIC_CORLIB=1', '-DMODBRIDGE_CAP10_CORLIB_DIR=' + json.dumps(str(STATIC / 'owner-runtime/lib')), '-DMODBRIDGE_CAP10_CONFIG_DIR=' + json.dumps(str(MDK / 'etc')), '-I' + str(BASE / 'native'), '-I' + str(BASE / 'tests'), '-I' + str(CAP / 'native'), '-I' + str(WORK), '-I' + str(WORK / 'mono/eglib')]
    for source in sources: run(flags + [str(source), '-o', str(objects / (source.stem + '.o'))])
    run(['/usr/bin/clang', '-arch', 'x86_64', '-O1', '-Wall', '-Wextra', '-Werror', '-c', str(native), '-o', str(objects / 'native-observer.o')])
    exe = BASE / 'bin/dispatch-v2'
    run(['/usr/bin/clang++', '-arch', 'x86_64', *[str(objects / (source.stem + '.o')) for source in sources], str(objects / 'native-observer.o'), str(aot), *['-Wl,-force_load,' + str(path) for path in archives], '-framework', 'CoreFoundation', '-framework', 'Foundation', '-lz', '-lm', '-liconv', '-lpthread', '-o', str(exe)])
    deps = run(['/usr/bin/otool', '-L', str(exe)])
    (BASE / 'logs/dispatch-v2-dependencies.log').write_text(deps)
    assert 'libmonosgen' not in deps and 'mscorlib.dll.dylib' not in deps
    symbols = run(['/usr/bin/nm', '-m', str(exe)])
    relevant = '\n'.join(line for line in symbols.splitlines() if 'mono_chain_signal' in line)
    (BASE / 'logs/dispatch-v2-symbol.log').write_text(relevant + '\n')
    assert '_mono_chain_signal' in relevant and '(undefined)' not in relevant
    env.update(MONO_LOG_LEVEL='debug', MONO_LOG_MASK='aot,dll', DYLD_PRINT_LIBRARIES='1')
    result = subprocess.run([str(exe)], cwd=corlib.parent, env=env, text=True, capture_output=True, timeout=60, preexec_fn=no_core)
    (BASE / 'logs/dispatch-v2.stdout.log').write_text(result.stdout)
    (BASE / 'logs/dispatch-v2.stderr.log').write_text(result.stderr)
    parsed = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{"verdict":')]
    proof = parsed[-1] if parsed else {'verdict': 'MISSING_RESULT'}
    combined = result.stdout + result.stderr
    proof.update(exit_code=result.returncode, commands=commands, input_sha256=inputs, inputs_unchanged=all(sha(Path(path)) == digest for path, digest in inputs.items()), executable_sha256=sha(exe), linked_genuine_hidden_runtime_symbol=relevant, static_runtime_archives=True, dynamic_mono_runtime_dependency=False, static_corlib_trace_observed="Found statically linked AOT module 'mscorlib'." in result.stdout, runtime_diagnostics_clean=all(text not in combined for text in ["assertion '", 'Assertion at ', 'Native Crash Reporting']), max_compile_jobs=1, target_execution=False, prx_build=False)
    receipt.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({key: value for key, value in proof.items() if key not in ['commands', 'input_sha256']}, indent=2))
    assert proof['verdict'] == 'GENUINE_SIGNAL_DISPATCH_PASS' and proof['exit_code'] == 0 and proof['inputs_unchanged'] and proof['static_corlib_trace_observed'] and proof['runtime_diagnostics_clean']
    assert proof['real_managed_runs'] == 2 and proof['real_managed_exceptions'] == 1

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""One sequential build, private real Mono owner process, durable before/after receipts."""
from pathlib import Path
import argparse, hashlib, json, os, resource, subprocess

BASE = Path(__file__).resolve().parents[1]
TEAM = BASE.parent
CAP = TEAM / 'cap10'
STRICT = TEAM / 'cap10-mono-build'
WORK = STRICT / 'runtime-work'
STATIC = TEAM / 'cap10-static-corlib-host'
MDK = TEAM / 'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('label', choices=['baseline', 'chaining', 'runtime-fixed', 'fixed', 'pipe-replaced', 'default-pipe', 'ignore-pipe', 'fixed-v2', 'pipe-replaced-v2', 'default-pipe-v2', 'ignore-pipe-v2'])
    args = parser.parse_args()
    label = args.label
    case = label.removesuffix('-v2')
    runtime_folder = 'runtime-host-v2' if label.endswith('-v2') else 'runtime-host'
    receipt = BASE / 'evidence' / (label + '.json')
    assert not receipt.exists(), 'Evidence is append-only; use a new documented test case.'
    owner = CAP / 'native/MonoOwner.cpp' if label == 'baseline' else BASE / 'native/MonoOwner.cpp'
    if label == 'runtime-fixed': owner = BASE / 'native/MonoOwner.chaining.cpp'
    lib = WORK / 'mono/mini/.libs/libmonosgen-2.0.1.dylib' if label in ['baseline', 'chaining'] else BASE / runtime_folder / 'libmonosgen-2.0.1.dylib'
    corlib = STATIC / 'owner-runtime/lib/mono/4.5/mscorlib.dll'
    aot = STATIC / 'aot-build/mscorlib.dll.o'
    aot_receipt = json.loads((STATIC / 'evidence/STATIC-CORLIB-BUILD.json').read_text())
    assert sha(aot) == aot_receipt['object_sha256'] and sha(corlib) == aot_receipt['corlib_sha256']
    objects = BASE / 'objects' / label
    objects.mkdir(parents=True, exist_ok=True)
    forwarding = BASE / 'tests/MonoGcRegionApi.hpp'
    forwarding.write_text('#pragma once\n#include "config.h"\n#include <mono/utils/mono-threads-api.h>\n')
    prefix = (CAP / 'host-check/MonoHarness.cpp').read_text().split('static void need(', 1)[0]
    prefix = '#include <csignal>\nstatic void test_replace_pipe_once();\n' + prefix
    prefix = prefix.replace('        active = true; kind = requested; cookie = expected;', '        active = true; kind = requested; cookie = expected;\n        if (requested == WindowKind::RunImports) test_replace_pipe_once();')
    tail = (BASE / 'tests/SignalHarnessTail.cpp').read_text()
    harness = objects / 'SignalHarness.cpp'
    harness.write_text(prefix + tail)
    source_inputs = [owner, BASE / 'native/MonoOwner.hpp', CAP / 'native/InvocationBoundary.cpp', CAP / 'host-check/native-mono-test.c', CAP / 'native/ProbeImage.hpp', CAP / 'native/MonoOwner.hpp', CAP / 'native/InvocationBoundary.hpp', CAP / 'native/Cap10InvocationObservation.hpp', harness, forwarding, lib, aot, corlib, WORK / 'config.h', WORK / 'mono/eglib/eglib-config.h', MDK / 'etc/mono/config']
    inputs = {str(path): sha(path) for path in source_inputs}
    env = {key: value for key, value in os.environ.items() if not key.startswith(('MONO_', 'DYLD_'))}
    env.update(MONO_CFG_DIR=str(MDK / 'etc'), MONO_CRASH_NOFILE='1', TMPDIR=str(BASE / 'tmp'))
    commands = []
    compile_log = BASE / 'logs' / (label + '-build.log')
    compile_log.write_text('')
    def run_build(command):
        result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
        commands.append({'command': command, 'exit_code': result.returncode})
        with compile_log.open('a') as output:
            output.write(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    flags = ['/usr/bin/clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-c', '-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1', '-DMODBRIDGE_CAP10_STATIC_CORLIB=1', '-DMODBRIDGE_CAP10_CORLIB_DIR=' + json.dumps(str(STATIC / 'owner-runtime/lib')), '-DMODBRIDGE_CAP10_CONFIG_DIR=' + json.dumps(str(MDK / 'etc')), '-I' + str(BASE / 'tests'), '-I' + str(BASE / 'native'), '-I' + str(CAP / 'native'), '-I' + str(WORK), '-I' + str(WORK / 'mono/eglib')]
    if case == 'pipe-replaced': flags.append('-DCAP10_TEST_REPLACE_PIPE=1')
    if case == 'default-pipe': flags.append('-DCAP10_TEST_DEFAULT_PIPE=1')
    if case == 'ignore-pipe': flags.append('-DCAP10_TEST_IGNORE_PIPE=1')
    for name, source in [('MonoOwner', owner), ('InvocationBoundary', CAP / 'native/InvocationBoundary.cpp'), ('SignalHarness', harness)]:
        run_build(flags + [str(source), '-o', str(objects / (name + '.o'))])
    run_build(['/usr/bin/clang', '-arch', 'x86_64', '-O1', '-Wall', '-Wextra', '-Werror', '-c', str(CAP / 'host-check/native-mono-test.c'), '-o', str(objects / 'native-observer.o')])
    exe = BASE / 'bin' / label
    run_build(['/usr/bin/clang++', '-arch', 'x86_64', *[str(objects / (name + '.o')) for name in ['MonoOwner', 'InvocationBoundary', 'SignalHarness', 'native-observer']], str(aot), str(lib), '-Wl,-headerpad_max_install_names', '-o', str(exe)])
    if label in ['baseline', 'chaining']:
        run_build(['/usr/bin/install_name_tool', '-change', str(STRICT / 'runtime-install/lib/libmonosgen-2.0.1.dylib'), str(lib), str(exe)])
    dependencies = run_build(['/usr/bin/otool', '-L', str(exe)])
    (BASE / 'logs' / (label + '-dependencies.log')).write_text(dependencies)
    assert str(lib) in dependencies and 'mscorlib.dll.dylib' not in dependencies
    env.update(MONO_LOG_LEVEL='debug', MONO_LOG_MASK='aot,dll', DYLD_PRINT_LIBRARIES='1')
    result = subprocess.run([str(exe)], cwd=corlib.parent, env=env, text=True, capture_output=True, timeout=60, preexec_fn=no_core)
    (BASE / 'logs' / (label + '.stdout.log')).write_text(result.stdout)
    (BASE / 'logs' / (label + '.stderr.log')).write_text(result.stderr)
    parsed = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{"verdict":')]
    proof = parsed[-1] if parsed else {'verdict': 'MISSING_RESULT'}
    combined = result.stdout + result.stderr
    proof.update(label=label, exit_code=result.returncode, commands=commands, inputs_sha256=inputs, frozen_inputs_unchanged=all(sha(Path(path)) == value for path, value in inputs.items()), executable_sha256=sha(exe), static_corlib_trace_observed="Found statically linked AOT module 'mscorlib'." in result.stdout, runtime_diagnostics_clean=all(text not in combined for text in ["assertion '", 'Assertion at ', 'Native Crash Reporting']), max_compile_jobs=1, prx_build=False, run_command=[str(exe)], cwd=str(corlib.parent), run_environment={key: value for key, value in env.items() if key.startswith(('MONO_', 'DYLD_')) or key == 'TMPDIR'})
    receipt.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({key: value for key, value in proof.items() if key not in ['commands', 'inputs_sha256', 'run_environment']}, indent=2))
    assert proof.get('completed') == (case != 'pipe-replaced') and proof.get('run_passed') and proof.get('throw_passed') and proof.get('recovery_passed') and proof.get('real_managed_runs') == 2 and proof.get('real_managed_exceptions') == 1
    assert proof['static_corlib_trace_observed'] and proof['runtime_diagnostics_clean'] and proof['frozen_inputs_unchanged']
    assert result.returncode == (1 if label in ['baseline', 'chaining', 'runtime-fixed'] else 0)

if __name__ == '__main__':
    main()

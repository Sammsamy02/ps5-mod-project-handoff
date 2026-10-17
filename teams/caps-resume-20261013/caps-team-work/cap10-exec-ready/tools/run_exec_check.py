#!/usr/bin/env python3
"""One genuine final-owner lifecycle with forwarded Darwin memory-call tracing."""
from pathlib import Path
import hashlib
import json
import os
import re
import resource
import subprocess

HERE = Path(__file__).resolve().parents[1]
TEAM = HERE.parent
SIGNAL = TEAM / 'cap10-signal-ready'
CAP = TEAM / 'cap10'
WORK = TEAM / 'cap10-mono-build/runtime-work'
STATIC = TEAM / 'cap10-static-corlib-host'
MDK = TEAM / 'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
runtime = SIGNAL / 'runtime-host-v2/libmonosgen-2.0.1.dylib'
owner = SIGNAL / 'native/MonoOwner.cpp'
corlib = STATIC / 'owner-runtime/lib/mono/4.5/mscorlib.dll'
aot = STATIC / 'aot-build/mscorlib.dll.o'
tracer = HERE / 'bin/libExecTrace.dylib'
exe = HERE / 'bin/exec-check'
receipt_path = HERE / 'evidence/EXEC-HOST-CHECK.json'
if receipt_path.exists():
    raise SystemExit('Preserve the measured process evidence; do not repeat unchanged.')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


runtime_receipt = json.loads((SIGNAL / 'evidence/runtime-build-v2.json').read_text())
assert sha(runtime) == runtime_receipt['outputs']['host']['dylib_sha256']
assert sha(owner) == '03edcc27dced6d2a1fa03162bbe3fbd74345e9d2207653688422a78d086c0bad'
aot_receipt = json.loads((STATIC / 'evidence/STATIC-CORLIB-BUILD.json').read_text())
assert sha(aot) == aot_receipt['object_sha256'] and sha(corlib) == aot_receipt['corlib_sha256']
prefix = '#include <csignal>\nstatic void test_replace_pipe_once();\n' + (CAP / 'host-check/MonoHarness.cpp').read_text().split('static void need(', 1)[0]
prefix = prefix.replace('        active = true; kind = requested; cookie = expected;',
                        '        active = true; kind = requested; cookie = expected;\n        if (requested == WindowKind::RunImports) test_replace_pipe_once();')
tail = (SIGNAL / 'tests/SignalHarnessTail.cpp').read_text()
needle = '    const auto result = RC::Cap10Host::run_embedded_probe_once();'
assert tail.count(needle) == 1
tail = '#include "ExecControls.hpp"\n' + tail.replace(needle,
    '    if (!check_exec_trace_controls()) return 95;\n'
    '    cap10_exec_start();\n' + needle + '\n'
    '    cap10_exec_stop();\n    cap10_exec_report();')
harness = HERE / 'native/ExecHarness.cpp'
harness.write_text(prefix + tail)
sources = [owner, CAP / 'native/InvocationBoundary.cpp', harness]
provider = CAP / 'host-check/native-mono-test.c'
source_inputs = [*sources, provider, runtime, aot, corlib, SIGNAL / 'native/MonoOwner.hpp',
                 SIGNAL / 'tests/MonoGcRegionApi.hpp', SIGNAL / 'tests/SignalHarnessTail.cpp',
                 CAP / 'native/ProbeImage.hpp', CAP / 'native/InvocationBoundary.hpp', CAP / 'native/Cap10InvocationObservation.hpp',
                 WORK / 'config.h', WORK / 'mono/eglib/eglib-config.h', MDK / 'etc/mono/config',
                 HERE / 'native/ExecTrace.c', HERE / 'native/ExecTrace.h', HERE / 'native/ExecControls.hpp']
inputs = {str(path): sha(path) for path in source_inputs}
env = {key: value for key, value in os.environ.items() if not key.startswith(('MONO_', 'DYLD_'))}
env.update(MONO_CFG_DIR=str(MDK / 'etc'), MONO_CRASH_NOFILE='1', TMPDIR=str(HERE / 'tmp'))
receipt = dict(verdict='INCOMPLETE', input_sha256=inputs, commands=[], host_only=True,
               target_execution=False, game_or_PRX_build=False, max_compile_jobs=1, measured_owner_runs=0)


def save():
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')


def run_build(label, command):
    result = subprocess.run(command, cwd=HERE, env=env, text=True, capture_output=True)
    log = HERE / 'logs' / (label + '.log')
    log.write_text(result.stdout + result.stderr)
    receipt['commands'].append(dict(label=label, command=command, exit_code=result.returncode, log_sha256=sha(log)))
    save()
    if result.returncode:
        raise SystemExit('Build failed: ' + label)
    return result.stdout


run_build('tracer-build', ['/usr/bin/clang', '-arch', 'x86_64', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                         '-dynamiclib', str(HERE / 'native/ExecTrace.c'), '-install_name', str(tracer), '-o', str(tracer)])
flags = ['/usr/bin/clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-c',
         '-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1', '-DMODBRIDGE_CAP10_STATIC_CORLIB=1',
         '-DMODBRIDGE_CAP10_CORLIB_DIR=' + json.dumps(str(STATIC / 'owner-runtime/lib')),
         '-DMODBRIDGE_CAP10_CONFIG_DIR=' + json.dumps(str(MDK / 'etc')),
         '-I' + str(HERE / 'native'), '-I' + str(SIGNAL / 'native'), '-I' + str(SIGNAL / 'tests'),
         '-I' + str(CAP / 'native'), '-I' + str(WORK), '-I' + str(WORK / 'mono/eglib')]
objects = []
for source in sources:
    obj = HERE / 'objects' / (source.stem + '.o')
    run_build(source.stem + '-compile', flags + [str(source), '-o', str(obj)])
    objects.append(obj)
obj = HERE / 'objects/native-observer.o'
run_build('native-observer-compile', ['/usr/bin/clang', '-arch', 'x86_64', '-O1', '-Wall', '-Wextra', '-Werror', '-c', str(provider), '-o', str(obj)])
objects.append(obj)
run_build('owner-link', ['/usr/bin/clang++', '-arch', 'x86_64', *map(str, objects), str(aot), str(runtime), str(tracer), '-o', str(exe)])
dependencies = run_build('owner-dependencies', ['/usr/bin/otool', '-L', str(exe)])
imports = run_build('runtime-undefined-symbols', ['/usr/bin/nm', '-u', str(runtime)])
memory_imports = sorted(set(re.findall(r'^(_\w*(?:mmap|mprotect|vm_allocate|vm_map|vm_protect|vm_remap)\w*)$', imports, re.M)))
assert memory_imports == ['_mmap', '_mprotect'], memory_imports
assert str(runtime) in dependencies and str(tracer) in dependencies and 'mscorlib.dll.dylib' not in dependencies
interpose_sections = run_build('tracer-sections', ['/usr/bin/otool', '-l', str(tracer)])
assert '__interpose' in interpose_sections
assert all(sha(Path(path)) == value for path, value in inputs.items()), 'Input drift before measured process'
env.update(MONO_LOG_LEVEL='debug', MONO_LOG_MASK='aot,dll', DYLD_PRINT_LIBRARIES='1', DYLD_INSERT_LIBRARIES=str(tracer))


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


result = subprocess.run([str(exe)], cwd=corlib.parent, env=env, text=True, capture_output=True, timeout=60, preexec_fn=no_core)
(HERE / 'logs/exec-check.stdout.log').write_text(result.stdout)
(HERE / 'logs/exec-check.stderr.log').write_text(result.stderr)
parsed = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
control = next((item for item in parsed if item.get('tracer_control')), {})
trace = next((item for item in parsed if item.get('exec_trace')), {})
owner_result = next((item for item in parsed if item.get('verdict') == 'SIGNAL_LIFECYCLE_PASS'), {})
events = [line for line in result.stdout.splitlines() if line.startswith('EXEC_EVENT ')]
receipt.update(exit_code=result.returncode, tracer_control=control, owner_memory_trace=trace,
               owner_result=owner_result, recorded_events=events, measured_owner_runs=1 if trace else 0,
               runtime_memory_imports=memory_imports, runtime_has_active_mach_memory_imports=False,
               source_runtime_dylib_sha256=sha(runtime), tracer_sha256=sha(tracer), executable_sha256=sha(exe),
               run_command=[str(exe)], cwd=str(corlib.parent),
               run_environment={key: value for key, value in env.items() if key.startswith(('MONO_', 'DYLD_')) or key == 'TMPDIR'},
               inputs_unchanged=all(sha(Path(path)) == value for path, value in inputs.items()),
               static_corlib_trace_observed="Found statically linked AOT module 'mscorlib'." in result.stdout,
               runtime_diagnostics_clean=all(text not in result.stdout + result.stderr for text in ["assertion '", 'Assertion at ', 'Native Crash Reporting']))
save()
assert result.returncode == 0 and control.get('passed') and trace.get('dropped_events') == 0
assert owner_result.get('completed') and owner_result.get('run_passed') and owner_result.get('throw_passed') and owner_result.get('recovery_passed')
assert owner_result.get('real_managed_runs') == 2 and owner_result.get('real_managed_exceptions') == 1
assert receipt['inputs_unchanged'] and receipt['static_corlib_trace_observed'] and receipt['runtime_diagnostics_clean']
receipt['verdict'] = 'GENUINE_FINAL_OWNER_EXECUTABLE_MEMORY_OBSERVATION_COMPLETE'
save()
print(json.dumps({key: receipt[key] for key in ['verdict', 'exit_code', 'tracer_control', 'owner_memory_trace', 'runtime_memory_imports', 'inputs_unchanged']}, indent=2))

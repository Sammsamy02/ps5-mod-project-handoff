#!/usr/bin/env python3
"""Run the updated production owner as sole static corlib registration path once."""
from pathlib import Path
import hashlib, json, os, resource, subprocess

BASE = Path(__file__).resolve().parents[1]
FROZEN = BASE.parent / 'cap10-mono-build'
WORK = FROZEN / 'runtime-work'
CAP = BASE.parent / 'cap10'
MDK = BASE.parent / 'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def main():
    receipt = BASE / 'evidence/PRODUCTION-STATIC-OWNER-CHECK.json'
    assert not receipt.exists(), 'Preserve evidence: do not repeat the completed owner run.'
    aot = json.loads((BASE / 'evidence/STATIC-CORLIB-BUILD.json').read_text())
    assert aot['complete']
    dest = BASE / 'owner-runtime/lib/mono/4.5'
    obj = BASE / 'aot-build/mscorlib.dll.o'
    lib = WORK / 'mono/mini/.libs/libmonosgen-2.0.1.dylib'
    expected = {Path(p): h for p, h in aot['frozen_inputs_sha256'].items()}
    expected.update({
        CAP / 'native/MonoOwner.cpp': 'a73847833fd1621a0304641e5f6add267787223474bea0d95b4c9d0848e4f732',
        CAP / 'native/InvocationBoundary.cpp': '9cdeae93c1904aff1e014266526a32fa17485ad07b2102cc6b68de775bc99508',
        CAP / 'host-check/native-mono-test.c': 'e0de4fe51e78df9e42d974f7473af3e988b9b771970a2e1f192ee1a45383e268',
        obj: aot['object_sha256'], dest / 'mscorlib.dll': aot['corlib_sha256'],
    })
    assert all(sha(p) == h for p, h in expected.items())
    prefix_files = sorted(str(p.relative_to(BASE / 'owner-runtime')) for p in (BASE / 'owner-runtime').rglob('*') if p.is_file())
    assert prefix_files == ['lib/mono/4.5/mscorlib.dll']
    prefix = (CAP / 'host-check/MonoHarness.cpp').read_text().split('static void need(', 1)[0]
    harness = BASE / 'tests/ProductionStaticMonoOwnerHarness.cpp'
    harness.write_text(prefix + r'''
#include "MonoOwner.hpp"
// Static corlib registration is performed solely by the actual production owner.
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    std::printf("{\"verdict\":\"%s\",\"stage\":%u,\"completed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"aot_interp_mode\":true,\"host_only_observer\":true,\"patched_loader\":true,\"static_corlib_registered\":true,\"townfall_proof\":false}\n",
        result.completed ? "REAL_MONO_OWNER_PRODUCTION_STATIC_CORLIB_HOST_PASS" : "REAL_MONO_OWNER_PRODUCTION_STATIC_CORLIB_HOST_FAIL",
        static_cast<unsigned>(result.stage), result.completed ? "true" : "false",
        unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked),
        unsigned(result.calls.deliberate.had_exception),
        result.calls.first.passed ? "true" : "false", result.calls.deliberate.passed ? "true" : "false",
        result.calls.recovery.passed ? "true" : "false");
    return result.completed ? 0 : 1;
}
''')
    forwarding_header = BASE / 'tests/MonoGcRegionApi.hpp'
    forwarding_header.write_text('#pragma once\n// Genuine full-source API and its actual configure-generated config.\n#include "config.h"\n#include <mono/utils/mono-threads-api.h>\n')
    env = os.environ.copy()
    for key in list(env):
        if key.startswith('MONO_') or key.startswith('DYLD_'):
            env.pop(key)
    env.update(MONO_CFG_DIR=str(MDK / 'etc'), MONO_CRASH_NOFILE='1', TMPDIR=str(BASE / 'tmp'))
    objects = BASE / 'production-owner-objects'
    objects.mkdir(exist_ok=True)
    commands = []
    compile_log = BASE / 'logs/PRODUCTION-STATIC-OWNER-COMPILE-LINK.log'
    compile_log.write_text('')
    def run_build(command):
        result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
        commands.append({'command': command, 'exit_code': result.returncode})
        with compile_log.open('a') as log:
            log.write(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    flags = ['/usr/bin/clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-c',
             '-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1', '-DMODBRIDGE_CAP10_STATIC_CORLIB=1',
             '-DMODBRIDGE_CAP10_CORLIB_DIR=' + json.dumps(str(BASE / 'owner-runtime/lib')),
             '-DMODBRIDGE_CAP10_CONFIG_DIR=' + json.dumps(str(MDK / 'etc')),
             '-I' + str(BASE / 'tests'), '-I' + str(CAP / 'native'),
             '-I' + str(WORK), '-I' + str(WORK / 'mono/eglib')]
    sources = [CAP / 'native/MonoOwner.cpp', CAP / 'native/InvocationBoundary.cpp', harness]
    for source in sources:
        run_build(flags + [str(source), '-o', str(objects / (source.stem + '.o'))])
    native = CAP / 'host-check/native-mono-test.c'
    run_build(['/usr/bin/clang', '-arch', 'x86_64', '-O1', '-Wall', '-Wextra', '-Werror', '-c', str(native), '-o', str(objects / 'native-observer.o')])
    exe = BASE / 'bin/mono-owner-production-static-corlib'
    run_build(['/usr/bin/clang++', '-arch', 'x86_64', *[str(objects / (p.stem + '.o')) for p in sources],
               str(objects / 'native-observer.o'), str(obj), str(lib), '-Wl,-headerpad_max_install_names', '-o', str(exe)])
    run_build(['/usr/bin/install_name_tool', '-change', str(FROZEN / 'runtime-install/lib/libmonosgen-2.0.1.dylib'), str(lib), str(exe)])
    linkdeps = run_build(['/usr/bin/otool', '-L', str(exe)])
    (BASE / 'logs/PRODUCTION-STATIC-OWNER-LINK-DEPENDENCIES.log').write_text(linkdeps)
    assert str(lib) in linkdeps and 'mscorlib.dll.dylib' not in linkdeps
    symbols = run_build(['/usr/bin/nm', '-g', str(exe)])
    (BASE / 'logs/PRODUCTION-STATIC-OWNER-SYMBOLS.log').write_text(symbols)
    assert ' D _mono_aot_module_mscorlib_info' in symbols
    env.update(MONO_LOG_LEVEL='debug', MONO_LOG_MASK='aot,dll', DYLD_PRINT_LIBRARIES='1')
    command = [str(exe)]  # Direct x86_64 launch preserves dyld trace through automatic Rosetta.
    result = subprocess.run(command, cwd=dest, env=env, text=True, capture_output=True,
                            timeout=60, preexec_fn=no_core)
    (BASE / 'logs/PRODUCTION-STATIC-OWNER.stdout.log').write_text(result.stdout)
    (BASE / 'logs/PRODUCTION-STATIC-OWNER.stderr.log').write_text(result.stderr)
    proof = {'verdict': 'REAL_MONO_OWNER_PRODUCTION_STATIC_CORLIB_HOST_FAIL', 'exit_code': result.returncode}
    result_lines = [line for line in result.stdout.splitlines() if line.startswith('{"verdict":')]
    if result_lines:
        proof.update(json.loads(result_lines[-1]))
    combined = result.stdout + result.stderr
    static_trace = "Found statically linked AOT module 'mscorlib'." in result.stdout
    clean = all(s not in combined for s in ["assertion '", 'Assertion at ', 'Native Crash Reporting', 'SIGSEGV', 'SIGABRT'])
    dynamic_corlib_loaded = any('mscorlib.dll.dylib' in line and 'dyld[' in line for line in result.stderr.splitlines())
    frozen_unchanged = all(sha(p) == h for p, h in expected.items())
    prefix_files_after = sorted(str(p.relative_to(BASE / 'owner-runtime')) for p in (BASE / 'owner-runtime').rglob('*') if p.is_file())
    proof.update({'commands': commands, 'run_command': command, 'cwd': str(dest),
                  'run_environment': {k: v for k, v in env.items() if k.startswith(('MONO_', 'DYLD_')) or k == 'TMPDIR'},
                  'inputs_sha256': {str(p): h for p, h in expected.items()},
                  'sources_sha256': {str(p): sha(p) for p in sources + [native, forwarding_header, CAP / 'native/ProbeImage.hpp', MDK / 'etc/mono/config']},
                  'executable_sha256': sha(exe), 'static_module_trace_observed': static_trace,
                  'runtime_diagnostics_clean': clean, 'dynamic_corlib_loaded': dynamic_corlib_loaded,
                  'prefix_files_before': prefix_files, 'prefix_files_after': prefix_files_after,
                  'frozen_inputs_unchanged': frozen_unchanged, 'mono_path_override_present': 'MONO_PATH' in env,
                  'registration_in_production_owner': True, 'harness_registration_present': 'mono_aot_register_module' in harness.read_text(),
                  'real_internal_headers': True, 'max_compile_jobs': 1, 'actual_owner_executions': 1,
                  'host_only': True, 'target_proof': False})
    receipt.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({k: v for k, v in proof.items() if k not in ['commands', 'inputs_sha256', 'sources_sha256']}, indent=2))
    assert result.returncode == 0 and proof.get('completed') and proof.get('real_managed_runs') == 2 and proof.get('real_managed_exceptions') == 1
    assert static_trace and clean and not dynamic_corlib_loaded and frozen_unchanged
    assert prefix_files == prefix_files_after and 'MONO_PATH' not in env
    assert not proof['harness_registration_present']

if __name__ == '__main__':
    main()

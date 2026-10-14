#!/usr/bin/env python3
"""Actual owner in full-AOT/interpreter mode on host; stock loader, no PS5 claim."""
import sys
sys.dont_write_bytecode = True
import hashlib
import json
import resource
import subprocess
from build_probe import BASE, environment
from run_mono_host_check import ROOT
from compile_owner_host import compiler_flags


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main():
    runtime = BASE / 'host-check/owner-runtime'
    assemblies = runtime / 'lib/mono/4.5'
    if not (assemblies / 'mscorlib.dll.dylib').is_file():
        raise ValueError('Generate genuine full,interp host AOT corlib first')
    aot_receipt = json.loads((BASE / 'evidence/OWNER-CORLIB-AOT.json').read_text())
    for name, expected in [('mscorlib.dll', aot_receipt['source_sha256']),
                           ('mscorlib.dll.dylib', aot_receipt['aot_sha256'])]:
        if hashlib.sha256((assemblies / name).read_bytes()).hexdigest() != expected:
            raise ValueError('host AOT dependency drift: ' + name)
    output = BASE / 'host-check/owner-objects'
    output.mkdir(exist_ok=True)
    # Reuse the real host-only native observer; no engine/readiness simulation claim.
    helper = (BASE / 'host-check/MonoHarness.cpp').read_text()
    if helper.count('static void need(') != 1:
        raise ValueError('host observer extraction anchor drift')
    harness = output / 'MonoOwnerHarness.cpp'
    harness.write_text(helper.split('static void need(', 1)[0] + r'''
#include "MonoOwner.hpp"
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    std::printf("{\"verdict\":\"%s\",\"stage\":%u,\"completed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"aot_interp_mode\":true,\"host_only_observer\":true,\"stock_loader\":true,\"strict_resolver_proof\":false,\"townfall_proof\":false}\n",
        result.completed ? "REAL_MONO_OWNER_HOST_PASS" : "REAL_MONO_OWNER_HOST_FAIL",
        static_cast<unsigned>(result.stage), result.completed ? "true" : "false",
        unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked),
        unsigned(result.calls.deliberate.had_exception),
        result.calls.first.passed ? "true" : "false",
        result.calls.deliberate.passed ? "true" : "false",
        result.calls.recovery.passed ? "true" : "false");
    return result.completed ? 0 : 1;
}
''')
    env = environment()
    for name in ['MONO_PATH', 'MONO_ENV_OPTIONS', 'MONO_AOT_CACHE']:
        env.pop(name, None)
    env.update(MONO_CFG_DIR=str(ROOT / 'etc'), MONO_CRASH_NOFILE='1')
    commands = []
    logs = []
    for name, source in [('MonoOwner', BASE / 'native/MonoOwner.cpp'),
                         ('MonoOwnerHarness', harness)]:
        command = compiler_flags(runtime) + [str(source), '-o', str(output / (name + '-run.o'))]
        result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
        commands.append(command)
        logs.append(result.stdout + result.stderr)
        (BASE / 'evidence/OWNER-HOST-LINK.log').write_text(''.join(logs))
        if result.returncode:
            raise RuntimeError(name + ' compile failed')
    executable = output / 'mono-owner-harness'
    command = ['/usr/bin/clang++', '-arch', 'x86_64',
               str(output / 'MonoOwner-run.o'), str(output / 'MonoOwnerHarness-run.o'),
               str(output / 'InvocationBoundary.o'), str(BASE / 'host-check/native-mono-test.o'),
               str(ROOT / 'lib/libmonosgen-2.0.1.dylib'),
               '-Wl,-headerpad_max_install_names', '-o', str(executable)]
    result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
    commands.append(command)
    logs.append(result.stdout + result.stderr)
    (BASE / 'evidence/OWNER-HOST-LINK.log').write_text(''.join(logs))
    if result.returncode:
        raise RuntimeError('actual owner host link failed')
    subprocess.run(['/usr/bin/install_name_tool', '-change',
                    '/Library/Frameworks/Mono.framework/Versions/6.12.0/lib/libmonosgen-2.0.1.dylib',
                    str(ROOT / 'lib/libmonosgen-2.0.1.dylib'), str(executable)], check=True)
    command = ['/usr/bin/arch', '-x86_64', str(executable)]
    result = subprocess.run(command, cwd=assemblies, env=env, text=True,
                            capture_output=True, timeout=45, preexec_fn=no_core)
    (BASE / 'evidence/OWNER-HOST-CHECK.log').write_text(result.stdout + result.stderr)
    proof = {'verdict': 'REAL_MONO_OWNER_HOST_FAIL', 'exit_code': result.returncode,
             'aot_interp_mode': True, 'strict_resolver_proof': False, 'townfall_proof': False}
    if result.returncode == 0:
        proof.update(json.loads(result.stdout.strip().splitlines()[-1]))
    proof['runtime_diagnostics_clean'] = "assertion '" not in result.stdout + result.stderr
    proof.update({'compiler_commands': commands, 'command': command,
                  'harness_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                  'source_sha256': hashlib.sha256((BASE / 'native/MonoOwner.cpp').read_bytes()).hexdigest(),
                  'corlib_aot_sha256': hashlib.sha256((assemblies / 'mscorlib.dll.dylib').read_bytes()).hexdigest()})
    (BASE / 'evidence/OWNER-HOST-CHECK.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({key: value for key, value in proof.items()
                      if key not in ['compiler_commands']}, indent=2))
    if result.returncode or not proof['runtime_diagnostics_clean']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

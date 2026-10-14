#!/usr/bin/env python3
"""Compile real owner/boundary against matching Mono APIs; no target admission/run."""
import sys
sys.dont_write_bytecode = True
import hashlib
import json
import subprocess
from build_probe import BASE, environment
from run_mono_host_check import ROOT


def compiler_flags(runtime_root=ROOT):
    return [
        '/usr/bin/clang++', '-arch', 'x86_64', '-std=c++17', '-O1',
        '-Wall', '-Wextra', '-Werror', '-c',
        '-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1',
        '-DMODBRIDGE_CAP10_CORLIB_DIR=' + json.dumps(str(runtime_root / 'lib')),
        '-DMODBRIDGE_CAP10_CONFIG_DIR=' + json.dumps(str(ROOT / 'etc')),
        '-I' + str(BASE / 'native'), '-I' + str(BASE / 'host-check'),
        '-I' + str(BASE / 'dependencies/mono-source'),
        '-I' + str(ROOT / 'include/mono-2.0'),
        '-I/opt/homebrew/include/glib-2.0',
        '-I/opt/homebrew/lib/glib-2.0/include',
    ]


def main():
    output = BASE / 'host-check/owner-objects'
    output.mkdir(exist_ok=True)
    common = compiler_flags()
    records = []
    logs = []
    for name in ['MonoOwner', 'InvocationBoundary']:
        source = BASE / 'native' / (name + '.cpp')
        target = output / (name + '.o')
        command = common + [str(source), '-o', str(target)]
        result = subprocess.run(command, cwd=BASE, env=environment(),
                                text=True, capture_output=True)
        logs.append(result.stdout + result.stderr)
        (BASE / 'evidence/OWNER-HOST-COMPILE.log').write_text(''.join(logs))
        if result.returncode:
            raise RuntimeError(name + ' compile failed; see OWNER-HOST-COMPILE.log')
        records.append({'source': str(source.relative_to(BASE)),
                        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                        'object_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                        'command': command})
    receipt = {'verdict': 'REAL_OWNER_HOST_COMPILE_PASS', 'translation_units': len(records),
               'matching_mono': '6.12.0.206 / 0cbf0e290c3',
               'host_validation_macro': True, 'target_admission_macro_defined': False,
               'target_compile': False, 'linked': False, 'executed': False,
               'strict_loader_proof': False, 'townfall_proof': False, 'objects': records}
    (BASE / 'evidence/OWNER-HOST-COMPILE.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

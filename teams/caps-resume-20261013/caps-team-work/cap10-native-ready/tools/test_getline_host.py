#!/usr/bin/env python3
"""Host-only semantic tests of the exact source function, both real stdio routes.

Macro-selected routes isolate code paths for testing. Target capability claims
come solely from genuine no-cache target configure and build receipts.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline-parent', action='store_true', help='Expect the exact unpatched parent wrapper to fail the same ownership tests')
ARGS = parser.parse_args()
SOURCE_ROOT = HERE.parent / 'cap10-port-closure/target-work' if ARGS.baseline_parent else HERE / 'target-work'
SOURCE = SOURCE_ROOT / 'external/corefx/src/Native/Unix/System.Native/pal_io.c'
OUT = HERE / ('tests/baseline-build' if ARGS.baseline_parent else 'tests/build')
OUT.mkdir(exist_ok=True)
text = SOURCE.read_text()
begin = text.index('char* SystemNative_GetLine(FILE* stream)\n{')
end = text.index('\n}\n', begin) + 3
function = text[begin:end]
unit = OUT / 'ExactGetLine.c'
unit.write_text('#include <assert.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <errno.h>\n' + function)
receipt = dict(scope='HOST_ONLY_EXACT_FUNCTION_SEMANTICS', target_execution=False,
               baseline_parent=ARGS.baseline_parent,
               source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
               function_sha256=hashlib.sha256(function.encode()).hexdigest(),
               cases=['newline/empty/EOF', 'final line without newline', 'longer than 4 BUFSIZ',
                      'embedded NUL', 'caller-owned buffer survives subsequent reads',
                      'actual Darwin stdio EBADF read failure', 'controlled malloc failure',
                      'negative getline after allocation frees buffer and preserves errno',
                      '200 concurrent shared-stream reads', 'zero live returned allocations'], commands=[])


def run(name, command):
    result = subprocess.run(command, text=True, capture_output=True)
    log = HERE / 'logs' / (('baseline-' if ARGS.baseline_parent else '') + name + '.log')
    log.write_text(result.stdout + result.stderr)
    receipt['commands'].append(dict(command=command, exit_code=result.returncode,
                                    log_sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    if result.returncode:
        if ARGS.baseline_parent and name == 'getline-host-run':
            receipt['verdict'] = 'EXPECTED_UNPATCHED_PARENT_OWNERSHIP_FAILURE'
            receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
            print('Expected unpatched-parent ownership failure captured.')
            raise SystemExit(0)
        raise SystemExit('Failed ' + name + ': inspect ' + str(log))
    if result.stdout:
        print(result.stdout.strip())


receipt_path = HERE / 'evidence' / ('GETLINE-PARENT-REGRESSION.json' if ARGS.baseline_parent else 'GETLINE-HOST-TEST.json')
for route in (['getline'] if ARGS.baseline_parent else ['fgetln', 'getline']):
    flags = ['-std=c11', '-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-pthread']
    definitions = ['-DHAVE_' + name + '=1' for name in ['FGETLN', 'DECL_FGETLN', 'FLOCKFILE', 'DECL_FLOCKFILE', 'FUNLOCKFILE', 'DECL_FUNLOCKFILE']]
    if route == 'getline':
        definitions += ['-DHAVE_GETLINE=1', '-DHAVE_DECL_GETLINE=1']
    obj = OUT / (route + '.o')
    exe = OUT / ('test-' + route)
    run(route + '-host-compile', ['/usr/bin/clang', *flags, *definitions,
                                '-Dmalloc=cap10_malloc', '-Dfree=cap10_free', '-Dgetline=cap10_getline',
                                '-c', str(unit), '-o', str(obj)])
    run(route + '-host-link', ['/usr/bin/clang', *flags, '-DTEST_FGETLN_ROUTE=' + str(int(route == 'fgetln')),
                             str(HERE / 'tests/GetLineHost.c'), str(obj), '-o', str(exe)])
    run(route + '-host-run', [str(exe)])
if ARGS.baseline_parent:
    raise SystemExit('Unexpected parent ownership pass; regression did not reproduce')
receipt['verdict'] = 'PASS_BOTH_REAL_STDIO_ROUTES_WITH_ASAN_UBSAN'
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')

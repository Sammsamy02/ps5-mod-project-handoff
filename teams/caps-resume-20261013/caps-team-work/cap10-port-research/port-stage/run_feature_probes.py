#!/usr/bin/env python3
"""Bounded syntax/assembly probes only: no link, PRX or device contact."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--sdk', type=pathlib.Path, required=True)
parser.add_argument('--llvm-bin', type=pathlib.Path, required=True)
args = parser.parse_args()
probe = HERE / 'probe'
probe.mkdir(exist_ok=True)
env = os.environ.copy()
env['LLVM_CONFIG'] = str(args.llvm_bin / 'llvm-config')
compiler = args.sdk / 'bin/prospero-clang'
commands = []


def run(name, options):
    command = [str(compiler), *options]
    result = subprocess.run(command, capture_output=True, text=True, env=env)
    (probe / (name + '.stdout')).write_text(result.stdout)
    (probe / (name + '.stderr')).write_text(result.stderr)
    commands.append({'name': name, 'command': command, 'returncode': result.returncode})
    if result.returncode:
        raise RuntimeError(name + ': ' + result.stderr)
    return result.stdout


# Check the actual source declarations, not redeclared substitute functions.
api = probe / 'public-api.c'
api.write_text('''#include <pthread.h>
#include <pthread_np.h>
#include <sys/thr.h>
#include <unistd.h>
#ifndef __PROSPERO__
#error genuine Prospero target required
#endif
#ifndef _POSIX_VERSION
#error no POSIX backend declaration
#endif
static int (*const attr_get)(pthread_t, pthread_attr_t*) = pthread_attr_get_np;
static int (*const stack_get)(const pthread_attr_t *restrict, void **restrict, size_t *restrict) = pthread_attr_getstack;
static int (*const os_id)(long*) = thr_self;
int api_probe(pthread_t thread, pthread_attr_t *attr, void **addr, size_t *size, long *tid, pthread_key_t *key) {
  return attr_get(thread,attr) + stack_get(attr,addr,size) + os_id(tid)
    + pthread_key_create(key,0) + pthread_setspecific(*key,addr)
    + (pthread_getspecific(*key) == 0) + pthread_key_delete(*key);
}
''')
run('public-api', ['-std=c11', '-Werror', '-fsyntax-only', str(api)])
macros = run('macros', ['-dM', '-E', '-x', 'c', '-include', 'unistd.h', '/dev/null'])
selected = {line.split()[1]: ' '.join(line.split()[2:]) for line in macros.splitlines()
            if len(line.split()) >= 3 and line.split()[1] in
            ['__PROSPERO__', '__FreeBSD__', '_POSIX_VERSION', '__SIZEOF_POINTER__', '__x86_64__']}
assert selected.get('__PROSPERO__') == '1'
assert selected.get('__SIZEOF_POINTER__') == '8'
assert selected.get('__x86_64__') == '1'
assert '__FreeBSD__' in selected and '_POSIX_VERSION' in selected

# Compile the exact AC_COMPILE_IFELSE payload from the staged product source.
configure = (HERE / 'derived/configure.ac').read_text()
detector = re.search(r'AC_MSG_CHECKING\(\[whether the compiler targets Prospero\]\)\n'
                     r'AC_COMPILE_IFELSE\(\[AC_LANG_SOURCE\(\[\[\n(.*?)\n\]\]\)', configure, re.S)
assert detector
detection = probe / 'configure-detector.c'
detection.write_text(detector.group(1) + '\n')
run('configure-detector', ['-std=c11', '-Werror', '-fsyntax-only', str(detection)])

tls = probe / 'compiler-tls.c'
tls.write_text('__thread void *tls_value;\nvoid **get_tls_address(void) { return &tls_value; }\n')
assembly = probe / 'compiler-tls.s'
run('compiler-tls', ['-std=c11', '-S', str(tls), '-o', str(assembly)])
assert '__emutls_get_address' in assembly.read_text(), 'wrapper did not emit emulated TLS'

receipt = {'scope': 'Mac-only syntax/preprocessor/assembly; no target link or execution',
           'commands': commands, 'genuine_compiler_macros': selected,
           'public_thread_stack_tls_declarations_compile': True,
           'exact_configure_detector_compiles': True, 'compiler_tls_is_emulated': True,
           'compiler_sha256': hashlib.sha256((args.llvm_bin / 'clang-21').read_bytes()).hexdigest(),
           'sdk_wrapper_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
           'checks': 5}
(HERE / 'FEATURE-PROBE-RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({k: v for k, v in receipt.items() if k != 'commands'}, indent=2))

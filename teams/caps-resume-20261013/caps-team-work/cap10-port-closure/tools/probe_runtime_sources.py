#!/usr/bin/env python3
"""Compile pinned Mono source with the public target; never link or execute it.

The deliberately small config header is a translation-unit fixture, not a
substitute for a complete configure run. Every selected ABI fact is asserted
against the actual compiler/SDK below. No OS macro or runtime API is replaced.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--llvm-bin', type=Path, required=True)
args = parser.parse_args()
src = args.source.resolve()
assert hashlib.sha256((src / 'configure.ac').read_bytes()).hexdigest() == '982a753d6163aa07b7b6ba5dacf3eb73b01379031f43c44c3bbca5ec7ce68754'
out = HERE / 'probe'
out.mkdir(exist_ok=True)
env = dict(os.environ, LLVM_CONFIG=str(args.llvm_bin / 'llvm-config'))
cc = str(args.sdk / 'bin/prospero-clang')
commands = []


def run(name, command, expect=0):
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    (HERE / 'logs' / (name + '.stdout')).write_text(result.stdout)
    (HERE / 'logs' / (name + '.stderr')).write_text(result.stderr)
    commands.append(dict(name=name, command=command, returncode=result.returncode))
    if result.returncode != expect:
        raise RuntimeError(name + '\n' + result.stderr)
    return result.stdout


# These are build selections and measured ABI widths, never fake OS defines.
(out / 'config.h').write_text('''/* Bounded compile fixture; NOT production config.h. */
#define TARGET_AMD64 1
#define HOST_AMD64 1
#define SIZEOF_VOID_P 8
#define TARGET_SIZEOF_VOID_P 8
#define SIZEOF_REGISTER 8
#define SIZEOF_LONG 8
#define HAVE_SIGACTION 1
#define HAVE_UCONTEXT_H 1
''')
eglib = (src / 'mono/eglib/eglib-config.h.in').read_text()
values = dict(GNUC_PRETTY='__PRETTY_FUNCTION__', GNUC_UNUSED='__attribute__((unused))',
              ORDER='1234', GNUC_NORETURN='__attribute__((noreturn))', SEARCHSEP=':',
              PATHSEP='/', BREAKPOINT='__builtin_trap()', OS='UNIX', HAVE_ALLOCA_H='0',
              HAVE_UNISTD_H='1', GSIZE='size_t', GSSIZE='ptrdiff_t', GSIZE_FORMAT='"lu"',
              PIDTYPE='int')
for key, value in values.items():
    eglib = eglib.replace('@' + key + '@', value)
assert '@' not in eglib
(out / 'eglib-config.h').write_text(eglib)
common = [cc, '-std=gnu11', '-Werror', '-fPIC', '-DPIC', '-I' + str(out), '-I' + str(src), '-I' + str(src / 'mono/eglib')]
abi = out / 'abi.c'
abi.write_text('''#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stddef.h>
#ifndef __PROSPERO__
#error genuine Prospero target required
#endif
#if __FreeBSD__ != 9 || __SIZEOF_POINTER__ != 8 || __SIZEOF_LONG__ != 8 || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error unexpected public target ABI
#endif
_Static_assert(__builtin_types_compatible_p(size_t,unsigned long), "size_t ABI");
_Static_assert(__builtin_types_compatible_p(pid_t,int), "pid_t ABI");
_Static_assert(__builtin_classify_type((pthread_t)0) == 5, "pthread_t must be a pointer");
int (*check_sigaction)(int,const struct sigaction *restrict,struct sigaction *restrict) = sigaction;
''')
run('target-abi', common + ['-fsyntax-only', str(abi)])
tls = src / 'mono/utils/mono-tls.c'
run('mono-tls-pthread', common + ['-c', str(tls), '-o', str(out / 'mono-tls-pthread.o')])
undefined = run('mono-tls-pthread-undefined', [str(args.llvm_bin / 'llvm-nm'), '-u', str(out / 'mono-tls-pthread.o')])
for required in ['pthread_key_create', 'pthread_key_delete', 'pthread_getspecific']:
    assert required in undefined, required
assert '__emutls_' not in undefined
run('mono-tls-pthread-assembly', common + ['-S', str(tls), '-o', str(out / 'mono-tls-pthread.s')])
assembly = (out / 'mono-tls-pthread.s').read_text()
assert 'GOTTPOFF' not in assembly and '@TPOFF' not in assembly and '__emutls_' not in assembly
# Reproduce the original route, preserving the actual public compiler flags.
run('mono-tls-compiler-assembly', common + ['-DMONO_KEYWORD_THREAD=__thread', '-S', str(tls), '-o', str(out / 'mono-tls-compiler.s')])
compiler_asm = (out / 'mono-tls-compiler.s').read_text()
assert '__emutls_get_address' in compiler_asm and 'GOTTPOFF' in compiler_asm
ctx = out / 'context.c'
ctx.write_text('''#include <config.h>
#include <glib.h>
#include <mono/utils/mono-context.h>
void capture_context(MonoContext *context) { MONO_CONTEXT_GET_CURRENT(*context); }
''')
run('mono-context', common + ['-c', str(ctx), '-o', str(out / 'mono-context.o')])
run('mono-context-conversion', common + ['-c', str(src / 'mono/utils/mono-context.c'), '-o', str(out / 'mono-context-conversion.o')])
receipt = dict(scope='Mac-only actual target object/assembly compilation; no target link/execution',
               source=str(src), source_commit='0cbf0e290c31adb476f9de0fa44b1d8829affa40',
               fixture_config_not_full_configure=True, commands=commands,
               checks=dict(abi=True, actual_mono_tls_pthread=True, no_emutls_or_native_tls_offsets=True,
                           incompatible_original_tls_route_reproduced=True, actual_context_capture_compiles=True,
                           actual_signal_context_conversion_compiles=True))
(HERE / 'evidence/SOURCE-PROBES.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt['checks'], indent=2))

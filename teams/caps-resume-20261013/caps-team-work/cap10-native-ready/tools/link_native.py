#!/usr/bin/env python3
"""Strict ordinary ELF closure check of every genuine native archive member.

No fixture supplies native/Mono implementations. Never execute this link-only
image, package it as a PRX, or treat it as a console-loadable native helper.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

HERE = Path(__file__).resolve().parents[1]
WORK = HERE / 'target-work'
LOCAL = json.loads((HERE / 'LOCAL-INPUTS.json').read_text())
SDK = Path(LOCAL['sdk'])
LLVM = Path(LOCAL['llvm_bin'])
OUT = HERE / 'link-check'
OUT.mkdir(exist_ok=True)
ENV = dict(os.environ, LLVM_CONFIG=str(LLVM / 'llvm-config'))
NATIVE = WORK / 'mono/native/.libs/libmono-native.a'
RUNTIME = WORK / 'mono/mini/.libs/libmonosgen-2.0.a'
IMAGE = OUT / 'native-whole-archive-link-only.elf'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


receipt = dict(verdict='INCOMPLETE', target_execution=False, prx_packaging=False,
               console_helper_loadability_proven=False, fixture_implementations=False,
               inputs={str(p): sha(p) for p in [NATIVE, RUNTIME, WORK / 'config.h']}, commands=[])


def save():
    (HERE / 'evidence/NATIVE-LINK.json').write_text(json.dumps(receipt, indent=2) + '\n')


def run(name, command):
    result = subprocess.run(command, cwd=HERE, env=ENV, text=True, capture_output=True)
    log = OUT / (name + '.txt')
    log.write_text(result.stdout + result.stderr)
    receipt['commands'].append(dict(name=name, command=command, exit_code=result.returncode,
                                    log_sha256=sha(log)))
    save()
    if result.returncode:
        raise SystemExit(name + ' failed: inspect ' + str(log))
    return result.stdout


run('link', [str(SDK / 'bin/prospero-clang'), '-nostartfiles', '-Wl,--whole-archive', str(NATIVE),
             '-Wl,--no-whole-archive', str(RUNTIME), '-lpthread',
             '-Wl,--no-undefined', '-Wl,-z,defs', '-Wl,--export-dynamic',
             '-Wl,-Map,' + str(OUT / 'link.map'), '-o', str(IMAGE)])
elf = run('ELF', [str(LLVM / 'llvm-readelf'), '--file-header', '--dynamic-table',
                  '--dyn-syms', '--sections', str(IMAGE)])
defined = run('NATIVE-DEFINED', [str(LLVM / 'llvm-nm'), '--defined-only', '--extern-only', str(NATIVE)])
undefined = run('NATIVE-UNDEFINED', [str(LLVM / 'llvm-nm'), '--undefined-only', str(NATIVE)])
symbols = run('IMAGE-SYMBOLS', [str(LLVM / 'llvm-nm'), str(IMAGE)])
members = run('ARCHIVE-MEMBERS', [str(Path(LOCAL['archive_bin']) / 'llvm-ar'), 't', str(NATIVE)])
native_symbols = sorted(set(re.findall(r'^[0-9a-f]+\s+[A-Za-z]\s+(SystemNative_\w+)$', defined, re.M)))
imports = sorted(set(re.findall(r'^\s*\d+:.*\bUND\s+(\S+)\s*$', elf, re.M)))
native_dynamic = sorted(set(re.findall(r'^\s*\d+:.*\bGLOBAL\s+DEFAULT\s+\d+\s+(SystemNative_\w+)\s*$', elf, re.M)))
providers = re.findall(r'\(NEEDED\).*\[([^]]+)\]', elf)
local_families = ('mono_', 'sgen_', 'ves_icall_', 'SystemNative_', 'Brotli')
undefined_local = [s for s in imports if s.startswith(local_families)]
assert not undefined_local, undefined_local
assert native_symbols and native_dynamic == native_symbols, (len(native_symbols), len(native_dynamic))
assert 'libmonosgen-2.0.so' not in elf
assert 'Advanced Micro Devices X86-64' in elf
assert all(sha(Path(p)) == digest for p, digest in receipt['inputs'].items()), 'Input drift'
for symbol in ['mono_pal_init', 'mono_native_initialize', 'mono_native_platform_type',
               'ves_icall_Interop_Sys_Read', 'mono_add_internal_call_with_flags']:
    assert re.search(r'^[0-9a-f]+\s+[A-Za-z]\s+' + symbol + r'$', symbols, re.M), symbol
receipt.update(verdict='WHOLE_NATIVE_ARCHIVE_STRICT_ELF_LINK_PASS', output=str(IMAGE),
               output_sha256=sha(IMAGE), output_bytes=IMAGE.stat().st_size,
               archive_bytes=NATIVE.stat().st_size, archive_sha256=sha(NATIVE),
               archive_member_count=len(members.splitlines()), systemnative_definition_count=len(native_symbols),
               systemnative_definitions=native_symbols, systemnative_dynamic_exports=native_dynamic,
               undefined_local_symbols=undefined_local, dynamic_import_count=len(imports),
               dynamic_imports=imports, needed_public_import_libraries=providers,
               link_map_sha256=sha(OUT / 'link.map'), textrel_dynamic_tag_present='TEXTREL' in elf,
               format='ELF64 x86-64 ET_DYN PIE via unmodified public compiler wrapper; link-only',
               runtime_library='genuine source-built static Mono archive; not SDK fake Mono import library')
save()
print(json.dumps({k: receipt[k] for k in ['verdict', 'archive_member_count', 'systemnative_definition_count',
                                         'dynamic_import_count', 'needed_public_import_libraries', 'output_sha256']}, indent=2))

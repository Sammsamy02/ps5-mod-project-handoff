"""Link genuine reviewed components as ordinary shared ELF, never package/run it."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

BASE = Path(__file__).resolve().parents[1]
PORT = BASE.parent/'cap10-port-closure'
CROSS = BASE.parent/'cap10-cross-aot'
local = json.loads((PORT/'LOCAL-INPUTS.json').read_text())
sdk = Path(local['sdk'])
llvm = Path(local['llvm_bin'])
out = BASE/'shared-owner.elf'
receipt_path = BASE/'evidence/SHARED-OWNER-LINK.json'
assert not out.exists() and not receipt_path.exists(), 'Preserve existing evidence'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

prior = json.loads((PORT/'evidence/TARGET-OWNER-LINK.json').read_text())
assert prior['verdict'] == 'TARGET_OWNER_STATIC_CORLIB_LINK_PASS'
inputs = {Path(p): h for p, h in prior['input_sha256'].items()}
objects = [PORT/'owner-link'/name for name in
           ('MonoOwner.o', 'InvocationBoundary.o', 'LinkOnlyObserver.o', 'LinkOnlyProvider.o')]
runtime = PORT/'target-work/mono/mini/.libs/libmonosgen-2.0.a'
aot = CROSS/'artifacts/mscorlib.full-interp.ps5.o'
for path in objects:
    inputs[path] = sha(path)
assert all(sha(path) == h for path, h in inputs.items())
exports = BASE/'test-only.exports'
exports.write_text('{ global: main; local: *; };\n')
inputs[exports] = sha(exports)
env = dict(os.environ, LLVM_CONFIG=str(llvm/'llvm-config'))
for key in list(env):
    if key.startswith(('MONO_', 'DYLD_')):
        env.pop(key)
commands = []

def run(label, command):
    result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
    log = BASE/'logs'/(label+'.log')
    log.write_text(result.stdout+result.stderr)
    commands.append({'label': label, 'command': command, 'exit_code': result.returncode,
                     'log_sha256': sha(log)})
    assert result.returncode == 0, label+': '+result.stderr[-6000:]
    return result.stdout

run('LINK', [str(sdk/'bin/prospero-clang++'), '-shared',
            *map(str, objects), str(aot), str(runtime), '-lpthread',
            '-Wl,--shared', '-Wl,--no-undefined', '-Wl,-z,defs',
            '-Wl,--version-script='+str(exports), '-Wl,-Map,'+str(BASE/'shared-owner.map'),
            '-o', str(out)])
header = run('ELF', [str(llvm/'llvm-readelf'), '-h', '-l', '-d', '-W', str(out)])
symbols = run('DYNSYMS', [str(llvm/'llvm-readelf'), '--dyn-syms', '-W', str(out)])
relocations = run('RELOCATIONS', [str(llvm/'llvm-readelf'), '-r', '-W', str(out)])
defined, undefined = [], []
for line in symbols.splitlines():
    match = re.match(r'\s*\d+:\s+[0-9a-f]+\s+\d+\s+\w+\s+(GLOBAL|WEAK)\s+\w+\s+(\w+)\s+(\S+)', line)
    if match:
        (undefined if match[2] == 'UND' else defined).append(match[3])
assert defined == ['main'], defined
assert undefined and not any(re.match(r'(mono_|sgen_|ves_icall_|SystemNative_)', x) for x in undefined)
assert 'DYN (Shared object file)' in header and '(PIE)' not in header and 'TEXTREL' not in header
assert 'libmonosgen' not in header
needed = re.findall(r'Shared library: \[([^\]]+)\]', header)
assert needed and len(needed) == len(set(needed))
types = {}
for name in re.findall(r'\bR_X86_64_[A-Z0-9_]+\b', relocations):
    types[name] = types.get(name, 0)+1
assert types and not any(x in types for x in ['R_X86_64_32', 'R_X86_64_32S', 'R_X86_64_PC32'])
full_symbols = run('LOCAL-SYMBOLS', [str(llvm/'llvm-nm'), str(out)])
required = ['mono_aot_module_mscorlib_info', 'mono_aot_register_module',
            'mono_jit_init_version', 'mono_threads_enter_gc_unsafe_region',
            'mono_threads_exit_gc_unsafe_region', 'mono_gchandle_new',
            'mono_gchandle_free', 'mono_runtime_invoke', 'mono_dl_fallback_register']
for name in required:
    assert re.search(r'\b'+re.escape(name)+r'$', full_symbols, re.MULTILINE), name
assert all(sha(path) == h for path, h in inputs.items())
receipt = {'verdict': 'ORDINARY_SHARED_OWNER_LINK_PASS', 'commands': commands,
           'input_sha256': {str(p): h for p, h in inputs.items()},
           'output': str(out), 'output_bytes': out.stat().st_size, 'output_sha256': sha(out),
           'defined_dynamic_symbols': defined, 'undefined_dynamic_symbols': sorted(undefined),
           'needed_libraries': needed, 'relocation_counts': types,
           'textrel': False, 'pie': False, 'monosgen_sdk_stub': False,
           'retained_registration_and_rooting_symbols': required,
           'test_fixture_export_only': True, 'native_dependency_behavior_proven': False,
           'target_execution': False, 'game_prx_build_or_package': False,
           'scope': 'Actual reviewed owner/runtime/corlib and link-only fixtures; ordinary shared ELF with public SDK providers. No combined game/PRX or console proof.'}
receipt_path.write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps({k: receipt[k] for k in ['verdict','output_bytes','output_sha256',
                  'defined_dynamic_symbols','needed_libraries','relocation_counts']}, indent=2))

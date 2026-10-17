#!/usr/bin/env python3
"""Bind genuine compiled native inputs, exported API coverage, providers and format."""
from pathlib import Path
import collections
import difflib
import hashlib
import json
import re
import shlex
import struct
import subprocess

HERE = Path(__file__).resolve().parents[1]
WORK = HERE / 'target-work'
BASE = HERE.parent / 'cap10-port-closure/target-work'
LOCAL = json.loads((HERE / 'LOCAL-INPUTS.json').read_text())
SDK = Path(LOCAL['sdk'])
LLVM = Path(LOCAL['llvm_bin'])
LINK = json.loads((HERE / 'evidence/NATIVE-LINK.json').read_text())
ARCHIVE = WORK / 'mono/native/.libs/libmono-native.a'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


dependencies = set()
compiled = []
declared = set()
for lo in sorted(WORK.rglob('libmono_native_la-*.lo')):
    dep = lo.parent / '.deps' / (lo.stem + '.Plo')
    logical = dep.read_text().replace('\\\n', ' ').split('\n', 1)[0]
    paths = [(WORK / 'mono/native' / x).resolve() for x in shlex.split(logical.split(':', 1)[1])]
    source = paths[0]
    assert source.is_relative_to(WORK) and source.is_file(), source
    dependencies.update(paths)
    rel = source.relative_to(WORK)
    original = BASE / rel
    compiled.append(dict(path=str(rel), sha256=sha(source),
                         matches_frozen_parent_source=original.exists() and sha(source) == sha(original),
                         dependency_receipt=str(dep.relative_to(HERE)), dependency_receipt_sha256=sha(dep)))
    header = source.with_suffix('.h')
    if header.exists():
        declared.update(re.findall(r'\b(SystemNative_\w+)\s*\(', header.read_text()))
assert len(compiled) == LINK['archive_member_count'] == 42
changed = [x['path'] for x in compiled if not x['matches_frozen_parent_source']]
assert changed == ['external/corefx/src/Native/Unix/System.Native/pal_io.c'], changed
bindings = [dict(path=str(p), sha256=sha(p), bytes=p.stat().st_size) for p in sorted(dependencies)]
(HERE / 'evidence/NATIVE-COMPILED-INPUTS.json').write_text(json.dumps(dict(compiled_sources=compiled, dependency_files=bindings), indent=2) + '\n')
exports = set(LINK['systemnative_definitions'])
missing = sorted(declared - exports)
statistics = WORK / 'external/corefx/src/Native/Unix/System.Native/pal_networkstatistics.h'
expected_missing = sorted(set(re.findall(r'\b(SystemNative_\w+)\s*\(', statistics.read_text())))
assert missing == expected_missing and len(missing) == 11, missing

provider_symbols = {}
provider_inputs = []
for needed in LINK['needed_public_import_libraries']:
    path = SDK / 'target/lib' / needed.replace('.sprx', '.so')
    result = subprocess.run([str(LLVM / 'llvm-nm'), '--defined-only', '--extern-only', str(path)], text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    provider_symbols[needed] = set(re.findall(r'^[0-9a-f]+\s+[A-Za-z]\s+(\S+)$', result.stdout, re.M))
    provider_inputs.append(dict(path=str(path), sha256=sha(path), public_import_library=True, implementation=False))
elf = (HERE / 'link-check/ELF.txt').read_text()
import_bindings = {match.group(2): match.group(1) for match in re.finditer(
    r'^\s*\d+:.*?\b(GLOBAL|WEAK)\s+DEFAULT\s+UND\s+(\S+)\s*$', elf, re.M)}
providers = {symbol: dict(binding=import_bindings[symbol],
                         providers=[name for name, names in provider_symbols.items() if symbol in names])
             for symbol in LINK['dynamic_imports']}
unprovided = sorted(symbol for symbol, item in providers.items() if not item['providers'])
assert all(item['providers'] or item['binding'] == 'WEAK' for item in providers.values())
assert unprovided == sorted(['__dladdr', '__dlclose', '__dlerror', '__dlopen', '__dlsym', 'kernel_mprotect'])
(HERE / 'evidence/NATIVE-IMPORT-PROVIDERS.json').write_text(json.dumps(dict(libraries=provider_inputs, symbols=providers,
    unresolved_weak_sdk_hooks=unprovided, strong_imports_without_provider=[]), indent=2) + '\n')


def objects(data):
    assert data.startswith(b'!<arch>\n')
    position = 8
    longnames = b''
    out = []
    while position < len(data):
        header = data[position:position + 60]
        assert header[58:60] == b'`\n'
        size = int(header[48:58])
        blob = data[position + 60:position + 60 + size]
        name = header[:16].decode().strip()
        position += 60 + size + size % 2
        if name == '//':
            longnames = blob
            continue
        if name.startswith('/') and name[1:].isdigit():
            name = longnames[int(name[1:]):].split(b'/\n', 1)[0].decode()
        if blob.startswith(b'\x7fELF'):
            out.append((name, blob))
    return out


relocations = collections.Counter()
constructors = []
members = objects(ARCHIVE.read_bytes())
for member, data in members:
    assert data[4:6] == b'\x02\x01'
    shoff = struct.unpack_from('<Q', data, 40)[0]
    entsize, count, stridx = struct.unpack_from('<HHH', data, 58)
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff + index * entsize) for index in range(count)]
    names = sections[stridx]
    strings = data[names[4]:names[4] + names[5]]
    for section in sections:
        name = strings[section[0]:].split(b'\0', 1)[0].decode()
        if name in ['.init_array', '.fini_array', '.ctors', '.dtors'] and section[5]:
            constructors.append(dict(member=member, section=name, bytes=section[5]))
        if section[1] in [4, 9] and sections[section[7]][2] & 2:
            for offset in range(section[4], section[4] + section[5], section[9]):
                kind = struct.unpack_from('<Q', data, offset + 8)[0] & 0xffffffff
                relocations[kind] += 1
assert len(members) == 42 and not constructors
assert not relocations[10] and not relocations[11]
config = WORK / 'config.h'
baseline_config = BASE / 'config.h'
delta = ''.join(difflib.unified_diff(baseline_config.read_text().splitlines(keepends=True), config.read_text().splitlines(keepends=True), fromfile='frozen-parent/config.h', tofile='native-ready/config.h'))
(HERE / 'evidence/NATIVE-CONFIG-DELTA.patch').write_text(delta)
feature_names = ['HAVE_STRCPY_S', 'HAVE_STRLCPY', 'HAVE_GETLINE', 'HAVE_DECL_GETLINE', 'HAVE_FGETLN', 'HAVE_DECL_FGETLN',
                 'HAVE_FLOCKFILE', 'HAVE_DECL_FLOCKFILE', 'HAVE_FUNLOCKFILE', 'HAVE_DECL_FUNLOCKFILE', 'HAVE_TCP_VAR_H',
                 'HAVE_KQUEUE', 'HAVE_INOTIFY', 'HAVE_GETDOMAINNAME', 'HAVE_SENDFILE_4', 'HAVE_SENDFILE_6', 'HAVE_SHM_OPEN_THAT_WORKS_WELL_ENOUGH_WITH_MMAP']
features = {name: bool(re.search(r'^#define ' + name + r' 1$', config.read_text(), re.M)) for name in feature_names}
source_contracts = ['mono/native/mono-native-platform.c', 'mono/native/mono-native-platform.h', 'mono/native/pal-icalls.c',
                    'mono/native/pal-icalls.h', 'mono/native/platform-type.c', 'data/config',
                    'mcs/class/System/corefx/Unix/Interop.cs', 'mcs/class/System/corefx/Unix/Interop.Read.cs']
receipt = dict(verdict='NATIVE_ARCHIVE_INPUT_EXPORT_PROVIDER_FORMAT_AUDIT_PASS',
               target_execution=False, arbitrary_managed_DLL_compatibility_proven=False,
               archive=dict(path=str(ARCHIVE), sha256=sha(ARCHIVE), bytes=ARCHIVE.stat().st_size, elf_members=len(members)),
               compiled_source_count=len(compiled), compiled_source_changes_relative_to_frozen_parent=changed,
               exact_compiler_dependency_file_count=len(bindings), selected_header_SystemNative_declaration_count=len(declared),
               exported_SystemNative_count=len(exports), missing_selected_header_exports=missing,
               missing_export_reason='Upstream HAVE_TCP_VAR_H is false; pal_networkstatistics.c deliberately emits no implementation',
               target_features=features, source_contracts={name: sha(WORK / name) for name in source_contracts},
               native_constructor_destructor_sections=constructors,
               allocated_section_relocations_by_numeric_type=dict(relocations), allocated_absolute32_or_32S_relocations=0,
               initialization='Actual managed Interop.Sys static constructor DllImports mono_pal_init from System.Native; it registers real Interop/Sys::Read icall using atomic once state',
               test_only_initialization='mono_native_initialize registers MonoNativePlatform test counter icall; not general Mono runtime initialization',
               library_lifetime='Keep native code/data alive until runtime cleanup and all managed/native calls and registered icalls have stopped; init records function pointers in Mono',
               config='Generated System.Native dllmap still names $mono_libdir/libmono-native.so; static archive has no OS-loadable helper and does not satisfy this mapping by itself',
               packaging='Claude must choose and verify target shared helper packaging or reviewed owned static provider mapping; neither is implemented by this proof',
               dynamic_undefined_symbol_count=len(providers), public_import_symbol_count=len(providers) - len(unprovided),
               unresolved_weak_sdk_hooks=unprovided, strong_imports_without_provider=[],
               weak_hook_behavior='Actual SDK-linked dl* wrappers check absent weak hooks and return NULL/-1; SDK mprotect PROT_EXEC branch calls kernel_mprotect without a null check. Platform/loader integration must account for this.',
               public_import_libraries=provider_inputs,
               parent_source_provenance_sha256=sha(HERE.parent / 'cap10-port-closure/evidence/SOURCE-INTEGRITY.json'),
               sdk_provenance_sha256=sha(HERE.parent / 'cap10-port-research/LOCAL-INVENTORY.json'))
(HERE / 'evidence/NATIVE-AUDIT.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({key: receipt[key] for key in ['verdict', 'archive', 'compiled_source_count', 'exported_SystemNative_count', 'missing_selected_header_exports', 'public_import_symbol_count']}, indent=2))

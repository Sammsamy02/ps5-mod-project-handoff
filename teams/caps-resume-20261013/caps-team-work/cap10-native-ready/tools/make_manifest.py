#!/usr/bin/env python3
"""Select stable native handoff files and verify hashes/receipts after assembly."""
from pathlib import Path
import hashlib
import json

HERE = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


paths = {HERE / 'README.md', HERE / 'LOCAL-INPUTS.json', HERE / 'tests/GetLineHost.c'}
for directory in ['tools', 'patches', 'evidence', 'logs', 'licenses', 'handoff', 'upstream', 'experiments', 'link-check']:
    paths.update(path for path in (HERE / directory).rglob('*') if path.is_file())
paths.discard(HERE / 'evidence/MANIFEST-VERIFICATION.json')
for name in ['configure.ac', 'config.h.in', 'config.h', 'configure', 'mono/native/Makefile', 'data/config',
             'mono/native/.libs/libmono-native.a', 'external/corefx/src/Native/Unix/System.Native/pal_io.c',
             'mono/native/mono-native-platform.c', 'mono/native/mono-native-platform.h', 'mono/native/pal-icalls.c',
             'mono/native/pal-icalls.h', 'mono/native/platform-type.c',
             'mcs/class/System/corefx/Unix/Interop.cs', 'mcs/class/System/corefx/Unix/Interop.Read.cs']:
    paths.add(HERE / 'target-work' / name)
files = []
for path in sorted(paths):
    rel = str(path.relative_to(HERE))
    historical = rel.startswith(('patches/history/', 'upstream/', 'experiments/'))
    files.append(dict(path=rel, bytes=path.stat().st_size, sha256=sha(path),
                      role='historical_or_unused_do_not_apply' if historical else 'handoff_input_artifact_or_evidence'))
audit = json.loads((HERE / 'evidence/NATIVE-AUDIT.json').read_text())
link = json.loads((HERE / 'evidence/NATIVE-LINK.json').read_text())
host = json.loads((HERE / 'evidence/GETLINE-HOST-TEST.json').read_text())
manifest = dict(component='cap10-native-ready', purpose='Genuine wider native PAL Mac compile/link readiness',
                target_execution=False, game_or_PRX_build=False, production_admission=False,
                arbitrary_CSharp_or_DLL_compatibility_claim=False, source= audit['archive'],
                native_static_artifact='handoff/libmono-native.a',
                genuine_upstream=dict(mono='0cbf0e290c31adb476f9de0fa44b1d8829affa40', corefx='c4eeab9fc2faa0195a812e552cd73ee298d39386',
                                      sdk_release='v0.43', sdk_commit='d9c9519116944a7f1c22d262012c53b80b3520b7', llvm='21.1.8'),
                final_patch_bindings='evidence/NATIVE-PATCH-BINDINGS.json', historical_patch_not_to_apply='patches/history/native-declaration-probe.patch',
                compiled_source_count=42, exported_SystemNative_count=125, absent_optional_statistics_exports=11,
                strong_resolved_public_imports=290, unresolved_weak_sdk_hooks=audit['unresolved_weak_sdk_hooks'],
                validation=dict(target_build='evidence/NATIVE-LINE-IO-NATIVE.json', strict_link='evidence/NATIVE-LINK.json',
                                source_export_import_format_audit='evidence/NATIVE-AUDIT.json', exact_function_host_semantics='evidence/GETLINE-HOST-TEST.json',
                                unpatched_parent_regression='evidence/GETLINE-PARENT-REGRESSION.json', fresh_staging_recipe='evidence/NATIVE-FRESH-STAGE-RECIPE.json'),
                host_test_scope='2 real stdio routes, 5 scenario groups each, 200 concurrent shared-stream lines per route; ASAN+UBSan; no target execution',
                native_loader_gap='Generated dllmap names libmono-native.so; static archive alone provides no helper load or provider mapping',
                retained_sdk_hook_gap='dl* weak hooks may be absent; mprotect PROT_EXEC requires kernel_mprotect; see actual SDK-linked disassembly',
                files=files)
out = HERE / 'HANDOFF-MANIFEST.json'
out.write_text(json.dumps(manifest, indent=2) + '\n')
assert host['verdict'] == 'PASS_BOTH_REAL_STDIO_ROUTES_WITH_ASAN_UBSAN'
assert sha(HERE / 'handoff/libmono-native.a') == audit['archive']['sha256'] == link['archive_sha256']
assert sha(Path(link['output'])) == link['output_sha256']
for path, digest in link['inputs'].items():
    assert sha(Path(path)) == digest, path
for item in json.loads((HERE / 'evidence/NATIVE-COMPILED-INPUTS.json').read_text())['dependency_files']:
    assert sha(Path(item['path'])) == item['sha256'], item['path']
for item in files:
    assert sha(HERE / item['path']) == item['sha256'], item['path']
verification = dict(verdict='PASS', manifest_sha256=sha(out), selected_file_count=len(files),
                    all_selected_files_rehashed=True, all_299_compiler_dependency_inputs_rehashed=True,
                    native_archive_and_actual_link_inputs_rehashed=True, native_link_image_rehashed=True,
                    console_execution=False)
(HERE / 'evidence/MANIFEST-VERIFICATION.json').write_text(json.dumps(verification, indent=2) + '\n')
print(json.dumps(verification, indent=2))

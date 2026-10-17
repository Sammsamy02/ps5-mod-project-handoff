#!/usr/bin/env python3
"""Freeze selected source/build/host evidence after root's independent review."""
from pathlib import Path
import argparse, hashlib, json

BASE = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--review', required=True, help='Exact root review receipt or message reference.')
    args = parser.parse_args()
    review = json.loads((BASE / args.review).read_text())
    assert review['owner_runtime_pair_required'] and not review['v1_runtime_integration_accepted']
    assert all(sha(BASE / path) == digest for path, digest in review['accepted_final_sha256'].items())
    manifest = BASE / 'HANDOFF-MANIFEST.json'
    assert not manifest.exists(), 'Selected packet is frozen; preserve the manifest.'
    target = json.loads((BASE / 'evidence/target-owner-compile.json').read_text())
    runtime = json.loads((BASE / 'evidence/runtime-build-v2.json').read_text())
    audit = json.loads((BASE / 'evidence/runtime-archive-audit-v2.json').read_text())
    assert target['verdict'] == 'TARGET_OWNER_COMPILE_PASS' and target['inputs_unchanged']
    assert runtime['verdict'] == 'GENUINE_MONO_SIGNAL_UNIT_BUILD_PASS' and runtime['frozen_inputs_unchanged']
    assert audit['verdict'] == 'ONE_GENUINE_MEMBER_CHANGED_PASS'
    labels = ['baseline', 'chaining', 'runtime-fixed', 'fixed', 'pipe-replaced', 'default-pipe', 'ignore-pipe', 'fixed-v2', 'pipe-replaced-v2', 'default-pipe-v2', 'ignore-pipe-v2']
    for label in labels:
        case = json.loads((BASE / 'evidence' / (label + '.json')).read_text())
        assert case['run_passed'] and case['throw_passed'] and case['recovery_passed']
        assert case['real_managed_runs'] == 2 and case['real_managed_exceptions'] == 1
        assert case['signals_raised'] == 0 and case['runtime_diagnostics_clean'] and case['static_corlib_trace_observed']
        assert case['exit_code'] == (1 if label in ['baseline', 'chaining', 'runtime-fixed'] else 0)
        assert case['completed'] == (label.removesuffix('-v2') != 'pipe-replaced')
    dispatch = json.loads((BASE / 'evidence/dispatch-v2.json').read_text())
    assert dispatch['verdict'] == 'GENUINE_SIGNAL_DISPATCH_PASS' and dispatch['direct_genuine_runtime_dispatch'] and dispatch['inputs_unchanged'] and dispatch['runtime_diagnostics_clean']
    integration = {
        'version': 1, 'owner_and_runtime_must_ship_together': True,
        'root_review': args.review,
        'files': {str(path.relative_to(BASE)): sha(path) for path in [BASE / 'native/MonoOwner.cpp', BASE / 'native/MonoOwner.hpp', BASE / 'runtime-source/mono/mini/mini-posix.c', BASE / 'runtime-target-v2/libmonosgen-2.0.a', BASE / 'runtime-target/MonoOwner.o', BASE / 'runtime-target-v2/libmini_la-mini-posix.o', BASE / 'runtime-host-v2/libmonosgen-2.0.1.dylib', BASE / 'evidence/owner-signal-lifecycle.patch', BASE / 'evidence/mono-signal-lifecycle.patch']},
        'target_archive_replaced_member': 'libmini_la-mini-posix.o',
        'target_archive_original_sha256': runtime['outputs']['target']['original_archive_sha256'],
        'target_compile_only': True, 'target_execution': False, 'production_admission': False,
        'caller_requirement': 'Sole Mono owner; same init/cleanup thread; exclusive changes to affected signal actions and that thread mask throughout the synchronous call.',
        'limits': ['No actual signal delivery, native-fault chaining or target signal-state behavior is proved.', 'No concurrent signal-action mutation support; POSIX query/restore is not atomic.', 'No recoverable failed/fatal partial initialization claim.', 'Direct genuine runtime dispatch is proved on host; actual OS signal delivery and console behavior remain unproved.']
    }
    (BASE / 'INTEGRATION-INPUTS.json').write_text(json.dumps(integration, indent=2) + '\n')
    selected = {BASE / 'README.md', BASE / 'INTEGRATION-INPUTS.json'}
    for folder in ['native', 'runtime-source', 'runtime-source-v1', 'tools', 'tests', 'evidence', 'logs']:
        selected.update(path for path in (BASE / folder).rglob('*') if path.is_file())
    for label in labels:
        selected.add(BASE / 'objects' / label / 'SignalHarness.cpp')
    selected.add(BASE / 'objects/dispatch-v2/DispatchHarness.cpp')
    selected.update(BASE / path for path in ['runtime-host-v2/libmini.a', 'runtime-host-v2/libmini_la-mini-posix.o', 'runtime-host-v2/libmonosgen-2.0.1.dylib', 'runtime-target-v2/libmonosgen-2.0.a', 'runtime-target-v2/libmini_la-mini-posix.o', 'runtime-target/MonoOwner.o'])
    files = [{'path': str(path.relative_to(BASE)), 'bytes': path.stat().st_size, 'sha256': sha(path)} for path in sorted(selected)]
    frozen_inputs = {path: digest for path, digest in runtime['inputs_sha256'].items()}
    for label in labels:
        case = json.loads((BASE / 'evidence' / (label + '.json')).read_text())
        frozen_inputs.update({path: digest for path, digest in case['inputs_sha256'].items() if not Path(path).is_relative_to(BASE)})
    frozen_inputs.update({path: digest for path, digest in dispatch['input_sha256'].items() if not Path(path).is_relative_to(BASE)})
    assert all(sha(Path(path)) == digest for path, digest in frozen_inputs.items())
    content = {'version': 1, 'status': 'reviewed_selected_handoff_frozen', 'root_review': args.review, 'files': files, 'count': len(files), 'selected_bytes': sum(item['bytes'] for item in files), 'external_frozen_inputs_sha256': frozen_inputs, 'external_frozen_inputs_unchanged': True, 'no_target_execution': True, 'no_prx_build': True, 'no_production_admission': True, 'excluded': ['V1 runtime binary artifacts are historical and unselected for integration', 'Raw host executables in bin/ (preserved in own folder; receipts retain exact hashes)', 'Intermediate .o files except selected changed-unit/target-owner objects', 'Raw archive-audit extraction trees', 'Frozen original runtime/header/SDK/corlib source trees (parent packet must retain their exact pinned dependencies)']}
    manifest.write_text(json.dumps(content, indent=2) + '\n')
    assert all(sha(BASE / item['path']) == item['sha256'] for item in files)
    print(json.dumps({'manifest': str(manifest), 'manifest_sha256': sha(manifest), 'selected_files': len(files), 'selected_bytes': content['selected_bytes'], 'all_selected_rehashed': True, 'external_inputs_rehashed': len(frozen_inputs)}, indent=2))

if __name__ == '__main__':
    main()

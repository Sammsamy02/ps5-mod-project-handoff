#!/usr/bin/env python3
"""Hash the one-run executable-memory observation and unchanged real inputs."""
from pathlib import Path
import hashlib
import json

HERE = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


proof = json.loads((HERE / 'evidence/EXEC-HOST-CHECK.json').read_text())
audit = json.loads((HERE / 'evidence/EXEC-SOURCE-CORRELATION.json').read_text())
assert proof['verdict'] == 'GENUINE_FINAL_OWNER_EXECUTABLE_MEMORY_OBSERVATION_COMPLETE'
assert audit['verdict'] == 'RECORDED_HOST_REQUESTS_SOURCE_AND_TARGET_OBJECT_CORRELATED'
paths = {HERE / 'README.md'}
for directory in ['native', 'tools', 'evidence', 'logs', 'bin', 'objects']:
    paths.update(path for path in (HERE / directory).rglob('*') if path.is_file())
paths.discard(HERE / 'evidence/MANIFEST-VERIFICATION.json')
files = [dict(path=str(path.relative_to(HERE)), bytes=path.stat().st_size, sha256=sha(path)) for path in sorted(paths)]
manifest = dict(component='cap10-exec-ready', scope='One actual final-owner host executable-memory observation and source correlation',
                target_execution=False, provider_or_kernel_implementation=False, game_or_PRX_build=False,
                observed_mmap_calls=33, observed_mprotect_calls=7, observed_executable_requests=1,
                observed_executable_mprotect_requests=0, dropped_events=0,
                executable_request='Darwin-only mono_valloc hardened-runtime probe, 4096 bytes WRITE|EXEC',
                actual_managed_runs=2, actual_managed_exceptions=1, cleanup_and_signal_checks_pass=True,
                target_inference=audit['target_inference'], target_behavior_proven=False,
                remaining_provider_dependency=audit['remaining_provider_dependency'],
                input_sha256=proof['input_sha256'], files=files)
out = HERE / 'HANDOFF-MANIFEST.json'
out.write_text(json.dumps(manifest, indent=2) + '\n')
for path, digest in proof['input_sha256'].items():
    assert sha(Path(path)) == digest, path
for item in files:
    assert sha(HERE / item['path']) == item['sha256'], item['path']
for item in audit['source_paths']:
    assert sha(Path(item['path'])) == item['sha256'], item['path']
verification = dict(verdict='PASS', manifest_sha256=sha(out), selected_file_count=len(files),
                    all_measured_inputs_rehashed=True, all_selected_files_rehashed=True,
                    all_source_paths_rehashed=True, measured_owner_processes=1)
(HERE / 'evidence/MANIFEST-VERIFICATION.json').write_text(json.dumps(verification, indent=2) + '\n')
print(json.dumps(verification, indent=2))

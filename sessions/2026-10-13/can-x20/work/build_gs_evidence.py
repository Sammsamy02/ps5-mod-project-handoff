"""Package source-pinned, read-only GS audit observations for review."""

import hashlib
import json
import sys
from pathlib import Path

TREE = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
STAGE = TREE / 'cg-gs-aligned-20261013-08'
TOOL = STAGE / 'TOOL'
LOADER = TREE / 'cg-gs-guard-20261013-07'
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-GS-ACCESS-AUDIT-ALIGNED-20261013.json')
sys.path.insert(0, str(TOOL))

from admission_dossier import load_registry, make_dossier, verify_source_pins  # noqa: E402
from pe_analyzer import analyze_file  # noqa: E402

names = [
    ('TrueFOV', STAGE / 'SAMPLE/TrueFOV-upstream-winmm.dll'),
    ('ZeroCompanyMandoWardrobe', TREE / 'cg-max-ready-20261013-03/cap13/upstream-inputs/ZeroCompanyMandoWardrobe-upstream-main.dll'),
    ('TFWWorkbench', TREE / 'cg-max-ready-20261013-03/cap13/upstream-inputs/TFWWorkbench-upstream-main.dll'),
    ('EventViewerMod', TREE / 'cg-max-ready-20261013-03/cap13/upstream-inputs/EventViewerMod-upstream-main.dll'),
    ('KismetDebuggerMod', TREE / 'cg-max-ready-20261013-03/cap13/upstream-inputs/KismetDebuggerMod-upstream-main.dll'),
    ('empty-TLS feature probe', TREE / 'cap13/SAMPLE/cap13-feature-probe.dll'),
    ('no-TLS dwmapi proxy', Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/windows-image-original-inputs-1/dwmapi.dll')),
    ('no-GS control', TREE / 'cap13/SAMPLE/real-worker-standard.dll'),
]
registry = load_registry(TOOL / 'loader_capabilities.json')
verify_source_pins(registry, LOADER)
rows = []
for label, path in names:
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    _, report = analyze_file(str(path))
    after = hashlib.sha256(path.read_bytes()).hexdigest()
    assert before == after == report['sha256'], label
    dossier = make_dossier(report, registry, source_pins_verified=True)
    rows.append({
        'label': label, 'source_path': str(path), 'sha256': before,
        'tls_status': report['tls']['status'], 'gs_access': report['gs_access'],
        'offline_gate': dossier['offline_gate'],
        'blockers': [item['feature'] for item in dossier['blockers']],
        'source_unchanged': True,
    })
assert all(row['gs_access']['candidate_count'] > 0 for row in rows[:7])
assert rows[5]['tls_status'] == 'empty' and rows[6]['tls_status'] == 'none'
assert rows[7]['gs_access']['candidate_count'] == 0
payload = {
    'scope': 'linear sweep of file-backed executable PE sections; not reachability or absence proof',
    'decoder': 'Capstone 5.0.7 x86-64, skipdata enabled',
    'loader_source_pins_verified': True,
    'analyzer_sha256': hashlib.sha256((TOOL / 'pe_analyzer.py').read_bytes()).hexdigest(),
    'gs_audit_sha256': hashlib.sha256((TOOL / 'gs_audit.py').read_bytes()).hexdigest(),
    'rows': rows,
}
OUT.write_text(json.dumps(payload, indent=2) + '\n')
print(f'wrote {OUT}; {len(rows)} source hashes unchanged')

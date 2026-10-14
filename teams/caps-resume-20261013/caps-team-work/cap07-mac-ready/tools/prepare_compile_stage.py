"""Explicit, narrow dependency-pin correction for root review; owned output only."""
import sys
sys.dont_write_bytecode = True
import argparse, importlib.util, json
from pathlib import Path

HOME = Path(__file__).resolve().parent.parent
ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json'

def rebind(derived, decision):
    spec = importlib.util.spec_from_file_location('integration', HOME / 'tools/build_integration.py')
    integration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(integration)
    metadata = json.loads(derived[REL])
    rows = decision['headers']
    if len(rows) != 2 or {Path(row['path']).name for row in rows} != {'NativePropertyReadBackend.inl', 'LuaFNameBridge.hpp'}:
        raise ValueError('expected the two audited stale dependency pins')
    for row in rows:
        path = row['path']
        snapshot = (HOME / row['snapshot']).resolve()
        if not snapshot.is_relative_to(HOME / 'evidence/header-audit'):
            raise ValueError('owned immutable audited header snapshot required')
        if path not in metadata['added_headers'] or metadata['added_header_hashes'].get(path) != row['declared_sha256']:
            raise ValueError('audited prior declared pin mismatch')
        data = snapshot.read_bytes()
        if len(data) != row['bytes'] or integration.sha(data) != row['actual_sha256']:
            raise ValueError('audited header snapshot drift')
        if Path(path).read_bytes() != data:
            raise ValueError('live header changed since audit: new review required')
        metadata['added_header_hashes'][path] = row['actual_sha256']
    result = dict(derived)
    result[REL] = (json.dumps(metadata, indent=2) + '\n').encode()
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    if out.parent != HOME / 'derived' or out.exists():
        raise ValueError('fresh owned derived stage required')
    spec = importlib.util.spec_from_file_location('integration', HOME / 'tools/build_integration.py')
    integration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(integration)
    decision_path = HOME / 'evidence/DEPENDENCY-REBIND.json'
    decision = json.loads(decision_path.read_bytes())
    derived, originals, census = integration.generate(ROOT)
    derived = rebind(derived, decision)
    out.mkdir()
    for rel, data in derived.items():
        path = out / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    patch = integration.integration_patch(derived, originals).encode()
    (out / 'integration.patch').write_bytes(patch)
    manifest = {'label': 'Complete #7 source/build stage plus two explicitly audited current dependency-pin corrections; root admission pending',
                'originals': [{'path': p, 'bytes': len(b), 'sha256': integration.sha(b)} for p, b in originals.items()],
                'files': [{'path': p, 'bytes': len(b), 'sha256': integration.sha(b)} for p, b in derived.items()],
                'cache_consumers': census, 'new_units': 5, 'expected_link_inputs': 396,
                'dependency_rebind': {'path': str(decision_path), 'sha256': integration.sha(decision_path.read_bytes()), 'root_admission': 'pending'},
                'target_build': False, 'execute_driver_main': False, 'policy': 'One combined full next-launch build; incremental prohibited'}
    (out / 'INTEGRATION.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared', len(derived), 'staged files; two explicit dependency pins corrected for review')

if __name__ == '__main__':
    main()

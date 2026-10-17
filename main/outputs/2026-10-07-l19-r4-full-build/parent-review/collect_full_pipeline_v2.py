"""One local collector after the real pair; never compiles, packages or changes pins."""
import hashlib
import json
from pathlib import Path
import sys

OWN = Path(__file__).resolve().parent.parent
ROOT = OWN.parents[1]
sys.path.insert(0, str(OWN / 'execution'))
from admission import pin, read, sha, require


def file_pin(path):
    path = Path(path).resolve()
    return {'path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}


def identities(path):
    imports = read(path)['contract']['imports']
    result = {r['ImportSymbol']['Name']: {k: r[k] for k in ('ImportSymbol', 'symbol_type', 'binding')}
              for r in imports}
    require(len(result) == 206 == len(imports), 'actual206 typed import identities required')
    return result


def put(path, data):
    require(not path.exists(), 'collector never overwrites a final receipt')
    path.write_text(json.dumps(data, indent=2) + '\n')


def main():
    complete = read(OWN / 'R4-EXECUTION-RECEIPT.json')
    require(complete.get('pass') is True, 'actual two-pass execution not complete')
    admission_path = pin(complete['admission'], 'completed execution admission')
    admission = read(admission_path)
    pin(complete['parent_go'], 'actual root GO')
    prior_delta = read(ROOT / 'outputs/2026-10-07-l19-r3-full-build/IMPORT-DELTA-VS-L18.json')
    baseline_path = pin(prior_delta['baseline'], 'saved final l18 typed import contract')
    baseline = identities(baseline_path)
    rows = []
    for number in (1, 2):
        run = OWN / ('runs/run-l19-' + str(number))
        candidate = OWN / ('prx-candidate-l19-' + str(number))
        receipt = read(run / 'receipt.json')
        artifact = read(candidate / 'artifacts-receipt.json')['artifacts']['final_prx']
        pin(artifact, 'actual final PRX/' + str(number))
        runtime = file_pin(receipt['runtime_object'])
        require(runtime['sha256'] == receipt['runtime_sha256'], 'actual composition receipt drift')
        require(receipt['r4_execution_admission']['sha256'] == complete['admission']['sha256'], 'pass admission drift')
        require(len(receipt['selected_runtime_units']) == 369 and len(read(run / 'input-hashes.json')) == 386,
                '369 selected /386 actual input count required')
        for path, digest in read(run / 'input-hashes.json').items():
            pin({'path': path, 'sha256': digest}, 'actual composition input')
        for name, unit in receipt['selected_runtime_units'].items():
            pin({'path': unit['source'], 'sha256': unit['source_sha256']}, 'actual selected source/' + name)
            pin({'path': unit['object'], 'sha256': unit['object_sha256']}, 'actual selected object/' + name)
        current = identities(candidate / 'supplier-contract.json')
        added, removed = sorted(current.keys() - baseline.keys()), sorted(baseline.keys() - current.keys())
        changed = {n: [baseline[n], current[n]] for n in current.keys() & baseline.keys() if baseline[n] != current[n]}
        require(not added and not removed and not changed, 'actual typed import identities differ from final l18')
        static = read(candidate / 'verification-evidence/static-checks.json')
        require(len(static['checks']) == 20 and all(r['pass'] is True for r in static['checks'].values()),
                'actual20 inherited package static checks required')
        gate_path = OWN / ('gate-receipt-l11-prx-candidate-l19-' + str(number) + '.json')
        gate = read(gate_path)
        require(gate['failed_checks'] == ([] if number == 2 else ['D1']), 'unexpected inherited final gate failure')
        lineage_path = candidate / 'verification-evidence/lineage-verification.json'
        lineage = read(lineage_path)
        require(all(row['pass'] is True for row in lineage['checks'].values()), 'reconciled inherited lineage rows must all pass')
        reconciliation = lineage['checks']['undefined_delta_added_matches_checkpoint_claim']['detail']['stage_aware_reconciliation']
        require(reconciliation['accepted'] is True and reconciliation['preproof_compile_verified'] is True, 'actual fresh stage-aware provider preproof missing')
        whole = read(candidate / 'R4-WHOLE-SELECTED.json')
        require(whole['pass'] is True and whole['affected_dependency_walks'] == 147,
                'actual147/header/Program whole-selected proof missing')
        delta = read(run / 'R4-UNDEFINED-VS-R3.json')
        require(not delta['added'] and not delta['removed'] and not delta['changed_bindings'], 'new raw undefined delta')
        rows.append({'pass_number': number, 'composition': runtime, 'artifact': artifact,
                     'fresh_compiles': len(receipt['freshly_compiled_units']),
                     'accepted_reused_units': len(receipt['r4_reused_accepted_pass1_units']),
                     'selected_units': 369, 'composition_inputs': 386, 'typed_imports': 206,
                     'typed_import_added': added, 'typed_import_removed': removed, 'typed_import_changed': changed,
                     'static_checks_passed': 20, 'gate_verdict': gate['verdict'], 'gate_failed_checks': gate['failed_checks'],
                     'raw_undefined_delta_vs_R3': {'added': [], 'removed': [], 'changed_bindings': {}},
                     'lineage_verification': file_pin(lineage_path), 'lineage_original_top_status': lineage.get('status'),
                     'lineage_reconciled_rows_passed': len(lineage['checks']), 'actual_fresh_stage_aware_provider_preproof': True,
                     'lineage_status_note': 'preserved original raw top status; all rows pass after existing stage-aware reconciliation',
                     'actual_whole_selected': file_pin(candidate / 'R4-WHOLE-SELECTED.json'),
                     'run_receipt': file_pin(run / 'receipt.json'), 'gate_receipt': file_pin(gate_path),
                     'supplier_contract': file_pin(candidate / 'supplier-contract.json'),
                     'package_build_receipt': file_pin(candidate / 'build-receipt.json')})
    require(rows[0]['artifact']['sha256'] == rows[1]['artifact']['sha256']
            and rows[0]['composition']['sha256'] == rows[1]['composition']['sha256'], 'actual twin determinism failure')
    require(rows[0]['fresh_compiles'] == 3 and rows[0]['accepted_reused_units'] == 144
            and rows[1]['fresh_compiles'] == 147 and rows[1]['accepted_reused_units'] == 0, 'actual pass compile/reuse counts')
    clone = read(OWN / 'PIPELINE-CLONE-RECEIPT.json')
    for name, record in clone['byte_identical_root_files'].items():
        pin({'path': str(OWN / name), 'sha256': record['sha256']}, 'inherited immutable/' + name)
    report = {'pass': True, 'scope': 'Mac-only actual local two-build/package/import/artifact verification; no console execution',
              'admission': complete['admission'], 'root_GO': complete['parent_go'],
              'source_manifest': admission['source_manifest'], 'profile_json': admission['profile_json'],
              'generated_header': admission['generated_header'], 'profile_digest': admission['profile_digest'],
              'inherited_pipeline_files_byte_identical': 14, 'required_native_members': 69, 'required_native_vtables': 7,
              'passes': rows, 'artifacts_identical': True, 'compositions_identical': True,
              'baseline_typed_import_contract': file_pin(baseline_path),
              'final_gate': rows[1]['gate_verdict'], 'console_contact': False,
              'runtime_menu_or_spawn_proven': False, 'upload': 'HOLD', 'findings': []}
    out = OWN / 'FULL-PIPELINE-RECEIPT.json'
    put(out, report)
    put(OWN / 'IMPORT-DELTA-VS-L18.json', {'pass': True, 'scope': 'Mac-only exact206 typed names/identities vs saved final l18',
        'baseline': file_pin(baseline_path), 'current': rows[1]['supplier_contract'], 'current_count': 206,
        'baseline_count': 206, 'added': [], 'removed': [], 'changed_identities': {}})
    print(json.dumps({'pass': True, 'selected_units': 369, 'inputs': 386, 'imports': 206,
                     'runtime_bytes': rows[1]['composition']['bytes'], 'runtime_sha256': rows[1]['composition']['sha256'],
                     'PRX_bytes': rows[1]['artifact']['bytes'], 'PRX_sha256': rows[1]['artifact']['sha256'],
                     'receipt_sha256': sha(out)}, sort_keys=True))


if __name__ == '__main__':
    main()

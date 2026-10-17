"""One bounded actual lineage diagnostic; never compiles, links or packages."""
import json
import os
from pathlib import Path
import sys
import types

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN))
sys.path.insert(0, str(OWN / 'execution'))
from admission import read, pin, require, sha
from composition import Composer, historical_delta, sdk_nm_evidence, write
import package_prx_l19 as package
import prep_guard


def main():
    run = OWN / 'parent-review/lineage-preflight-v1'
    require(not run.exists(), 'new preflight directory required'); run.mkdir()
    original = OWN / 'failed-attempt-4/runs/run-l19-1/receipt.json'
    receipt = read(original)
    data = read(OWN / 'parent-review/EXECUTION-ADMISSION.FINAL.v3.json')
    recipes = read(pin(data['affected_recipes'], 'exact affected recipes'))
    base = read(pin(recipes['base_receipt'], 'actual R3 recomposition base'))
    nm = pin(data['nm'], 'actual nm')
    source = original.parent / 'coherent-tick-file-witness-runtime.o'
    require(sha(source) == receipt['runtime_sha256'], 'preserved actual composition drift')
    target = run / source.name
    os.link(source, target)
    command = Composer(types.SimpleNamespace()).command
    env = prep_guard.local_environment()
    result = command('actual-undefined', [str(nm), '--extern-only', '--undefined-only', '--format=posix', str(target)], run, env)
    current = {r[0]: r[1] for r in (line.split() for line in result['stdout'].splitlines()) if len(r) >= 2}
    require(current == base['undefined_delta']['current'], 'actual R4-vs-R3 typed delta')
    delta = historical_delta(base, current)
    sdk = sdk_nm_evidence(base, nm, command, run, env)
    repaired = dict(receipt, base_receipt=base['base_receipt'], base_receipt_sha256=base['base_receipt_sha256'],
                    r4_recomposition_base_receipt=recipes['base_receipt'], r4_sdk_provider_evidence=sdk,
                    runtime_object=str(target), undefined_delta=delta)
    write(run / 'receipt.json', repaired)
    write(run / 'undefined-delta.json', delta)
    package.PIN.update(composition_dir=run, composition_object_name=target.name,
                       composition_object_sha256=sha(target), composition_object_bytes=target.stat().st_size,
                       composition_checkpoint='zcode_supplier_composition_6',
                       ledger=OWN / 'transport-ledger.composition6-mirror.json',
                       lineage=OWN / 'staging/lineage-overlay-2')
    build = package.Build(run / 'evidence', link=False)
    raw = package.verify_lineage(build)
    failed = [n for n, row in raw['checks'].items() if not row['pass']]
    expected = 'undefined_delta_added_matches_checkpoint_claim'
    require(failed == [expected], 'unexpected original raw lineage failure: ' + str(failed))
    resolutions = sorted(delta['expected_bootstrap_resolutions'])
    require(sorted(delta['added']) == sorted({'sceKernelGetModuleInfo', 'sceKernelVirtualQuery'} | set(resolutions)),
            'original stage-aware delta predicate')
    require(not delta['removed'] and not delta['changed_bindings'], 'actual cumulative lineage changes')
    write(run / 'ORIGINAL-RAW-LINEAGE.json', raw)
    write(run / 'RECEIPT.json', {'pass': True,
        'scope': 'actual original raw lineage compatibility diagnostic; no compiler/linker/packager invoked',
        'original_composition_receipt': {'path': str(original), 'sha256': sha(original)},
        'repaired_test_receipt': {'path': str(run / 'receipt.json'), 'sha256': sha(run / 'receipt.json')},
        'actual_runtime': {'path': str(target), 'sha256': sha(target), 'bytes': target.stat().st_size},
        'actual_current_typed_undefined': len(current), 'actual_historical_current': 197,
        'measured_cumulative_added': delta['added'], 'measured_cumulative_removed': [],
        'measured_cumulative_binding_changes': {}, 'immediate_R4_vs_R3_delta': {'added': [], 'removed': [], 'changed_bindings': {}},
        'fresh_actual_SDK_nm_evidence': sdk,
        'original_raw_lineage': {'path': str(run / 'ORIGINAL-RAW-LINEAGE.json'), 'sha256': sha(run / 'ORIGINAL-RAW-LINEAGE.json')},
        'original_raw_checks_passed': len(raw['checks']) - 1, 'original_raw_failed_checks': failed,
        'unchanged_stage_aware_delta_predicate': True,
        'fresh_stage_aware_provider_compile_proof': 'PENDING actual inherited packaging under next root GO',
        'full_packager_lineage_or_package_PASS_claimed': False,
        'compiler_invocations': 0, 'linker_invocations': 0, 'package_invocations': 0, 'console_contact': False})
    print(json.dumps({'pass': True, 'checks_passed': len(raw['checks']) - 1,
                      'expected_raw_checkpoint_discrepancy': failed, 'fresh_SDK_captures': len(sdk),
                      'receipt_sha256': sha(run / 'RECEIPT.json')}))


if __name__ == '__main__':
    main()

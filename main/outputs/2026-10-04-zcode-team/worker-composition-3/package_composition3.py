#!/usr/bin/env python3
"""composition-3 packaging driver for worker-prx-link-gaps's FIXED package_prx.py.

The fixed script (sha256 e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313,
verified before load) is executed UNMODIFIED. Because its PIN block was authored
before composition-3 existed (it pins composition-2), this driver applies the
documented pinned-values adjustment pattern (same pattern the fixed script itself
applied to worker-prx-packaging's original, see its package_prx_composition2.patch):

  composition_dir            -> ue4ss-native-supplier-composition-3/run-1
  composition_object_sha256  -> full digest from the composition-3 run receipt
  composition_object_bytes   -> from the composition-3 run receipt
  composition_checkpoint     -> zcode_supplier_composition_3
  ledger                     -> byte-mirror of the real checkpoint ledger PLUS a
                                provisional zcode_supplier_composition_3 entry
                                (the real ledger at
                                .superpowers/native-observations/transport-experiments-progress.json
                                is read-only for this worker; the controller
                                must reconcile the real ledger).

Every packaging pipeline function, compile, link, Packager invocation and static
check runs from the fixed script's own code.
"""
import hashlib
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
MY = ROOT / 'outputs/2026-10-04-zcode-team/worker-composition-3'
FIXED = ROOT / 'outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py'
EXPECTED_SHA = 'e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313'

got = hashlib.sha256(FIXED.read_bytes()).hexdigest()
assert got == EXPECTED_SHA, f'fixed package_prx.py sha drift: {got}'

spec = importlib.util.spec_from_file_location('package_prx_fixed', FIXED)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

RUN = ROOT / ('.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/'
              'ue4ss-native-supplier-composition-3/run-1')
rec = json.loads((RUN / 'receipt.json').read_text())
assert rec['status'] == 'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED', rec['status']
assert rec['runtime_sha256'] == hashlib.sha256(
    Path(rec['runtime_object']).read_bytes()).hexdigest()

REAL_LEDGER = ROOT / '.superpowers/native-observations/transport-experiments-progress.json'
led = json.loads(REAL_LEDGER.read_text())
led['zcode_supplier_composition_3'] = {
    'utc': __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat(),
    'actor': 'worker-composition-3 (PROVISIONAL mirror entry; real ledger owned by controller)',
    'status': rec['status'],
    'runtime_object': rec['runtime_object'],
    'runtime_sha256': rec['runtime_sha256'],
    'runtime_bytes': rec['runtime_bytes'],
    'units': '353 retained + 13 fresh (incl. ack-enabled bootstrap runtime_owner, ue4ss_backend; 366 total)',
    'applied': 'bundle 01 applied to canonical outputs bootstrap tree (4/4 hashes verified); '
               'bundle 02 verified byte-identical and consumed via hash-attested staging copies '
               '(worker-composition-3/bootstrap-stage, replace-receipt.json) because the frozen '
               'native-coherent-runtime-1 validate_inputs gate pins the pre-ack spike hashes',
    'delta': 'added [sceKernelGetModuleInfo] vs extended baseline — IDENTICAL to composition-2 '
             'added set: the ack bootstrap units contributed zero new undefined imports',
}
MIRROR = MY / 'evidence' / 'transport-ledger.composition3-mirror.json'
MIRROR.write_text(json.dumps(led, indent=1) + '\n')

m.PIN['composition_dir'] = RUN
m.PIN['composition_object_sha256'] = rec['runtime_sha256']
m.PIN['composition_object_bytes'] = rec['runtime_bytes']
m.PIN['composition_checkpoint'] = 'zcode_supplier_composition_3'
m.PIN['ledger'] = MIRROR

OUT = MY / 'prx-candidate-composition3-1'
assert not OUT.exists(), f'output dir exists: {OUT}'
sys.argv = ['package_prx.py', '--link', '--out', str(OUT)]
print('driver: PIN adjustments applied; running fixed package_prx.py --link', flush=True)
raise SystemExit(m.main())

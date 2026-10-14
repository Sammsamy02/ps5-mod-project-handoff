#!/usr/bin/env python3
"""p1_ac_row_updates.py — AC-time JSON row updates for P1 (fault-ledger desaturation).

Owner: agent IG (ZCode), 2026-10-10/11. Companion to round5-staging/P1/APPLY.md.
Run at AC AFTER `patch -p1 --fuzz=0 < parked/P1-fault-ledger/01-fault-ledger-desaturation.patch`
(step 2) and AFTER the step-3 sha verification. Idempotent: safe to re-run.

What it does (and refuses to do):
  1. VERIFIES the patch's own JSON work (copyin-probe/MANIFEST.json hunk 3):
     bridge candidate pin == a35c7f91..., copyinsink row present with base
     2018c9bd... / candidate 0b113b1c..., l27_fledger_note present. If any of
     these are missing/wrong it REPAIRS them (exact values below) — this covers
     an AC-time hunk-3 partial failure — but only after proving the CODE files
     byte-match FX's construction (a repaired row pointing at unpatched code
     would be a lie, so that case is a hard abort instead).
  2. Re-baselines harvest/pipeline-pins.json (the launch preflight pin source
     bind_launch.sh gate 4 enforces; NOT covered by FX's patch — found by IG
     staging census):
       pins.bridge_gated.sha256 -> a35c7f91... (was 94f81a17...)
       pins.pkgdrv_all.sha256   -> observed post-P1 driver sha (guarded: the
         driver must contain the FX fault_ledger_overlay block and py_compile;
         was ee3aa230... which was ALREADY stale pre-P1 — live driver had
         moved to 810efaae... on 2026-10-11 by the P8 238->239 rebaseline).
     Old values are printed and recorded in the receipt for rollback.
  3. Writes a receipt JSON next to itself (p1_row_update_receipt.json).

NO builds, no console contact, no other files touched.
"""
import hashlib, json, py_compile, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[3]                      # .../translation-core/what
                                              # parents: [3]=what [2]=outputs [1]=l25-prep [0]=round5-staging
L26 = WHAT / 'outputs/2026-10-08-l26-runtime'
MANIFEST = L26 / 'copyin-probe/MANIFEST.json'
PINS = WHAT / 'outputs/2026-10-08-l25-prep/harvest/pipeline-pins.json'
RECEIPT = HERE / 'p1_row_update_receipt.json'

BRIDGE_CAND = L26 / 'copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp'
SINK_CAND = L26 / 'copyin-probe/candidate/CopyinSink.hpp'
DRIVER = L26 / 'l26_packaging_driver.py'
SINK_BASE = WHAT / 'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/CopyinSink.hpp'

# FX construction pins (authoring-verified; IG re-verified byte-identical in /tmp staging)
SHA_BRIDGE_A35C = 'a35c7f9111b1f91cead80a05d489db04f6d4a25bb32b6bf0bee09879785baca1'
SHA_SINK_0B11 = '0b113b1c19b4eac7f9959567487845c8639a28937f94a4881103dcbb163e69cd'
SHA_SINK_BASE_2018 = '2018c9bdc63b8f2001aeadcc44ab698d0d5c54c08652e5fc0c78f45c0fa65d98'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def die(msg):
    print('ABORT: ' + msg)
    sys.exit(1)

def main():
    # ---- 1. code files must byte-match FX's construction -------------------
    if not BRIDGE_CAND.exists():
        die('bridge candidate not found at %s — wrong tree layout?' % BRIDGE_CAND)
    if sha(BRIDGE_CAND) != SHA_BRIDGE_A35C:
        die('bridge candidate is %s..., expected a35c7f91... — P1 hunks 1/2 not (correctly) applied; run APPLY.md step 2/3 first' % sha(BRIDGE_CAND)[:12])
    if not SINK_CAND.exists() or sha(SINK_CAND) != SHA_SINK_0B11:
        die('CopyinSink.hpp candidate missing/not 0b113b1c... — hunk 1 not applied')
    if not SINK_BASE.exists() or sha(SINK_BASE) != SHA_SINK_BASE_2018:
        die('frozen lineage CopyinSink.hpp no longer 2018c9bd... — lineage moved; reconcile before wiring')
    drv_text = DRIVER.read_text()
    if 'fault_ledger_overlay' not in drv_text or "copyinsink' not in qman['base_files']" not in drv_text:
        die('l26_packaging_driver.py does not carry the FX hunk-4 overlay wiring — hunk 4 not applied')
    with tempfile.TemporaryDirectory() as td:
        py_compile.compile(str(DRIVER), cfile=str(Path(td) / 'd.pyc'), doraise=True)
    drv_sha = sha(DRIVER)

    changed = {}

    # ---- 2. copyin-probe/MANIFEST.json (verify; repair exact rows) ---------
    man = json.loads(MANIFEST.read_text())
    bf = man.setdefault('base_files', {})
    bridge = bf.setdefault('runtime_start_bridge', {})
    bridge['candidate_path'] = str(BRIDGE_CAND)
    if bridge.get('candidate_sha256') != SHA_BRIDGE_A35C:
        changed['manifest.bridge_candidate_sha256'] = {'old': bridge.get('candidate_sha256'), 'new': SHA_BRIDGE_A35C}
    bridge['candidate_sha256'] = SHA_BRIDGE_A35C
    sink = bf.setdefault('copyinsink', {
        'base_path': str(SINK_BASE),
        'base_sha256': SHA_SINK_BASE_2018,
        'candidate_path': str(SINK_CAND),
        'candidate_sha256': SHA_SINK_0B11,
        'ownership': ("lineage-root header consumed by the bridge TU at packaging; wired by "
                      "l26_packaging_driver.py as a lineage_overlays destination 'CopyinSink.hpp' "
                      "(EQ pattern) that shadows the frozen copytree file"),
    })
    sink['base_path'] = str(SINK_BASE); sink['base_sha256'] = SHA_SINK_BASE_2018
    sink['candidate_path'] = str(SINK_CAND); sink['candidate_sha256'] = SHA_SINK_0B11
    if 'copyinsink' not in json.loads((L26 / 'copyin-probe/MANIFEST.json').read_text()).get('base_files', {}):
        changed['manifest.copyinsink_row'] = {'added': True}
    if 'l27_fledger_note' not in man:
        man['l27_fledger_note'] = {
            'date': '2026-10-10', 'by': 'agent FX (parked P1 patch set; row repaired by IG p1_ac_row_updates.py)',
            'composition_chain_step': '94f81a17 (EE gated) + FX fault-ledger desaturation = a35c7f91',
            'note': 'full note text in parked/P1-fault-ledger/NOTES.md and the patch header',
            'console_contact': False,
        }
        changed['manifest.l27_fledger_note'] = {'added': True}
    MANIFEST.write_text(json.dumps(man, indent=2) + '\n')

    # ---- 3. harvest/pipeline-pins.json (the launch preflight pin source) ---
    pins = json.loads(PINS.read_text())
    p = pins.setdefault('pins', {})
    old_bridge = p.get('bridge_gated', {}).get('sha256')
    old_pkg = p.get('pkgdrv_all', {}).get('sha256')
    bg = p.setdefault('bridge_gated', {})
    bg['sha256'] = SHA_BRIDGE_A35C
    bg['label'] = ('P1 fault-ledger desaturation candidate a35c7f91... (was 94f81a17 Z15-gated; '
                   'desaturated FaultLedger + [ST] copyin_fault_ledger_st line; '
                   'round5-staging/P1/APPLY.md)')
    pd = p.setdefault('pkgdrv_all', {})
    pd['sha256'] = drv_sha
    pd['label'] = ('l26_packaging_driver.py post-P1 (FX hunk-4 CopyinSink lineage_overlays wiring; '
                   'prior pin ee3aa230 was already stale since the 2026-10-11 P8 238->239 rebaseline '
                   'to 810efaae); re-baselined by round5-staging/P1/p1_ac_row_updates.py')
    if old_bridge != SHA_BRIDGE_A35C:
        changed['pins.bridge_gated'] = {'old': old_bridge, 'new': SHA_BRIDGE_A35C}
    if old_pkg != drv_sha:
        changed['pins.pkgdrv_all'] = {'old': old_pkg, 'new': drv_sha}
    PINS.write_text(json.dumps(pins, indent=2) + '\n')

    receipt = {
        'status': 'P1_ROW_UPDATES_APPLIED', 'changed': changed,
        'verified': {'bridge_candidate': SHA_BRIDGE_A35C, 'sink_candidate': SHA_SINK_0B11,
                     'sink_base_frozen': SHA_SINK_BASE_2018, 'driver_post_p1': drv_sha,
                     'driver_py_compile': True},
        'rollback_hint': {'pins.bridge_gated': old_bridge, 'pins.pkgdrv_all': old_pkg},
        'console_contact': False, 'by': 'agent IG round5-staging/P1 (AC script)', 'builds': False,
    }
    RECEIPT.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))
    if not changed:
        print('NOTE: nothing to change — rows were already correct (idempotent re-run).')

if __name__ == '__main__':
    main()

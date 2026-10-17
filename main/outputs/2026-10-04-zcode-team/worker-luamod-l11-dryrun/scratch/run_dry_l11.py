#!/usr/bin/env python3
"""worker-luamod-l11-dryrun scratch harness.

Executes the L10LM driver's OWN main() sequence (module imported from this
scratch dir; the driver copy differs from the pinned 9b332fc5 edition by the
single documented MY scope-isolation redirect) through:

  check_sha(COMPOSE6_FIXED) -> odr_gate -> receipt_preflight ->
  edited_landed_gate -> entry_preflight -> bridge pin check ->
  compose_phase (THE REAL COMPOSE, fresh scratch run dir) ->
  chain_symbols_gate

and then STOPS, by explicit mission bound: build_overlay + package_phase invoke
the frozen shared package_prx.py pipeline (e6fbe6c5...) that launch-10 owns for
its serialized builds; the dry run must not contend with it. No --out
prx-candidate dir is created. Everything written stays under
worker-luamod-l11-dryrun/scratch/.
"""
import argparse
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import importlib.util
spec = importlib.util.spec_from_file_location('l10lm_driver', HERE / 'build_runtime_composition6_l10lm.py')
drv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(drv)

MY = drv.MY
assert str(MY).endswith('worker-luamod-l11-dryrun/scratch'), f'MY not scratch-redirected: {MY}'
assert drv.COMPOSE6_FIXED == MY / 'build_runtime_composition6_fixed_l10lm.py'
assert drv.DEFAULT_REPLACE_RECEIPT == MY / 'receipts/composition6-replace-receipt.json'

RUN = MY / ('ue4ss-native-supplier-composition-6-l10lm/run-'
            + (sys.argv[1] if len(sys.argv) > 1 else 'a'))

# --program-source: the rr-covered canonical staged copy (the r3-armed run-a
# precedent; the L10LM driver's MY-relative default does not exist in the
# rebase staging and a scratch-local copy would NOT be rr-covered — coverage
# is by resolved path).
PROGRAM_SOURCE = drv.ROOT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp'
assert PROGRAM_SOURCE.is_file(), PROGRAM_SOURCE

args = argparse.Namespace(
    program_source=PROGRAM_SOURCE,
    adapter_source=drv.ADAPTER_DEFAULT,
    odr_port_header=drv.ODR_HEADER_PORT,
    odr_staged_header=None,
    replace_receipt=drv.DEFAULT_REPLACE_RECEIPT,
    extra_receipt=drv.DEFAULT_EXTRA_RECEIPT,
    entry_source=drv.ENTRY_DEFAULT,
    entry_sha256=drv.ENTRY_MEASURED_SHA,
    bridge_source=drv.BRIDGE_DEFAULT,
    bridge_sha256=drv.BRIDGE_MEASURED_SHA,
    no_bridge_unit=False,
    stage_unit=[],
    composition_run=RUN,
    reuse_composition_run=False,
    overlay_dir=None,
    verify_only=False,
    allow_unedited=False,
    require_symbol=[],
    stage_header=[],
    out=MY / 'prx-candidate-NOT-CREATED-dryrun-stops-before-package',
)

# main() default resolution (mirror):
if args.odr_staged_header is None:
    args.odr_staged_header = args.odr_port_header
# NOTE: main() would default composition_run to RUN6 when None; we pass our own
# scratch run explicitly, matching INTEGRATION.md §3 "pick fresh ones per pass".

t0 = time.time()
def mark(msg):
    print('[dryrun +%.1fs] %s' % (time.time() - t0, msg), flush=True)

mark('driver module loaded; MY=%s' % MY)
drv.check_sha(drv.COMPOSE6_FIXED, drv.COMPOSE6_FIXED_SHA, 'fixed build_runtime_composition6_fixed.py')
logs = MY / 'staging'
logs.mkdir(parents=True, exist_ok=True)

drv.odr_gate(Path(args.odr_port_header), Path(args.odr_staged_header), logs)
mark('odr_gate OK')
drv.receipt_preflight(Path(args.replace_receipt), Path(args.extra_receipt),
                      Path(args.program_source), Path(args.adapter_source),
                      Path(args.odr_port_header), drv.LC_CPP_DEFAULT, logs)
mark('receipt_preflight OK')
drv.edited_landed_gate(args)
mark('edited_landed_gate OK')
drv.entry_preflight(Path(args.entry_source), args.entry_sha256, logs)
mark('entry_preflight OK')

if not args.no_bridge_unit:
    bridge = Path(args.bridge_source)
    if not bridge.is_file():
        raise SystemExit(f'bridge TU not found: {bridge}')
    bridge_got = drv.sha256_file(bridge)
    if bridge_got != args.bridge_sha256:
        raise SystemExit(f'bridge source drift vs pin: {bridge_got} != {args.bridge_sha256}')
    mark('bridge pin OK: %s...' % bridge_got[:16])

if args.verify_only:
    raise SystemExit('verify_only not used by this harness')

run_dir = drv.compose_phase(args, logs)
mark('compose_phase OK: %s' % run_dir)
drv.chain_symbols_gate(run_dir, drv.DEFAULT_REQUIRE_SYMBOLS + args.require_symbol)
mark('chain_symbols_gate OK')

print()
print('=' * 72)
print('L11-DRYRUN STOP BOUND REACHED: compose + chain-symbols gate PASSED.')
print('build_overlay + package_phase NOT run: package_prx.py (e6fbe6c5...) is')
print('the shared serialized packaging pipeline launch-10 owns; per mission')
print('limits the dry run stops here and notes the serialization requirement.')
print('No prx-candidate dir was created: %s' % args.out)
print('=' * 72)

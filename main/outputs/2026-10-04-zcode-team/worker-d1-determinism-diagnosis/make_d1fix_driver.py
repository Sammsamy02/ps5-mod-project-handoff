#!/usr/bin/env python3
"""Create the D1FIX driver copy from the original comp6 driver (Procedure B).

Each edit is tagged D1FIX E# in the output and asserted unique against the
original text before replacement. Run from the workspace root (what/).
"""
from pathlib import Path

src = Path('outputs/2026-10-04-zcode-team/worker-comp6-driver/build_runtime_composition6.py').read_text()

edits = []

# E1: scope isolation — MY points at this worker's dir
edits.append((
    "MY = TEAM / 'worker-comp6-driver'",
    "MY = TEAM / 'worker-d1-determinism-diagnosis'  # D1FIX E1: scope isolation (was worker-comp6-driver)",
))

# E2: receipts resolve to the original driver's attestations
edits.append((
    "RECEIPTS = MY / 'receipts'",
    "RECEIPTS = TEAM / 'worker-comp6-driver/receipts'  # D1FIX E2: attestations live with the original driver",
))

# E3: compose script resolves to the original driver's pinned fixed script
edits.append((
    "COMPOSE6_FIXED = MY / 'build_runtime_composition6_fixed.py'",
    "COMPOSE6_FIXED = TEAM / 'worker-comp6-driver/build_runtime_composition6_fixed.py'  # D1FIX E3 (sha-pinned below)",
))

# E4: THE FIX — reuse re-verifies the composition-6 object
edits.append((
    "        args.composition_run = RUN4 if args.reuse_composition_run else RUN6",
    "        # D1FIX E4: --reuse-composition-run previously fell back to RUN4\n"
    "        # (composition-4/run-3, the STALE pre-comp6 object 45dd9901..., 77,368 B\n"
    "        # larger, missing the program/initializer/Resolution-B replacements).\n"
    "        # That silently built candidate-2 from the wrong lineage (D1 FAIL).\n"
    "        # Reuse now re-verifies the composition-6 composed object itself.\n"
    "        args.composition_run = RUN6",
))

# E5: help text matches E4
edits.append((
    "                    help='composition run dir (default: fresh composition-6 run-1 for '\n"
    "                         'Phase A, composition-4 run-3 with --reuse-composition-run)')",
    "                    help='composition run dir (default: fresh composition-6 run-1 for '\n"
    "                         'Phase A; --reuse-composition-run re-verifies the SAME '\n"
    "                         'composition-6 run-1 object - D1FIX E4/E5)')",
))

# E6: build_overlay takes an explicit overlay dir
edits.append((
    "def build_overlay(entry: Path, stage_headers: list, stage_units: list,\n"
    "                  bridge: Path, odr_staged: Path) -> tuple:",
    "def build_overlay(entry: Path, stage_headers: list, stage_units: list,\n"
    "                  bridge: Path, odr_staged: Path,\n"
    "                  overlay_dir=None) -> tuple:  # D1FIX E6 (unannotated: host python3 < 3.10)",
))
edits.append((
    "    overlay = MY / 'staging/lineage-overlay-2'",
    "    overlay = Path(overlay_dir) if overlay_dir else MY / 'staging/lineage-overlay-2'  # D1FIX E6",
))

# E7: --overlay-dir argument + pass-through in main
edits.append((
    "    ap.add_argument('--reuse-composition-run', action='store_true')",
    "    ap.add_argument('--reuse-composition-run', action='store_true')\n"
    "    ap.add_argument('--overlay-dir', type=Path, default=None,\n"
    "                    help='D1FIX E7: overlay dir for this run (default '\n"
    "                         'MY/staging/lineage-overlay-2-d1 semantics: '\n"
    "                         'staging/lineage-overlay-2 under this worker dir)')",
))
edits.append((
    "                                       args.stage_unit, bridge,\n"
    "                                       Path(args.odr_staged_header))",
    "                                       args.stage_unit, bridge,\n"
    "                                       Path(args.odr_staged_header),\n"
    "                                       args.overlay_dir)  # D1FIX E7",
))

for old, new in edits:
    n = src.count(old)
    assert n == 1, f'anchor not unique/found (count={n}): {old[:80]!r}'
    src = src.replace(old, new)

header = '''"""D1-FIX COPY (worker-d1-determinism-diagnosis) - composition-6 driver.

Procedure-B copy of worker-comp6-driver/build_runtime_composition6.py with the
minimal D1 determinism fix set (each edit tagged D1FIX E# and asserted unique):

  E1  MY -> this worker dir (scope isolation: logs/overlay/ledger mirror written
      under outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/)
  E2  receipts resolve to the original driver's attestations (content unchanged)
  E3  COMPOSE6_FIXED resolves to the original driver's pinned fixed script
      (sha 8dd92a6f... re-verified on every run, content unchanged)
  E4  THE FIX: --reuse-composition-run re-verifies the composition-6/run-1
      composed object (4dd4f309...) instead of silently falling back to the
      stale composition-4/run-3 object (45dd9901...). The stale fallback is
      what made candidate-2 differ from candidate-1 by ~19 KB (wrong lineage).
  E5  help text updated to match E4
  E6  build_overlay takes an explicit overlay dir (refuse-if-exists guard kept)
  E7  new --overlay-dir argument, passed through by main()

Everything else (compose argv, packaging pins, gates, C++20 injection,
ledger-mirror semantics) is byte-identical to the original driver.
"""
'''
anchor = '#!/usr/bin/env python3\n"""composition-6 one-step rebuild driver (worker-comp6-driver).'
assert src.count(anchor) == 1
src = src.replace(anchor, '#!/usr/bin/env python3\n' + header + '# --- original docstring preserved below ---\n"""composition-6 one-step rebuild driver (worker-comp6-driver).')

out = Path('outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/build_runtime_composition6_d1fix.py')
out.write_text(src)
import py_compile
py_compile.compile(str(out), doraise=True)
print('written + compiles:', out, out.stat().st_size, 'bytes')

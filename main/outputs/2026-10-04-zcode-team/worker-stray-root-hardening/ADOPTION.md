# ADOPTION — find_root_v2 (worker-stray-root-hardening)

2026-10-05 · LOCAL-ONLY · Companion file: `find_root_v2.py` (sha256
`5590f8bdc87593c3500efffe71e1b57a1575bcef5caf3a74d030a9bd26425bc2`), tested in
`host-test/` (9/9 PASS, see `host-test/PASS-receipt.json`).

## 1. What v2 changes vs the frozen `_find_root()`

Frozen (`worker-prx-link-gaps/package_prx.py` lines 74–85, sha
`e6fbe6c5…`, UNTOUCHED): walks up ≤6 ancestors, returns the FIRST dir with a
bare `.superpowers` subdir → shadowable by any stray staging dir (the
2026-10-05 05:42 incident; same class hit worker-crt-integrate on 10-04 17:08
via vendor `build.py` `parents[2]`).

v2:

| Aspect | Frozen | find_root_v2 |
|---|---|---|
| Marker | any dir named `.superpowers` | STRONG: `.superpowers/native-observations/` **AND** ≥1 of: `.superpowers/native-observations/transport-experiments-progress.json` (ledger) · `README-RETURN.md` · `.superpowers/sdd` |
| Walk | 6 nodes | start + up to **8 ancestors** (9 nodes) |
| Ambiguity | silent first match | **FAILS LOUD** (`RootResolutionError`, a `RuntimeError` subclass) listing every candidate + weak decoy seen |
| Escape hatch | none | `--root DIR` override (verified; `--allow-weak-override` explicit) |
| Return contract | `Path`, `RuntimeError` on miss | identical (subclass raise keeps existing handlers working) |

Marker set enumerated from the true root `…/what` on 2026-10-05; the 05:42
stray (only `native-dependency-spike`) matches NONE of the strong rules, and a
hypothetical `native-observations`-only decoy without the ledger still fails
loud (T3). The quarantined dir's name (`.superpowers.QUARANTINED-…`) is inert
under exact-name matching — verified on the live tree (T9).

## 2. Who should switch, and the exact lines

All 12 drivers that load the frozen file were grepped. Every one already has
`import importlib.util` and a hard-pinned `ROOT = Path('…/what')`, so the
import line is uniform. **None of these edits exist yet** — they are the
recommended change for the next bounded revision of each driver (drivers are
frozen receipts' provenance; do not silently edit them — give any edited
driver a NEW versioned filename/pin like the lineage already does).

### Pattern P1 — 9 drivers (`PKG_FIXED` / `PKG_FIXED_SHA` + `check_sha` + spec load)

worker-comp6-driver/build_runtime_composition6.py ·
worker-comp6-drybuild/build_runtime_composition6_dry1.py ·
worker-d1-determinism-diagnosis/build_runtime_composition6_d1fix.py ·
worker-launch11-vtable-prebuild/build_runtime_composition6_l11pre.py ·
worker-launch11-dry-compose/build_runtime_composition6_drycompose.py ·
worker-launch10-prebuild/build_runtime_composition6_l10pre.py ·
worker-launch10-walkguard-prep/build_runtime_composition6_walkguard.py ·
worker-launch10-r3-armed/build_runtime_composition6_l10r3.py ·
worker-composition-5-rebuild/build_runtime_composition5.py

One-line import (put once near the existing `PKG_FIXED = TEAM / …` line):

```python
_v2spec = importlib.util.spec_from_file_location('find_root_v2', TEAM / 'worker-stray-root-hardening/find_root_v2.py'); find_root_v2 = importlib.util.module_from_spec(_v2spec); _v2spec.loader.exec_module(find_root_v2)
```

Guard (immediately after the existing `spec.loader.exec_module(m)`):

```python
_v2root = find_root_v2.find_root(start=PKG_FIXED.parent)
if _v2root != Path(m.ROOT).resolve(): raise SystemExit(f'find_root_v2 GUARD: frozen _find_root={m.ROOT} vs v2={_v2root} — stray-marker shadow suspected; refusing (see worker-stray-root-hardening/ADOPTION.md)')
```

`start=PKG_FIXED.parent` is byte-exact frozen parity: it walks from the same
directory the frozen `BASE` uses, so the guard can only fire on a genuine
shadow, never on a legitimate vendored relocation.

If a driver prefers AUTO-RECOVERY over refusing (mirror of walkguard WGP E4,
keyed on v2's trusted root instead of E4's `ROOT` constant):

```python
if _v2root != Path(m.ROOT).resolve():
    _old = Path(m.ROOT); m.ROOT = _v2root; m.SPIKE = _v2root / '.superpowers/native-dependency-spike'
    m.PIN = {k: (_v2root / str(v).relative_to(_old) if str(v).startswith(str(_old)) else v) for k, v in m.PIN.items()}
```

(Recovery re-point MUST stay paired with the per-path existence checks the
walkguard driver already perfected — a repointed PIN that does not exist must
still refuse.)

### Pattern P2 — 3 older drivers (`FIXED` / `EXPECTED_SHA`)

worker-composition-3/package_composition3.py ·
worker-composition-4-fix/package_composition4.py ·
worker-composition-5-minimal/build_comp5_minimal.py

These define `FIXED` (not `PKG_FIXED`) and have no `TEAM` var — use `ROOT`:

```python
_v2spec = importlib.util.spec_from_file_location('find_root_v2', ROOT / 'outputs/2026-10-04-zcode-team/worker-stray-root-hardening/find_root_v2.py'); find_root_v2 = importlib.util.module_from_spec(_v2spec); _v2spec.loader.exec_module(find_root_v2)
```

Guard after their `spec.loader.exec_module(m)` (line 40 / ~41 / ~74):

```python
_v2root = find_root_v2.find_root(start=FIXED.parent)
if _v2root != Path(m.ROOT).resolve(): raise SystemExit(f'find_root_v2 GUARD: frozen _find_root={m.ROOT} vs v2={_v2root} — refusing')
```

### Pattern P3 — walkguard WGP E4 (keep, don't replace)

`build_runtime_composition6_walkguard.py` (~lines 612–640) re-points
`m.ROOT/m.SPIKE/m.PIN` in memory when `m.ROOT != ROOT`. Post-quarantine it is
a verified no-op (its own logs: runs a2/b "WGP E4 root re-base applied", 17/17
PASS). Adoption: add the P1 import+guard; KEEP E4 armed as the second layer.
v2's loud failure fires before E4 ever needs to.

### Pattern P4 — r3-armed L10R3 E3 (keep, don't replace)

`build_runtime_composition6_l10r3.py` (~lines 575–587) re-anchors via virtual
`m.__file__` under `ROOT/.superpowers/vendor-origin-l10r3/` with a hard-pinned
`ROOT` constant. Remains valid under v2 (the virtual path still walks up to
the true root, which is strong). Add the P1 import+guard; keep E3.

### No change needed

- worker-composition5-gate/gate_check_composition5.py and
  worker-gate-evolution/gate_check_composition5_v2.py — only mention
  package_prx.py in comments; they never load it.
- worker-prx-packaging/package_prx.py and everything under
  outputs/ps5-mod-bridge/**: fixed-depth `parents[2]` from depth-correct
  locations (audit §4a) — unaffected by this bug class.
- **Vendor `windows-candidate/build.py` copies are the CAUSE, not the
  consumer**: their `ROOT=HERE.parents[2]` misroots whenever vendored deeper
  than 2 levels (crt-integrate 10-04, seh64 05:42). Any future vendoring
  should pin ROOT explicitly or call `find_root_v2.find_root(start=<its dir>)`
  instead of `parents[2]`.

## 3. Operator smoke check (no driver edit required)

```sh
python3 outputs/2026-10-04-zcode-team/worker-stray-root-hardening/find_root_v2.py \
  --start outputs/2026-10-04-zcode-team/worker-prx-link-gaps --json
# expect {"ok": true, "root": "…/what", "corroboration": [ledger, README-RETURN.md, sdd]}
```

If a stray ever reappears, this exits 2 and prints the decoy path, the strong
candidate, and the `--root` hint.

## 4. Why the frozen file must stay byte-identical

- 12 drivers attest its bytes: 9 via `check_sha(PKG_FIXED, PKG_FIXED_SHA,
  'fixed package_prx.py')`, 3 via `EXPECTED_SHA` asserts — all pinning
  `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313`.
- Packaging receipts (comp5/6, d1fix, l10pre/l10wg/l10r3, l11pre, drycompose)
  embed that sha in their provenance chains; an in-place edit breaks every pin
  retroactively and with them the reproducibility of launch-10/11/walkguard/r3
  evidence (audit §5b).
- The hardening therefore ships as a NEW versioned file with NEW pins, exactly
  the pattern this tree already uses for every other frozen artifact.
- This worker re-verified the sha before and after all work, including a
  read-only import of the frozen module during T9: unchanged.

## 5. Rollout order (suggestion, controller-owned)

1. Now: nothing to do — quarantine already restored correct resolution; the
   frozen file + all 12 drivers work unmodified again.
2. Next bounded revision of any lineage driver: add the P1/P2 one-liner +
   guard, bump the driver's own versioned filename/pin (never edit in place).
3. Optional: controller adds the LEDGER-ADDENDUM.md entry (proposed text in
   this dir, NOT appended by this worker).

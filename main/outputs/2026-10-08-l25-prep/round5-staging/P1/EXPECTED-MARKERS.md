# P1 — expected new markers (composition + console signature)

Owner: agent IG (ZCode), 2026-10-10/11. Source: FX's parked patch hunks +
`parked/P1-fault-ledger/NOTES.md` §2(c)/§3. All values below verified against
the /tmp staged apply (`/tmp/p1-stage-IG`).

## 1. Mac-side (composition proof, BEFORE any upload)

New strings P1 composes into the PRX (all narrow UTF-8 literals in the bridge
TU / header — grep-able with `harvest/grep_markers.py`):

| marker text | kind | where it lands | expected |
|---|---|---|---|
| `copyin_fault_ledger_st` | diagnostic_event tag (bridge TU) | `.rodata` UTF-8 | present (>=1) |
| `copyin_fault_ledger_st_pass` | diagnostic_event tag (bridge TU) | `.rodata` UTF-8 | present (>=1) |
| `copyin_fault_ledger_reject_invalid` | diagnostic_event tag (bridge TU) | `.rodata` UTF-8 | present (>=1) |
| `copyin_fault_ledger_full` | diagnostic_event tag (bridge TU) | `.rodata` UTF-8 | present (>=1) |
| `saturated_capacity` | symbol/inline fn (new CopyinSink.hpp) | inline code | present |
| `self_test_mask` | symbol (new CopyinSink.hpp) | inline code | present |

One-command check after packaging (BUILD-CARD step 6):

```bash
python3 outputs/2026-10-08-l25-prep/harvest/grep_markers.py \
  outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx \
  --json --marker copyin_fault_ledger_st --marker copyin_fault_ledger_st_pass \
  --marker copyin_fault_ledger_reject_invalid --marker copyin_fault_ledger_full
```

`copyin_fault_ledger_st` count 0 in BOTH encodings = the desaturated header did
NOT reach the PRX (packaging staged the frozen 2018c9bd sink) — stop, check the
BUILD-INPUTS `lineage_overlays` row + `fault_ledger_overlay` receipt section
(FX NOTES §3.1). This is exactly the failure mode hunk 4's hard error exists to
prevent, so count 0 also means the overlay wiring was bypassed.

markers.json registry (harvest/markers.json, 54 entries today): adding
`copyin_fault_ledger_st` as `critical:true` is RECOMMENDED at AC (it is the
single cheapest composition proof of P1). If added, the count becomes 55 —
the "markers count changed" bookkeeping the AC receipt must then note. The
grep above works without a registry change (`--marker` is ad-hoc).

## 2. Console-side (the [ST] validation signature, from the planned session)

In `supplier-diag.jsonl` (bridge diagnostic events), ONE-SHOT, EARLY — emitted
from `bridge_copyin_directory_prepare()` BEFORE any admission/probe activity:

```
copyin_fault_ledger_st    = 15        (0xF: all four self-test mask bits)
copyin_fault_ledger_st_pass = 1
```

Anything else (missing, st != 15, st_pass != 1) = the desaturated header did
not reach the PRX — stop and reconcile (FX NOTES §3.1).

Never-normal events (each ONE-SHOT, only if the class actually occurs):
- `copyin_fault_ledger_reject_invalid = <rejected_invalid count>` — a caller
  passed an invalid range to record() (new, named, countable; today no caller
  can — defense-in-depth).
- `copyin_fault_ledger_full = <count 64>` — ledger capacity saturation (named);
  queries no longer auto-poison.

Distinguishing evidence for any future `[Seeds][L27]` refusal (FX NOTES §3.2):
reject_invalid present → invalid-range record attempt; _full present →
capacity; NEITHER → a REAL recorded fault range overlaps the label address
(check `pool_copyin_fault_addr_lo/hi`) or the owner-gate fail-closed path
(recheck EM sequencing).

## 3. Static (build-time) self-tests — fail the BUILD, not the session

- `static_assert(l13_sink::FaultLedger::self_test_mask() == 0xFu)` in the new
  `copyin-probe/candidate/CopyinSink.hpp` — fires in every TU including the
  header if anyone reintroduces saturating semantics (host clang++ -std=c++20
  compile + runtime self-test re-verified PASS in staging).
- The runtime `[ST]` pair above is the same test on-console, on a LOCAL ledger
  (`g_copyin_faults` is never touched by the self-test).

## 4. What must NOT change

- The fail-closed owner-gate line in `bridge_copyin_range_faulted` is
  byte-unchanged (verified byte-identical live vs patched in staging):
  `if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no ledger read`
- Real recorded EFAULT ranges still refuse admission; invalid QUERY ranges
  still fail closed (`overlaps` returns true). No import delta
  (diagnostic_event was already an undefined symbol of the composed object).
- UE4SS.log content: P1 emits nothing there (bridge diagnostic events only,
  supplier-diag.jsonl). No HUD/menu/log-line changes beyond the diag channel.

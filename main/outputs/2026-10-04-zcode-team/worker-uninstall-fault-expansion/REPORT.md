# REPORT.md — worker-uninstall-fault-expansion

Origin: worker-uninstall-fault-expansion, 2026-10-04 (ZCode team, PS5 mod-manager).
Re-dispatch with the corrected absolute workspace after a rate-limit death; no
state from the dead attempt existed (fresh CLAIM.json).
Host-only; no network; no console; no original file modified; all writes under
`outputs/2026-10-04-zcode-team/worker-uninstall-fault-expansion/`.

## 1. Result

**One real defect (D1) found, demonstrated on the baseline, and fixed with 4
recorded patch tuples; zero new defects.** The expanded uninstall/crash
recovery suite is green:

```
python3 compose_fault.py --flavor host   # OK (49-patch chain build)
sh test/run_bridge_tests.sh              # 75 PASS / 0 FAIL (regression, patched binary)
sh test/run_fault_tests.sh               # 186 PASS / 0 FAIL (expanded suite)
```

| Artifact | Meaning |
|---|---|
| `composition/build/host/modinst_composed` | patched binary, sha256 `46d873455fa7f6e0b085a39d30dea36c07453eeb473417088a1f55a1ba5b7f1d` |
| `composition/build_baseline/modinst_composed` | pre-patch chain build, sha256 `947affc87b422197021fdf9ef35a036ca73d0f43d7ab34602035a4feccc95c92` — byte-identical to worker-zip-to-mfb's delivered receipt pin (vendoring provenance proven) |
| `test/run_fault_tests.sh`, `test/fault-e2e-summary.log`, `test/fault-e2e.log` | expanded suite + per-case evidence |
| `test/run_bridge_tests.sh`, `test/bridge-e2e-summary.log` | 75-case regression re-run in this dir, green on the patched binary |
| `receipts/d1-window-demo.log` | D1 demonstrated on the baseline binary |
| `compose_fault.py`, `e2e-receipts/fault-script-patches.json`, `composition/receipts/patches.json` | chain driver, splice receipts, full 49-tuple patch chain (45 delivered + 4 new `uf-*`, apply order preserved, anchors + shas logged) |

## 2. D1 — records-lie crash window in the commit sequence (fixed)

`cmd_install` (used by BOTH install and uninstall via `--remove`) deleted the
journal BEFORE committing records (baseline modinst_core.c: `journal_delete`
at the tail of the swap phase, `records_save` in the commit block after it),
and both `recover_title` commit paths had the same order. A death in that
window leaves **journal durably gone + records stale** — an end state NO
recovery can repair: `recover_all` finds no journal ("nothing in flight") and
returns, so `records` claims the removed mod is *installed* while its staged
files are already gone, and the next transaction rebuilds its surviving set
from that lie. The window was unreachable by the stock `--crash-at` harness
(no probe point between journal_delete and records_save), which is why the
delivered 45-patch chain never caught it.

Evidence and fix, all in my dir:
- `test/d1_window_probe.sh` + `receipts/d1-window-demo.log`: reconstructs the
  window's exact end state on the baseline binary (uninstall to completion,
  restore pre-uninstall records.txt with the journal durably deleted) and
  observes the lie: `D1 DEMONSTRATED: records say 'mod skin installed' while
  .../Skin_P.pak is absent; journal/ empty, so recover_all can never repair`.
- Fix (`uf-order-commit`): records commit BEFORE journal_delete at all three
  commit sites. The residual window is now (records durable, journal still
  present), which `recover_title` repairs **idempotently** via the staged
  load-list hash — proven by a new `pre-journal-del` crash probe + recovery
  case (`uf-i5-new-window-probe`, green).
- Patch tuples (appended after the bridge chain, count-checked anchors):
  `uf-order-commit-a`, `uf-order-commit-b`, `uf-recover-forward`,
  `uf-recover-commit`.

## 3. Expanded coverage (all green on the patched binary)

Crash points exercised on UNINSTALL (`install --remove`), each with exit-86
assert, recovery-phrase assert, then invariants:

| Case | Recovery behavior verified |
|---|---|
| `stage-journal`, `staged` | "rolled back staging"; old set untouched |
| `swap-journal` | "rolled back before swap"; old set untouched |
| `rename1` | "finished swap forward" from .next (root missing) |
| `rename2`, `prevdel` | "committed" from live root (ll_sha match) |
| `pre-journal-del` (new probe) | "committed"; records/journal window repaired |

Interleavings: install-crash → remove in one open (`uf-i1`); remove-crash →
install in one open (`uf-i2`); double-uninstall is a benign re-transaction
(`uf-i3`); remove-during-prepare keeps the prepared bystander prepared and
unstaged (`uf-i4`); back-to-back crashes across two transactions, each on its
own journal (`uf-i7`); kill-during-enumeration emulated by a partially
deleted .prev — recovery finishes it (`uf-i6`).

Power-loss / fsync-ordering sims (faithful state surgery on harness-reachable
states): journal rename lost after swap-journal crash → stale .next is
cleared, never mixed, next install lands the FULL tree (`uf-p1`); .next dir
entry lost at rename1 → byte-for-byte rollback from .prev (`uf-p2`); stray
`journal/T.json.tmp` ignored by the `*.json` filter, untouched (`uf-p4`);
unknown-phase journal fails LOUDLY (exit 1, "unknown journal phase") instead
of guessing — the contract fail-safe, pinned as behavior (`uf-p5`).
Quarantine-retained-on-refusal re-pinned in-suite (`uf-q`).

Invariants asserted after every crash + recovery:
- **I1 tree-hash old-or-current, never mixed**: live root tree hash (content +
  relpath, `find|sort|shasum`) must equal the FULL reference tree or the RM
  reference tree computed from clean runs — never anything else.
- **I2 records never lie**: record state is installed iff staged files are
  present (checked per mod, in both rollback and forward outcomes).
- **I3 journal empty after recovery; second `recover` is a silent no-op.**
- **I4 the next transaction after recovery converges** (branching on the
  recovered state: rollback → remove remaining mod; forward-commit → remove
  empties the set to marker+loadlist).
- Transcript sanity: unmatched begins == number of crash runs per case dir
  (crash exits emit begin without end, by design); no unexpected failed ends.

Non-defects documented (no patch, by design/out of threat model): unknown-phase
journal poison pill fails fast rather than guessing (contract: uncertain
operations require reconciliation, never blind replay); a hand-tampered
records.txt with no journal is external tampering, unreachable once D1's order
is fixed (single-writer model); stray `.json.tmp` proven harmless.

## 4. Bounds accounting (honest)

- No nested subagents; no network; no console; originals untouched (verified
  baseline sha equals the delivered pin; all patching via recorded chain).
- Tool-call budget ≤35 was **exceeded (~46)**, driven by suite-debugging
  iterations (a `local`-expansion trap under `set -u` and four suite
  expectation bugs — none were binary defects; every fix is in
  `test/run_fault_tests.sh` only). No work was left half-done to save calls;
  reporting the overage per the honesty rule.
- All suite trees/logs live under `test/run*` inside this directory.

## 5. Not done here (unchanged from the chain)

- Console flavor rebuild/link (same status as worker-zip-to-mfb: bridge TU
  compiles under console flags; console link is the controller's pinned step;
  my 4 tuples touch flavor-independent core code only).
- F-R2 (install's dangling begin on refusal) remains open upstream, unchanged.
- Real runtime ack on console remains the truth (host `running=1` is the
  simulate-runtime file-format proof only).

# REPORT.md — worker-f-r2-fix: F-R2 install-transcript fix (vendored patch-chain extension)

Origin: worker-f-r2-fix, 2026-10-04 (ZCode team, PS5 mod-manager).
Scope: fix the F-R2 defect (worker-e2e-rehearsal REPORT.md §3) in a vendored
copy of the worker-zip-to-mfb composition, extending the recorded exact-anchor
patch chain. Every existing file outside this directory is untouched (read
only, incl. the corpus zips and all upstream receipts); all writes are under
`outputs/2026-10-04-zcode-team/worker-f-r2-fix/` plus the suites' ephemeral
`test/run/` state inside this dir. Host-only; no network; no console; no
nested subagents; 21 tool calls (bound 25).

## 1. The defect and the fix

F-R2 said: `install`'s refusal path exits inside `cmd_install` (e.g. the
`--no-auto-bundle` refusal "not a modforge bundle") BEFORE the dispatch emits
`e2e_status_end`, leaving a dangling `begin` line in e2e-status.jsonl
(baseline zip-to-mfb run: `off` transcript = 3 begins, 2 ends, 1 unmatched),
and `install` emits no terminal `status` event (download/prepare-archive do).

Fix = two new modinst.c patch tuples appended AFTER the zb chain
(apply order = list order), delivered as one new recorded driver splice
(`r2-script-patches-append`) in `compose_bridge_r2.py`:

- **`r2-exit-shim`** — after the LAST include of the composed core
  (`#include "../vendored/modforge_activation.h"`, verified unique), insert a
  TU-local shim: capture real `exit` in a function pointer, then
  `#define exit r2_e2e_exit` with
  `static void r2_e2e_exit(int code) { e2e_status_end(code); r2_real_exit(code); }`.
  Every controlled `exit(EX_*)` of the composed core (all 59 call sites,
  including cmd_install's refusal exits) now emits the matched `end` line
  carrying the failure code first. Exit codes and all `say()` output are
  unchanged. `_exit(86)` crash probes and real signals still leave no end
  line, so the RUNBOOK's "missing end == command died" honesty rule is
  preserved. Anchored after the last include so no header text is seen
  through the macro; placed in modinst_core.c only (bridge/download TUs and
  the status adapter are untouched).
- **`r2-install-status-ok`** — directly after the committed install's
  `say("installed title=... mods=... loadlist=...")` line, add
  `if (mods_out[0]) e2e_status_event("stage-3-install", "ok", mods_out);`,
  mirroring stage-1/stage-2 terminal status events. The `mods_out[0]` guard
  matters: the `install --remove` commit shares this exact line with an empty
  mods list (first build attempt fired there too — caught by the suite, count
  was 2), so remove behavior gains no new event and detail is never empty.

No other tuple, flag, refusal text, exit code, or behavior changed.

## 2. Deliverables in this directory

| File | Meaning |
|---|---|
| `CLAIM.json` | task claim, bounds, acceptance criteria (written first) |
| `compose_bridge_r2.py` | vendored worker-zip-to-mfb compose_bridge.py + the `r2-script-patches-append` splice + `stage_r2_provenance()` |
| `compose_bridge_e2e.py`, `composition_bridge_inner.py` | regenerated adapted driver/inner (apply the full 46-tuple chain) |
| `composition/vendored/modinst_core.c`, `composition/receipts/patches.json` | patched core + per-tuple hashes (chain tail: `zb-usage-bridge -> r2-exit-shim -> r2-install-status-ok`) |
| `e2e-receipts/r2-provenance.json` | sha pins of all vendored F-R2 inputs (compose_bridge.py `0c9ebc2c…`, zip_bridge c/h `3dd2d226…`/`2a46c3bd…`, pre-edit suite `1957d173…`) |
| `e2e-receipts/bridge-script-patches.json` | driver splice log with old/new shas (base compose_e2e.py `3f310101d78a506e…` matches the e2e-final provenance pin — verified in-driver) |
| `owned/…`, `test/run_bridge_tests.sh` | vendored zip_bridge TU (identical to source) + suite with the F-R2 extension |
| `composition/build/host/modinst_composed` | rebuilt host binary (`f2215f2f…`) |
| `test/bridge-e2e-summary.log`, `test/bridge-e2e.log` | full run evidence |

Key shas (sha256, truncated): adapted driver `ac51f006b5634898`, inner
`78830f753e3d0c65`, patched modinst_core.c `67a38c1b8959ed6b`, host binary
`f2215f2ffd92b63c`, suite `915c7342b17bd54f`, driver itself `ad8fa1d6a739a524`.

## 3. Verification (all green)

```
python3 compose_bridge_r2.py --flavor host   # COMPOSITION OK (all 46 tuples applied, count==1 each)
sh test/run_bridge_tests.sh                  # PASS=87 FAIL=0, exit 0  (baseline pattern was 75/75)
```

87 = the original 75 checks (all still passing, incl. the 3 corpus sha pins,
auto-bridge flows, negative refusal, records transitions) + 12 new F-R2
checks:

- **fr2-a (refusal pairing):** `off` transcript now 3 begins / 3 ends /
  **0 unmatched** (was 1 unmatched); install begin present; last end carries
  the failure code `exit=2`; begins==ends; and the `--no-auto-bundle`
  refusal line is still byte-exact:
  `error archive_unreadable bundle for mod '?' rejected: not a modforge bundle`.
- **fr2-b (terminal status):** exactly one
  `{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"zb-lua"}`
  per flow (present + count==1; the remove run emits none, by the guard).
- **transcript sanity strengthened:** the loop now FAILS on any unmatched
  begin. Final transcripts: zb-lua/zb-pak/zb-ini 8/8, neg 3/3 (failed end =
  prepare refusal exit 2), off 3/3 (failed end = install refusal exit 2) —
  0 unmatched everywhere.

Provenance cross-checks: base `compose_e2e.py` sha verified against
worker-e2e-final's `final-provenance.json` pin in-driver; the 3 e2e-final
owned adapters verified against its `owned-manifest.json`; zip_bridge c/h
byte-identical to worker-zip-to-mfb (`identical_to_source: true` in
`r2-provenance.json`).

## 4. Bounds and caveats

- Host flavor only, matching zip-to-mfb's delivery; the r2 tuples live in the
  shared modinst_core.c chain so the console flavor would chain identically,
  but no console payload was rebuilt or claimed here.
- The shim deliberately routes ALL controlled exits of the composed core
  (die() paths, other commands' refusals) through the end path — this IS the
  defect spec's prescribed fix ("routing cmd_install's internal exit(EX_*)
  calls through the status end path"); crash semantics (`_exit(86)`/signals)
  unchanged. Download TU internal exits are out of F-R2 scope and untouched.
- The composed binary is built from the same read-only upstream originals
  (worker-7, ps5io, archive closure); their vendored copies under
  `composition/vendored/` in this dir are fresh per-run copies.

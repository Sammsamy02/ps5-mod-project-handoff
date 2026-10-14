# REPORT — worker-comp6-preflight-audit

2026-10-04 · Independent read-only preflight cross-audit of the composition-6
input set before the real one-pass build. Flag-only; no fixes; no console
contact; no nested subagents. Deliverables order honored: `CLAIM.json` (first)
→ `PREFLIGHT-VERDICT.md` → this file (last).

## 1. Scope and method

Audited the seven declared inputs:
(a) controller scaffold `outputs/2026-10-04-zcode-controller/comp6-integration/`
(8 sources + change-manifest.json); (b) bridge r12 + entry r4
(worker-module-start-wiring); (c) dry-build proof + one-shot checklist
(worker-comp6-drybuild, DRYBUILD-RECEIPT §6 + staging/extra-stage-headers.json
+ vintage-coupling decision); (d) program-unit inventory (176-argv + 1328-row
pins); (e) gate v2 + manifest spec (worker-gate-evolution); (f) comp6-driver
(mid-delivery at audit start; ONE-PASS-CHECKLIST/REPORT/real receipts arrived
during the audit and were audited as found); (g) ODR safety audit
(worker-odr-header-safety; CLAIM-only at start, ODR-SAFETY.md + REPORT.md
arrived mid-audit and were cross-checked).

Method: sha256 re-measurement of every referenced pin; full re-hash of the
1328-row inventory against disk; manifest↔scaffold set comparison; extraction
and count of DEFAULT_STAGE_HEADERS with per-row pin verification; grep of every
composition `.d` (compositions 1–4 run-3, dry1 run-1) for the five changed
header basenames; r11→r12 and r3→r4 diffs; gate-spec↔manifest conformance
check; targeted hunt for stale pins, wrong-count anchors, edited-but-unrecorded
files, and ownership overlaps.

## 2. Headline result

**GO-WITH-FIXES.** Three blocking fixes (entry r3/r4 conflict; v2-format
manifest not yet derivable from the current change-manifest.json; post-build
acceptance table missing the ODR §6.5/6.6 rows), plus checklist-owned
mechanical steps verified pending on disk (P8 overlay move; scaffold-sha
recording; DRYBUILD §6 supersession note; optional r12 syntax-only
pre-flight). Full itemization with exact one-line actions in
`PREFLIGHT-VERDICT.md`.

## 3. What bit us before — hunt results

- Stale pins (r5 lesson): FOUND and already being fixed mid-audit —
  DEFAULT_STAGE_HEADERS carried old Ps/Owned shas (`b4612648`/`69db72d3`)
  after Diff B/C landed in place; the driver worker re-pinned them
  (`f04ce4aa`/`fb8aabec`) at 21:49:32. Post-fix sweep: 28/28 pinned rows match
  disk. The surviving stale-pin RISK is the entry default (F1) — r3 pinned
  where the manifest declares r4, and all gates stay green on the wrong
  vintage, which is exactly the dangerous shape.
- Wrong-count anchors: one found — "31-header" (driver docstring + help text)
  vs actual 30 (three other artifacts say 30). Also reconciled a seeming
  17-vs-20 static-check count discrepancy (packager prints 17 of 20; gate P1
  reads the receipt's "20 checks, failing=none").
- Files edited-but-not-recorded: the driver script, receipts, and three
  reports changed during the audit window; the drybuild APPLY-ORDER Step 1
  still records the pre-edit-7 driver copy ("6 edits", sha `50e8d16c…`) while
  the file is `e0f494e6…`/7 edits. The manifest's `PENDING-FROM-ONE-PASS` sha
  placeholders and "feed5254-pending-final" text are the remaining
  not-yet-recorded items (shas measured; recording is the pending step).
- Ownership overlaps: ONE real conflict (entry r3 vs r4 — driver/checklist vs
  controller manifest; F1). One guidance contradiction (ODR remediation 1
  "land the port program edit" vs checklist P5 "keep port program unmodified"
  — P5 is authoritative; N6). Directory/ledger ownership is otherwise clean:
  drybuild (staging-d1 + `-dry1` run/candidate/ledger-key) and comp6-driver
  (rehearsal-1 + `-rehearsal-1` + `zcode_supplier_composition_6` mirror) never
  collide; real ledger untouched by both.

## 4. Checklist-referenced files: existence + sha verification

All referenced files exist with the referenced shas: pinned composition script
`36cc8199…`; live comp5 driver `aaf47bda…`; dry-build driver copy `e0f494e6…`
(RECEIPT value; APPLY-ORDER row stale per N2); dry object `ef19a4b8…`
(117,781,064 B) + receipt `ea32f3f4…` + gate receipt `7f5906c3…` (8/8 GO);
bridge r11 `ebef44bf…` / r12 `feed5254…` / bridge.h `ecada77e…` (the stale
`c47f7184` default was fixed by drybuild edit 7 and the comp6 driver pins the
current one); entry r3 `51771ab5…` / r4 `c97055f4…`; 21-header
extra-stage-headers.json shas all match both the dry1 staging sources and the
comp6 driver's absorbed defaults; port-of-record header `66499615…` (dry1) →
landed `0a068bb9…` (real), matching the scaffold. The scaffold's 8 sources
match the manifest's declarations exactly, contain zero rehearsal markers
(marker-drop decision pre-absorbed), and `staging/comp6-program/UE4SSProgram.cpp`
is byte-identical to the scaffold copy (`75abf2ab…`).

## 5. The five-hop / vintage / identity / entry questions

- 5-hop lesson fully absorbed: no quoted-include path to any changed header
  lacks a staged intercept in the real-build shape. Independently: every
  composition `.d` grep shows only the fresh-15 family contacting the changed
  headers; the ODR worker's full 368-walk audit (all dependency walks, not
  just `.d`) classifies all 24 contacts (10 NativeProgramContext-family, 3
  NativeLoaderContext, 1 lmf, 9 D2) with retained contacts proven benign
  (offset-safe insertion invariant; reference-only collector). The dry1
  intercept hack itself is superseded by the in-place landings.
- Bridge-header vintage decision: RESOLVED by construction. r12 references the
  Resolution-B fields, so the "keep frozen vintages" branch of the drybuild
  §6 step-4 conditional is impossible; the merged vintages landed in place
  (port + W + both satellite trees all == scaffold shas) and flow to the
  bridge TU through the default staging set. r12's include list is identical
  to r11's, so the 30-header staging list still covers its closure. Residual
  risk: r12 has never been packaging-compiled (rehearsal + dry-build both
  compiled r11) — cheap syntax-only pre-flight recommended before the pass.
- ODR port-vs-staged identity plan: coherent and rehearsed — sha-equality
  gate on Platform/NativeProgramContext.hpp, negative refusal exercised,
  `odr-gate.json` evidence, overlay copy staged from the ODR-gated file.
  Caveat noted: `--odr-staged-header` defaults to the port path, so the gate
  is trivially satisfied unless a distinct staged copy is passed (acceptable;
  build_overlay then copies the gated bytes).
- Entry r4 × packaging checks: r3→r4 diff touches only the module_start
  failure-path logging plus a static helper; `force_align_arg_pointer`
  (MODBRIDGE_ENTRY_ATTRS), both exports, atomic guards, and the import set
  (`clock_gettime`/`getpid` already imported in r3's write_status_failed) are
  unchanged — entry pre-flight checks, packager entry-alignment byte
  evidence, and I1=196 all remain valid for r4.

## 6. Inventory verification detail

`dependency-manifest.json`: 1328 rows, categories 934 sdk-outputs + 394
superpowers-port; full re-hash vs disk at audit time: 0 missing, 0 drift
(effective_sha256 preferred, anticipating the in-place landings — the
NativeProgramContext row's effective sha equals the landed scaffold/port
vintage `0a068bb9…`). `program-argv.json` (authoritative, from the run-3
receipt) = 176 elements; control.commands.json re-run = 176 elements; the
only diffs are the `-o`/`-MF` output paths (expected for a control recompile).
The affected-set assertion in the fixed edition matches the six affected
units.

## 7. Honest limits

- The tree was live during the audit (driver sha `57418b7f…`→`d760b5b7…` at
  21:49:32; receipts regenerated; three reports + ODR deliverables arrived
  mid-audit). Findings reflect the state at measurement time; shas of moving
  artifacts must be re-measured at execution.
- Static reasoning only: no compile, no composition, no gate execution was
  run by this audit (read-only mandate). The r12-packaging-compile gap is
  reasoned, not tested.
- The ODR worker's mid-audit arrival means my independent `.d` grep and its
  368-walk audit were cross-validated against each other rather than one
  being the sole evidence.
- No in-game, completion, or quality claim of any kind follows from this
  audit.

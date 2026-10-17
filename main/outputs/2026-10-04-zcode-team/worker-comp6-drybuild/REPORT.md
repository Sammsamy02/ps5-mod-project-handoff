# REPORT — worker-comp6-drybuild (2026-10-04/05)

DRY BUILD rehearsal: the REPLACE-MECHANICS.md §2 procedure executed
end-to-end with the D2 comp6 scaffold, producing a real changed composition,
a real changed PRX candidate, and a full composition-5 gate receipt. All
writes inside `outputs/2026-10-04-zcode-team/worker-comp6-drybuild/`; no
console contact, no nested subagents, no frozen-tree writes; the pinned
composition script (`36cc8199…`) and the live composition-5 driver
(`aaf47bda…`) re-verified unmodified after every run.

## Deliverables (in order)

`CLAIM.json` → `staging-d1/` (+`STAGING-DIFFS.md`) →
`build_runtime_composition6_dry1.py` + `APPLY-ORDER-comp6-dry1.md` →
`ue4ss-native-supplier-composition-6-dry1/run-1|run-2` →
`prx-candidate-composition6-dry1-a|-b` (+ preserved `-a-failed1`) →
`gate-receipt-composition6-dry1.json` → `DRYBUILD-RECEIPT.md` → this file.

## Result (headline numbers)

- Composed object: `ef19a4b89bdac29a6fb1e63282fd56ad8af947cf583b99b8ef08475f34f3a8b4`
  (117,781,064 B) vs run-3 `45dd9901…` (117,779,552 B). 368 units, 353
  retained — same counts as run-3.
- **14 of 15 fresh units BIT-IDENTICAL to run-3; only `lmf-loader-adapter`
  changed** (D2 Diff A + Resolution B content).
- Defined-symbol delta: **+3 / −0** — exactly the three declared rehearsal
  markers, nothing else. Undefined/imports: identical set to run-3
  (198; added-vs-base unchanged: `sceKernelGetModuleInfo`); packaging import
  census 196 = expected.
- Packaging: wrapped PRX `35ba028e…` (30,792,663 B), bare `0dede0ee…`
  (30,774,360 B; +0.01% vs comp4 baseline), packager `STATIC_CHECKS_PASSED`
  (20 checks, `gaps: []`).
- Determinism: run-2 composed object and candidate-b PRX **bit-identical**
  to run-1/candidate-a across fully independent compositions.
- Gate: **all 8 checks PASS (W1 W2 W3 I1 D1 F1 S1 P1), verdict GO (offline
  gate)** against candidate-a with candidate-b as compare-dir.
- D2 Diff B/C proof: `#error` include-resolution guards passed,
  `lmf-loader-adapter.d` resolves all patched headers from `staging-d1/`
  (zero frozen-path references remain), and the two header marker strings
  are IN the composed object. Honest limit: Diff B/C are ctor guards; the
  adapter TU never constructs those callers, so there is no ctor disassembly
  to show — the R3-standard evidence (guard + `.d` + marker + verbatim
  staged-content diff) is what "in the object" means here.

## Pipeline findings the real comp6 build needs (details + one-shot checklist in DRYBUILD-RECEIPT.md §6)

1. The §2 procedure works as documented for lmf-side changes: argv-only
   redirection of `lmf-loader-adapter` to a staging dir is sufficient for
   Phase A; the staged cpp+hpp override reaches the composed object and
   auto-pins (R3 re-proven at D2 scale, including the multi-hop quoted
   include chain through 5 intercept copies).
2. **Header overrides need the FULL interception chain**: the first attempt
   failed because a 4th direct includer (`OwnedSerializedElfSource.hpp`)
   still pulled the frozen twin → redefinition. Any same-basename header
   override must enumerate ALL direct includers within the consumer TU and
   quote-convert each hop from the staging cpp down. The syntax-only
   pre-flight (exact unit argv + `-fsyntax-only`) catches this cheaply and
   should be a standing step.
3. **r11 bridge vs composition-5 driver defaults — two real gaps**:
   (a) `runtime_start_bridge.h` driver pin is stale (`c47f7184…` vs current
   `ecada77e…`; the real comp5 build staged the current content via an
   explicit `--stage-header` override — proven by its overlay manifest);
   (b) the r11 bridge include closure needs 21 headers flat-staged in the
   packaging overlay (list + shas in `staging/extra-stage-headers.json`),
   including the PORT vintage of `Platform/NativeProgramContext.hpp`
   (`66499615…`, NOT the older `ps-scan-provider-1` copy `5ebad2f1…`).
   Both are absorbed in my driver copy (edit 7) and Step 2.5 args.
4. The scaffold moved under me (21:11: adapter absorbed the configured-roots
   Resolution B opt-in and gained the lmf hpp option field). The dry build
   consumed the 21:11 state (shas pinned in CLAIM/STAGING-DIFFS) and it
   compiles/links clean; configured-roots-only scaffold files
   (`NativeLoaderContext.cpp/.hpp`, `UE4SSProgram.cpp`,
   `Platform-NativeProgramContext.hpp`) remain deferred per task — they
   require Procedure B and are step 3 of the real-build checklist.
5. The rehearsal markers add 3 public symbols to the composed object; the
   real build should drop the marker strings/functions but KEEP the
   `#error` include-resolution guards (zero symbols) + `.d` evidence
   (checklist step 1c).

## Honest limits

- This is a pipeline rehearsal: gates prove compile/link/import/packaging
  invariants offline. No claim about on-target behavior of Diff A/B/C, no
  gameplay/visual/audio acceptance, no AAA/parity/completion claim.
- The negative-control W2 property was not re-run here (the checker's
  comp4 control was already proven by worker-composition5-gate).
- D1 evidence is two independent compositions + two packagings from this
  machine today; it does not claim cross-machine determinism.
- `PRX candidate produced and statically verified (no in-game claim)` — the
  packager's own wording, adopted verbatim.

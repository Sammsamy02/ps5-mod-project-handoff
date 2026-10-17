# REPORT — worker-d1-determinism-diagnosis

2026-10-04. Question owned: composition-6 D1 determinism FAIL (candidate-1 vs
candidate-2, ~19 KB delta). Full analysis in `D1-DIAGNOSIS.md`; machine-readable
fix evidence in `d1-fix-receipt.json`. No console contact, no nested subagents.

## TL;DR

**Root cause: not toolchain nondeterminism.** The original comp6 driver's
`--reuse-composition-run` falls back to the STALE composition-4/run-3 composed
object (`45dd9901…`, 117,779,552 B) instead of re-verifying the
composition-6/run-1 object (`4dd4f309…`, 117,702,184 B) that candidate-1 was
built from — `build_runtime_composition6.py` line 627
(`args.composition_run = RUN4 if args.reuse_composition_run else RUN6`), a
composition-5-era leftover. The reuse sha check is self-consistent against the
stale run's own receipt, so it passed while linking the wrong lineage.
Candidate-2 therefore lacks the composition-6 content (program-unit §3.2
replacement, initializer-integration merge, Resolution-B vintages — 13
differing `source_hashes` between the two runs) and only D1 caught it.

**Fix:** Procedure-B driver copy `build_runtime_composition6_d1fix.py`
(sha `27d85da3…`, edits D1FIX E1–E7; E4 = the fix). Original driver file NOT
modified — its owner should land the same one-liner.

**Proof:** three consecutive packaging runs are byte-identical at every artifact
level — shipped candidate-1 (one-pass), candidate-3 (fresh compose), candidate-4
(fixed reuse): bare-original `843a94d9…`, bare-sdk `bce857c5…`, wrapped fself
`76ee6af3…` (30,773,943 B). Zero-byte delta — no `__FILE__`-class exception
needed. Fresh compose also reproduced RUN6's composed object bit-identically
(4dd4f309…), re-proving Phase A determinism independently.

## Gate command that now passes (exit 0, GO, 8/8 PASS incl. D1)

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir  outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-3 \
  --compare-dir outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-4 \
  --json outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/gate-receipt-d1fix-c3-vs-c4.json
# canonical pair (shipped candidate-1 vs reproduction):
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir  outputs/2026-10-04-zcode-team/worker-comp6-driver/prx-candidate-composition6-1 \
  --compare-dir outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-3 \
  --json outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/gate-receipt-d1fix-c1-vs-c3.json
```

Both receipts on disk; D1 line: "identical sha256 across two builds:
843a94d9600dbe2b… vs 843a94d9600dbe2b…".

## Evidence chain (byte-level)

1. Build inventories: same 14-object set; **12/12 overlay objects sha-identical**
   (overlay recreate exonerated); only `combined-supplier.o` differs (+77,512 B).
2. Receipts: only pinned diff = `composition_dir` (comp-6/run-1 vs comp-4/run-3);
   cand-2's own log shows `ld.lld -r` consuming the comp-4/run-3 object.
3. RUN6 vs RUN4: same 368 unit names, 13 source hashes differ (program,
   UnrealInitializer, NativeLoaderContext cpp/hpp, lmf cpp/hpp, Ps/Owned hpp,
   NativeProgramContext hpp, PS5PadConfig, SeedTable/SeedValidation);
   `retained_units_replaced ["program"]` only in RUN6; 55 extern symbols only in
   RUN4, 1 only in RUN6.
4. Section attribution (cand-2 − cand-1): `.text` +11,936; `.eh_frame` +9,104;
   `.eh_frame_hdr` +2,464; `.gcc_except_table` +396; `.rodata` −4,400;
   `.data.rel.ro` −224; `.rela.dyn` −192 → file +16,192, fself-wrapped +19,216.
   `.dynsym` identical (0x12a8) — same exports, different code.
5. `__FILE__`/abs-vs-rel path styles: non-load-bearing here (identical objects
   compiled with different `-o` styles; zero-byte final delta across differing
   run/overlay/out dir paths).

## Deliverables (this dir)

- `CLAIM.json` (first) · `D1-DIAGNOSIS.md` · `d1-fix-receipt.json` · this file (last)
- `make_d1fix_driver.py` + `build_runtime_composition6_d1fix.py` — documented fix copy
- `prx-candidate-composition6-3/` (fresh compose) and `prx-candidate-composition6-4/`
  (fixed reuse) with full build-receipt.json each
- `run-c3.console.log`, `run-c4.console.log` — raw build logs (incl. the
  `[gate] reused composed object OK: 4dd4f309…` line)
- `gate-receipt-d1fix-c3-vs-c4.json`, `gate-receipt-d1fix-c1-vs-c3.json` — GO receipts
- `ue4ss-native-supplier-composition-6/run-1/` — the fresh compose run (object
  bit-identical to RUN6), `staging/lineage-overlay-2-d1-c3|c4/` — per-run overlays
- `transport-ledger.composition6-mirror.json` — mirror only (real ledger read-only)

## Scope + honesty

Writes confined to this worker dir (+ its script-owned run/candidate dirs);
frozen spike trees read-only; original comp6 driver untouched (upstream one-line
E4 change recommended for its owner). No in-game/runtime claim is made anywhere;
these are packaging-level facts. Gate D1 PASS is a byte-identity statement about
two packaging runs, not a gameplay-quality statement.

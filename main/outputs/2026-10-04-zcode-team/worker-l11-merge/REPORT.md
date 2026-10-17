# REPORT — worker-l11-merge

Worker: worker-l11-merge · 2026-10-05 · gate M-0 / step P-1 of LAUNCH11-PLAN.
LOCAL ONLY throughout: this worker dir + owned staging; no builds composed, no
packaging, no console, nothing in-game. Mission: merge delta A
(enqueue-ladder, `3b19068a…`) and delta B (loadmap-defer r2, `d2ad2818…`) onto
base `04c9d916…` into ONE staged source and prove it.

## Verdict: MERGE PROVEN at host level. M-0 comparator NEW = `813b249d0ba3c16a…`

| requirement (mission) | result |
|---|---|
| Apply A then B AND B then A | both orders rc=0, no fuzz/offset/.rej; results BYTE-IDENTICAL (`dc68760253ec141f…`). Order-independence proven with ZERO conflict resolution — the plan's "plausible" is now proven |
| 3-way derivation record with every input sha | MERGE.md §1/§3; all six input shas verified against plan pins before use; round-trip (`patch -R` B then A) reproduces base byte-identically |
| Frozen 172-token compile gate; control byte-match `854a476b…` | control object = `854a476bdd4f998581f206af9faaa10b64eba8814fc590350c9565dce47f9565` EXACT (path-independence re-proven); argv drift limited to tokens 165/168/171, count 172 preserved |
| NEW merged object sha | `813b249d0ba3c16a641db53c3c4dc04b52442cccc553b88369d0ca50eb96f4a9` (7,039,032 B, +4,528 vs control) — this is the M-0 comparator the controller pins for compose (plan §2 step 3: `luamod.o == <merged comparator>`) |
| Warning-kind parity | 23 warnings / 13 kinds BOTH sides; kind receipts diff EMPTY; 0 errors both |
| EL host suite vs merged (23/23 expected) | **23/23 PASS** (exit 0) + control world 14/14 (bug still reproduced); all 12 extracted regions byte-identical to suite originals; stdout byte-identical to EL's recorded run |
| LG loadmap 5-phase suite vs merged (5/5 expected) | **5/5 PASS** (exit 0, ASan/UBSan 0 errors); stdout byte-identical to LG's recorded run |
| Symbol/UND set | 568 UND both sides, added ∅ / removed ∅; three declared bridge UNDs present in BOTH (lineage, not new); delta B's identical-UND claim holds through the merge; delta A's new calls all bind to pre-existing symbols (empty added set IS the proof); consumer defined-symbol shape intact; both deltas' runtime log strings byte-present in the merged object only |

## Deliverables (this directory)

- `CLAIM.json` — machine-readable claims + honest bounds + disclosure
- `MERGE.md` — full derivation record: inputs, disjointness, both orders,
  order-independence, round-trip, compile chain, UND proof, host re-runs
- `staging/luamod-l11-merged/LuaMod.cpp` — `dc68760253ec141f…` (8327 lines)
- `DIFF.md` + `receipts/merged-vs-base.diff` — 7 hunks vs base, fully attributed
- Compile receipts: `receipts/gate-control.{stderr,stdout,warn-kinds.txt}`,
  `receipts/gate-merged.{stderr,stdout,warn-kinds.txt}`; argv at `work/argv-*.json`
- Host PASS receipts: `receipts/el-host-merged-PASS-receipt.txt`,
  `receipts/lg-host-merged-PASS-receipt.txt` (+ raw stdout/stderr, regions.json)
- UND sets: `receipts/undefined-{control,merged}.txt`
- Cross-diffs: `work/el-vs-merged.diff` (≡ delta B), `work/lg-vs-merged.diff` (≡ delta A)

## What is NOT proven (honest)

1. **No composition.** The composed launch-11 candidate object is NOT measured
   here and must NOT be assumed from any prior value (not `633a8c82…`, not any
   sha in this receipt). The controller's B5 pass measures it; what compose
   should pin for the luamod unit is source `dc687602…` / object
   `813b249d0ba3c16a…` (plan §2 step 3's "NEW comparator").
2. **Chain steps +EL/+LG not re-compiled by me.** `8593884e…` / `3eeba02f…`
   remain EL/LG's objects; my consistency with them is proven at TEXT level
   (staging cross-diffs ≡ the other delta exactly), not re-measured at object
   level. Nothing in my gate needed them (control + merged suffice for M-0).
3. **No gate v2, no determinism double-pass, no chain-symbols 14/14, no
   packaging, no console.** All remain controller-owned steps B5–B7.
4. **Host suites shim the PS5 surface** (declared knobs) — behavioral host
   evidence only; in-game acceptance stays with LAUNCH11-PLAN §4's tier oracle.
5. U-2 (WithDelay family still arm-before-enqueue) is untouched by this merge —
   it was scoped out of delta A by its own mission and I did not expand scope.

## Incident disclosure

My first EL-suite build invocation executed the copied-but-unedited
`build_host_test.sh`, which hard-codes its HT path at the EL worker dir: it
wrote fresh build artifacts into `worker-l11-enqueue-ladder/host-test/build/`.
EL's receipts (PASS-receipt.txt, stdout/stderr) were NOT touched. The
overwritten binaries were rebuilt from byte-identical inputs (identical regions,
sources, flags), so the delivered evidence chain is unaffected; my authoritative
run used `worker-l11-merge/work/el-host-test/build/`. The LG suite was re-pointed
BEFORE its first build — no LG-dir write occurred. I corrected the copied
scripts (one-line HT/W re-point) and note the lesson: these suite scripts embed
absolute output roots; copiers must re-point before first execution.

## Bottom line for the controller

P-1 is delivered: ONE staged source `staging/luamod-l11-merged/LuaMod.cpp`
(`dc68760253ec141f…`) that is provably base+A+B, order-independent,
round-trippable, compiles clean under the frozen argv (control byte-matching
`854a476b…`), with UND set unchanged and both delivered host suites passing
against the merged unit. For compose: pin `shasum $MG/LuaMod.cpp` =
`dc687602…` and expect the pass-3 `luamod.o == 813b249d0ba3c16a…`. The
composed-object sha and all launch-window questions remain unmeasured and
unclaimed.

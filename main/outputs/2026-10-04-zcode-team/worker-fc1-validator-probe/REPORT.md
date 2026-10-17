# REPORT — worker-fc1-validator-probe (2026-10-04)

Status: **DELIVERED** — standalone data-side FName-pool validation probe PRX
(`modbridge_fc1pool.prx`, build `fc1pool1-78508a2e2940`) + STAGE-PLAN for the
comp6 session ridealong + receipts. No console contact, no target execution,
no nested subagents; writes confined to this worker directory. Claim written
first (`CLAIM.json`); this report written last. ~55 tool calls.

## Task 1 — assessment: where is the "built FC-1 validator", and is it standalone?

Findings (all paths under `outputs/2026-10-04-zcode-team/`):

- `worker-fname-fc1-materialize` did NOT build an in-game validator. Its
  artifacts are `FC1-CHECKS.md` (the FC-1 acceptance CONTRACT: FC1-A..FC1-E,
  byte-exact hex, refutation catalogue), `calls_by_anchor.materialized.json`
  (9 call sites materialized offline), and `fc1_offline_scan.py` (a bounded
  read-only HOST scanner mirroring the resolver). Standalone — but host-side
  offline, not an in-game module.
- The built in-game validator is `worker-fname-runtime-probe`:
  `ig_run_fc1_probe(IgSink*)` IS standalone-module-pattern (callable from a
  probe's module_start chain; 8/8 host scenarios; `INTEGRATION.md` describes
  the igf6/combined-igf5 wiring). It is NOT initializer-embedded. But it is
  **exec-side**: FC1-A/B read candidate code bytes via mprotect-first at
  runtime 0x63d590 — exactly the class `worker-fname-ctor-audit`
  (FNAME-VERDICT.md sec 5 sequencing note) declares exec-blocked in-game
  ("code-anchor byte-read from a ploader module blocked"; igf6 interrupted
  before its first exec-span read), and it was never wired into any PRX.
- The validation chain the seed initializer actually needs FIRST — the
  DATA-SIDE pool proof at runtime 0x9765ec0 (CurrentBlock>=1 @+0x8,
  Blocks[0] heap pointer @+0x10, entry 0 = "None", bootstrap ids at
  +0x14040/+0x14044, ByteProperty cross-check) — existed only as a spec in
  `worker-fname-ctor-audit/FNAME-VERDICT.md` sec 5. No runnable module
  implemented it anywhere in the team tree (grep over all worker dirs).

Conclusion: the initializer-branch of the task applies — extract/adapt the
pool validation chain into a standalone data-side probe. Done as specified.

## Task 2 — the standalone probe (fc1pool1)

`modbridge_fc1pool.prx` (22733 B, fself; inner bare-sdk ELF 69408 B):
one-shot synchronous igf-lineage module_start validator riding the proven
qdrv1/igf6 skeleton:

- **Chain (all plain reads)**: fresh per-invocation sysctl KERN_PROC_VMMAP
  scan gates EVERY dereference (mapscan.c, controller correction #1
  discipline); GUA anchor admission at 0x97ba110 (qdrv1 rcodes 1-7,
  layout-A predicates) derives base = GUA_rt - 0x93ba110; pool = base +
  0x9365ec0; then CurrentBlock@+0x8 >= 1 (<= 8192 sane), Blocks[0]@+0x10
  nonzero+canonical+in a readable span, bootstrap ids @+0x14040/+0x14044
  distinct (id_none expected 0), Blocks[id>>16] pointer chase, entry =
  blockptr + (id&0xFFFF)<<1, header u16 bit0 == 0, ASCII == "None\0" /
  "ByteProperty\0" at entry+2. 11 checks; `byteprop_id_for_find_crosscheck`
  emitted for the initializer's later behavioral FNAME_Find comparison.
  Init guard byte @ pool-0x20 recorded as evidence.
- **Fail-open**: hard refusals terminal at first poll with a `reason_code`;
  only transient stages wait (8 x 1 s grace max, `refused_after_grace` at
  exhaustion); no retry loops, no guesses, module_start returns 0 unless the
  record channel itself cannot open.
- **Writes**: OWN channel `/data/modbridge-fc1pool-facts.jsonl` (v3
  separator-correct writer, one sceKernelWrite per <1 KiB line) + exactly
  ONE `FC1POOL: …` line appended to `/data/plugins/UE4SS.log` per launch
  (path_test precedent class). NO mprotect, NO raw syscall (build gate: 0
  `syscall` instructions in .text), NO code reads, NO game-memory writes.
- **Imports**: 8 verbatim rows of the frozen 191-row contract — the igf 7 +
  `nanosleep` (single flagged addition, grace sleeps; qdrv1's three pthread
  rows dropped — no thread at all). Checker proves exact decoded-dynsym
  equality with an independent SceNid oracle.
- **Identity**: `modbridge_fc1pool` — length-preserving patch of the
  Packager's hardcoded string BEFORE finalize/make_fself (17 bytes + 5 NULs
  in the 22-byte slot), distinct from igf6/qdrv1/observer/composition.

## Task 3 — safety conformance

Reads only from map-gated readable spans containing the anchor/pool; the
pool window [0x9765ec0, 0x9779f08) sits in the eboot's observed RW span
[0x953c000, 0x9a34000); no writes anywhere (the only memory touched by the
CPU besides its own stack is the byte-dereference destinations on stack);
igf7 map-gating inherited byte-for-byte from `mapscan.c`/`vmmap_walk.c`
(byte-identical to the qdrv1 build); fail-open semantics per above.

## Task 4 / deliverables

- `modbridge_fc1pool.prx` + `determinism/{run1,run2}` (byte-identical
  double-build; final PRX, inner ELF, combined object, contract all equal)
- `build_fc1pool_probe.py`, `check_fc1pool_artifact.py`
- `src/` (10 byte-identical lineage files, 2 labeled-change files sha-pinned,
  3 new files recorded), `tests/` (8-scenario host suite)
- `receipts/host-test-receipt.txt`, `VERIFICATION-RECEIPTS.json`
- `STAGE-PLAN.md` — 7-entry ploader.ini layout (6 registered, `= 2` appended
  LAST), Step-0 preflight deltas, SHT-strip readback law with exact
  constants (67848 B readback, body sha `7e2dce27…`), interaction audit
  delta (single-writer new file; one bounded UE4SS.log line), expected
  record shapes for PASS, every hard refusal code, and the three
  grace-refusal codes, rollback, honesty boundaries.

## Verification (local only; NO in-game claims)

- Determinism double-build PASS (all four artifacts run1==run2).
- `check_fc1pool_artifact.py` PASS on run1 AND run2
  (LOCAL_PACKAGE_CHECKS_PASSED; init/fini 1/1; SDK 0x11000000; explicit
  DT_INIT/DT_FINI; per-phdr segment equality; 8-row import exact equality;
  module-name patch evidence; forbidden-symbol gate).
- Host tests 8/8 PASS against the REAL driver/sink/walk sources on a really
  mapped synthetic arena: all-pass (11/11 checks, derived
  base/pool arithmetic asserted), uninitialized grace, pool window
  uncovered (terminal at poll 1), none-ASCII mismatch, Blocks[0]=0, map
  untrusted grace, wide-flag refusal, handle-block-OOB — each with strict
  JSONL oracle and exactly one UE4SS.log line per run.
- Frozen lineage PRX hash re-verified at build time (`3b998f09…`).

## Honest limits

- The PRX has never executed on target; the read CLASS and import surface
  are console-proven lineage, this module is not. Staging remains
  proposal-only behind the standing launch ritual.
- The host test exercises base DERIVATION with a synthetic divergent base
  (macOS arm64 cannot map the real 0x97ba110 below 4 GiB); the real-launch
  prediction base=0x400000 / pool=0x9765ec0 / base_matches_observed_bias=1
  is in STAGE-PLAN sec 7 and is a target-only fact.
- `entry 0 = "None"`: the probe records `id_none` and
  `id_none_is_zero_expected` but gates only on the resolved ASCII string —
  if the engine ordered entries differently the byte evidence still lands.
- The behavioral `FNAME_Find("ByteProperty")` cross-check and the FC1-A/B
  code-byte checks remain with the initializer route (exec-side); this
  probe independently proves the pool BEFORE the initializer depends on it —
  the de-risk the seed task asked for.

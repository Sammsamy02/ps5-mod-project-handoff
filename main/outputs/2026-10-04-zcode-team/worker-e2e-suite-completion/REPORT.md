# REPORT.md — worker-e2e-suite-completion (2026-10-04)

Task: finish the host-side items worker-e2e-payload-build honestly listed
NOT-RUN, plus the two controller decisions, without touching frozen trees or
the console. CLAIM.json was written first; REPORT.md last. All writes stayed
inside `outputs/2026-10-04-zcode-team/worker-e2e-suite-completion/`. No
console contact, no nested subagents, no provider calls.

## 1. Verdict

ALL FOUR HOST SUITES PASS ON THE BUILD-R1 CHAIN — **348/348 checks, 0
failures**:

| suite | result |
|---|---|
| Bridge TU (zip-to-mfb lineage) | PASS=75 FAIL=0 |
| F-R2 archive suite | PASS=87 FAIL=0 |
| Fault matrix (merged-chain revision) | PASS=186 FAIL=0 |
| Config-entry host suite (compiled-in entry) | OVERALL: PASS (46/46 OK lines; refusal matrix 13x EX_CONFIG 6 + absent 2) |

The binary under test was re-derived in this dir and is BYTE-EXACT
`4659c24c4a4d829dc13a59813e7b137bf4d0efeaf0bca45b21e064b589eb2aa1` (host
flavor of the build-r1 sources: pristine 51-tuple core `45c04d08…` + config-entry
diff `4bab84ab…` -> `43ea66c1…` byte-exact, D1 host-build.json argv verbatim,
.o byte-exact `51b88171…`). This proves the payload delivery's "host proof
transfer" claim directly instead of trusting it.

No failures occurred, so the no-fixing-by-edit rule never triggered: the
program under test was never edited; the only harness changes are LOG/SUM/RUN
output-path renames in my runner copies (diffed against origins; check logic
untouched) and a symlink `composition/build/host/modinst_composed` -> my
binary. Every suite's PASS lines are line-identical to the pinned 51-tuple
runs (`worker-d1-chain-relink/test/*-summary.log`) and the original
config-entry receipt — the config-entry revision is observably delta-zero to
these suites, and the 35-line transcript sha `31a4764a…` re-proved.

## 2. Deliverables (this dir)

| file | what |
|---|---|
| `CLAIM.json` | bounds + plan (written first) |
| `SUITE-RESULTS.md` | per-suite counts, receipts, cross-checks, honest scope |
| `PROMOTION-PREP.md` | 51->54 patches.json promotion package: exact metadata delta, tuple table, placement (after `r2-install-status-ok`, delivered order parse->allowlist->consume), controller application steps. PREPARED, NOT APPLIED |
| `EXCONFIG6-MEMO.md` | EX_CONFIG-6 decision memo: schema's own wording quoted verbatim, implementation facts, pros/cons, recommendation: ACK + one-line amendment |
| `build-host/` | pristine copy, patch-apply log, patched source, .o, host binary (all shas in SUITE-RESULTS.md sec 0) |
| `test/` | adapted runners + full/summary logs for the three suites (`bridge75-*`, `fr2-87-*`, `fault-186-*`) + scratch run dirs |
| `host-test/` | path-adapted config-entry harness + fresh receipt/log/scratch |
| `promotion/` | `patches.json.new.copy` (`63c28979…`), `g1_block.copy.py` (`9f46b66a…`), independent chain replay (`g1-chain-replay-core.c` == owned, `g1-chain-replay-verify.log`), optional controller helper `apply-promotion.sh` (gated; not executed) |

## 3. Independent promotion verification (beyond the delivery's own gate)

Replayed `G1_BLOCK` verbatim (imported from `g1_block.py`) onto the pristine
51-tuple core: every anchor occurrences==1; sequential states
`45c04d08… -> 330d47f2… -> 91a122a8… -> 43ea66c1…`; endpoint == owned source
BYTE-EXACT; all literal shas/byte-counts in `patches.json.new` match the
literals; tuple shape identical to the existing receipt (anchor-literal sha
semantics). One correction recorded in the verify log: tuple shas are
ANCHOR-LITERAL shas, not whole-file state shas (first cross-check attempt
compared the wrong thing; the file-state proof is the replay itself).

## 4. EX_CONFIG-6 recommendation (details in EXCONFIG6-MEMO.md)

ACK exit 6 for present-but-refused files. The schema (E2E-CMD-SCHEMA.md §1,
§6 row 3, §8(3)) is quoted verbatim in the memo; the implementation preserves
every refusal behavior the schema actually specifies and splits one collapsed
class; exit 6 is the next free slot in the payload's own taxonomy
(EX_OK 0…EX_CANCELLED 5). Cost: one-line amendment to the schema (and
STAGED-E2E-COMMANDS §4 expected-exit notes) BEFORE RUN-CONFIG B, plus
controller gates must learn "6 = refused, inspect + DELE; 2 = absent/stale".
Reverting would lose information and cost another revision for no gain.

## 5. Honest limits / remaining controller work

- Console E2E staging remains PENDING (user unavailable) — nothing was
  attempted; RUN-CONFIG B behavior stays [TARGET] until the controller's
  staged dispatch of `a21771f8…` (681888 B) with the re-pinned
  `CONFIG_ENTRY_SHA`/`CONFIG_ENTRY_BYTES`.
- The promotion was NOT applied: `worker-d1-chain-relink/composition/` is
  untouched (receipt still 51 tuples, vendored core still `45c04d08…`).
  PROMOTION-PREP.md sec 5 has the exact steps; `apply-promotion.sh` is
  gated and optional.
- The recommended composer splice (G1_BLOCK into
  `composition_zb_console_inner.py` EXTRA_PATCHES for tree reproducibility)
  is flagged as a separate controller step, not done here.
- The one-line schema amendment for EX_CONFIG-6 is proposed, not applied
  (schema doc lives in worker-e2e-console-dry's dir).
- Suite scratch state (state dirs, roots, fixtures) under `test/run*` is
  disposable evidence; the logs are the receipts.

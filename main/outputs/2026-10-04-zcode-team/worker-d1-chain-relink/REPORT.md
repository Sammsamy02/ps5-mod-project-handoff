# REPORT.md — worker-d1-chain-relink (2026-10-04)

Task: build the canonical merged 51-tuple chain (D1 fix BEFORE F-R2 fix) from
`worker-risk-e2e/D1-PROPAGATION.md` section 4, rebuild the console payload ELF
(host + console flavors), run the full regression on the merged chain, and
re-pin the staging manifest per F-R3. CLAIM.json was written first; all writes
stayed under `outputs/2026-10-04-zcode-team/worker-d1-chain-relink/`. No
console, no network; every other worker's files read-only.

## 1. What was built

Vendored `worker-zip-bridge-console-link/compose_console_bridge.py`
(sha `ab9de3dd53fe208a…` verified before use) and appended exactly two
recorded splices to its `MY_SPLICES` list, producing
`compose_merged_relink.py` (sha `aedcd8eaf899bd20f200415728c676fb82505844a6b74b2bd2cf92c16c74492e`):

- `d1-uf-block-append` — inserts the worker-uninstall-fault-expansion
  `UF_BLOCK` (4 uf tuples) immediately after `zb-usage-bridge` in the inner
  script's `EXTRA_PATCHES` (D1 FIRST).
- `fr2-block-append` — inserts the worker-f-r2-fix `R2_BLOCK` (2 r2 tuples)
  after `uf-recover-commit` (F-R2 LAST).

The builder (`compose_merged_relink_builder.py`, sha `77d17b60…`) extracted
both block texts VERBATIM via AST string-literal extraction (no retyping),
then exec'd each block in a sandbox to prove tuple ids/order, and verified
every tuple's old/new sha256 against the fix-origin
`composition/receipts/patches.json` pins (uf 4/4, r2 2/2, occurrences==1).
The console driver's byte-exact ZB replay proof still passes (intermediate
sha == delivered `adapted_driver_sha256`). Splice provenance:
`receipts/merged-script-patches.json`.

Tuple order in the generated `composition_zb_console_inner.py`
(sha `de3a4a3dbaaf7200…`), AST-enumerated: **51 tuples** ending
`… zb-usage-bridge, uf-order-commit-a, uf-order-commit-b, uf-recover-forward,
uf-recover-commit, r2-exit-shim, r2-install-status-ok` — exactly the
canonical order.

## 2. Merged-core identity proofs

- Patched core `composition/vendored/modinst_core.c`
  sha `45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d`.
- Cross-proof: applying the 2 r2 tuples (simulated, count==1 each) to the
  uf-chain core `f9110fb4531df28b…` reproduces the merged core BYTE-EXACT.
- All 4 uf OLD anchors remaining in the merged core: **0**.
- `composition/receipts/patches.json`: 51 patches, every occurrence matches
  its expected count; the single non-1 is base-chain `empty-root-open`==2,
  identical to the delivered base chain's receipt (pre-existing, not mine).
- Expected per D1-PROPAGATION §3.3: the merged core does NOT hash to the
  uf-only `f9110fb4…` pin; new shas pinned here (this document + F-R3
  identity receipt).

## 3. Build facts (flavor=all)

| Artifact | Bytes | sha256 |
|---|---|---|
| console payload ELF `composition/build/console/modbridge-installer.payload.elf` (THE DELIVERABLE) | 681528 | `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436` |
| host regression binary `composition/build/host/modinst_composed` | 585296 | `8dffe3cfba5fb211601b3430ca7d990faff4bb53ac6c2117be9f1840a01c8534` |

Console provider check: **116 dynamic imports, missing=[]** — the import NAME
set and per-name provider lists are IDENTICAL to the currently staged payload
`cfa96ab5…` (681632 B). Bridge link intact: 6 `mi_bridge_*` symbols in
`modbridge-installer.payload.map`. Receipts:
`composition/receipts/console-build.json`, `console-import-providers.json`.

## 4. Regression on the merged chain

| Suite | Result | Evidence (this dir) |
|---|---|---|
| Bridge suite (worker-zip-bridge-console-link verbatim copy) | **75/75 PASS** (rc=0) | `test/bridge75-summary.log`, `test/bridge75-e2e.log` |
| F-R2 extended bridge suite (worker-f-r2-fix copy; 75 + 12 F-R2 checks) | **87/87 PASS** (rc=0) | `test/fr2-87-summary.log`, `test/fr2-87-e2e.log` |
| Fault suite (worker-uninstall-fault-expansion, merged-chain revision) | **186/186 PASS** (rc=0) | `test/fault-merged-186-summary.log`, `test/fault-merged-186-e2e.log` |
| D1 repaired-window probe | `uf-i5-new-window-probe`: crash exit 86 at `pre-journal-del`, recover committed generation 2 — PASS | `test/fault-merged-186-e2e.log` |

Fault-suite revision (the only finding of the run): first run scored
**185/186**, failing ONLY `sanity uf-p5-poison-pill unmatched=0 expected=1`.
Mechanism, verified in the actual transcript
(`test/run-fault/uf-p5-poison-pill/state/e2e-status.jsonl`: begin +
`{"event":"end","exit":1}`): the F-R2 `r2-exit-shim` routes the CONTROLLED
`die()` exit of the refused `records` command through `e2e_status_end`, so the
refusal now has a MATCHED failed end instead of a dangling begin — exactly
F-R2's designed behavior. The case's functional assertions both PASS (exit=1,
"unknown journal phase" refused loudly, healthy after poison removed). Proof
this is the merge's F-R2 semantics and not a D1 regression: the pre-merge uf
chain's delivered summary shows the same check PASSING with `unmatched=1
expected=1` (186/186 on a no-r2 chain). Resolution per the iteration rules
(distinct ID, no silent edits): `test/run_fault_tests_merged.sh` — a copy of
the uf suite whose ONLY delta is the `uf-p5-poison-pill` expected-unmatched
pin 1→0 with an inline rationale; diff-verified single-line; re-run = 186/186.
All crash-probe cases (`_exit(86)`) still show unmatched=1 as expected.

## 5. F-R3 staging re-pin

`STAGING-MANIFEST-REPIN.md` (this dir) supersedes staging row #1: new payload
sha `e3489297…`, 681528 bytes, import-set identity proven, corpus pins
unchanged, receipt references recorded. Staging size law computed:
phdr-covered prefix **606256 bytes** (3 PT_LOAD) + ELF magic verified —
readback acceptance uses size law + magic, never full-hash (R-FTP-01); the
sha is re-pinned deliberately by that receipt.

Notes recorded exactly:

1. D1-PROPAGATION §3.4 predicted +~200–400 B of text; the actual payload is
   **104 bytes SMALLER** than the staged pre-D1 payload. Plausible cause:
   `uf-order-commit-a` removes cmd_install's early `journal_delete(title)`
   call site while `uf-order-commit-b` re-adds one after the commit block;
   net generated text shrank. Recorded, not hidden; size identity for staging
   comes from this receipt (F-R3 law), not from the old size.
2. D1-PROPAGATION §4 anticipated "87/87 + 186/186" for the merged chain; the
   186/186 holds only with the documented single-pin revision above (the
   design doc simulated text anchors but not status-transcript semantics).
3. Honesty gates untouched: running=1 stays SIMULATED (R-ACK-01); stage-1
   native download deferred (R-DL-01); G1/G3 open (R-ARGV-01/R-G1-02,
   R-G3-01). No console executed; no file outside this worker dir written;
   no raw model/API/billing anything.

## 6. Deliverables

- Merged chain driver: `compose_merged_relink.py` (+ builder + receipts)
- Inner composition (51 tuples): `composition_zb_console_inner.py`
- Console payload ELF (deliverable): `composition/build/console/modbridge-installer.payload.elf`
- Host suite binary: `composition/build/host/modinst_composed`
- Suite evidence: `test/bridge75-*.log`, `test/fr2-87-*.log`, `test/fault-merged-186-*.log`, `test/run_fault_tests_merged.sh`
- Re-pin: `STAGING-MANIFEST-REPIN.md`; identity: `receipts/f-r3-identity.json`
- Splice provenance: `receipts/merged-script-patches.json`; patch evidence: `composition/receipts/patches.json`

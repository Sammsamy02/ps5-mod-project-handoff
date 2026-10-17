# REPORT.md — worker-dll-integration

Worker: worker-dll-integration (ZCode team, 2026-10-05). Budget well under
the 60-call bound. No console contact, no upload/dispatch, no target/PS5
execution, no nested subagents, no network; writes only under
`outputs/2026-10-04-zcode-team/worker-dll-integration/`. No ledger, vendor,
plan, or tool file was modified; the only newly generated evidence is three
on-disk sha256 re-computations.

Status: **DLL_LINE_CLOSED_COMPLETE — successor step chosen = closure,
EXECUTED host-side. dll7 fallback rejected (condition lapsed); runtime
integration rejected (no plan calls for it, nothing depends on it); line
verdict INDEPENDENT of the menu path and PARKED.**

## 1. Inputs read (read-only)

- `worker-dll-8fam-combined/` — CLAIM.json, REPORT.md (8-family merge +
  host validation + SDK composition), EXPECTED-DELTA-8FAM.md (the frozen
  console oracle source), vendor source (entry_consumer.c, loader_consumer.c,
  loader_helper.c, owned_runtime.c) for the results semantics.
- Ledger `.superpowers/native-observations/transport-experiments-progress.json`:
  `owned_dll_console_test` (history: standalone entry/clock PE-DLL runs),
  `zcode_allfam_dll_console_run` (6-family PASS, 2026-10-04),
  `zcode_return_phase1_dll8` (EIGHT_FAMILY_CONSOLE_RUN_PASSED, 2026-10-05),
  `zcode_return_session_log` (consolidation; supersede policy "originals
  preserved intact"), `zcode_worker_dll_track_closure`,
  `zcode_dll_family_summary`, `zcode_worker_7fam_stage_prep`.
- `worker-dll-track-closure/` — TRACK-COMPLETE.md (the 8 done-criteria and
  the dll8 PASS branch), DLL-TRACK-STATE.md (provenance tables, plans,
  successor queue §6, UE4SS independence §7).
- `worker-dll-7fam-stage-prep/` + `worker-8fam-console-prep/` — the frozen
  fallback and preferred stage plans.
- `worker-console-batch-runner/runs/20261005-063508-dll8/` — the tool-run
  artifacts (preflight PASS, upload PASS, gate halt at its own missing-
  filename check; controller dispatched manually once — matches the ledger
  note).
- Controller `session-artifacts-manifest.json` (allfam8-elf pin) and the
  `C/console/` inventory (receipt/archive state).

## 2. The dll8 result, honestly interpreted

Ledger record (quoted basis): upload identity **79281191 EXACT**, upload NOT
repeated, dispatch exactly ONCE; final record **12 family keys ALL enabled**
(incl. `static_tls` + `seh64` — both first console proofs), worker **4/4**,
consumer_imports **90**, results **[86,93]**, memory.last **163840** —
LEDGER MATCH vs the 8fam EXPECTED-DELTA table. Batch-tool provenance: its
`manage:check` lacked `?filename=`; it halted at its own gate correctly (no
dispatch through the tool), was fixed in place (both dll8 and e2e sites),
and the controller dispatched once after verifying the upload.

- **results [86,93] are the substrate's base export-call pair, not
  per-family verdicts**: `HelperCompute(11)=11*7+9=86`,
  `HelperCompute(12)=12*7+9=93` via GetProcAddress name/ordinal, with
  malloc/free + TLS alloc/set/get/free exercised per call. Family verdicts
  are established by each family exercise returning exactly 1 (else a unique
  negative code would have replaced the pair and set first_failure in that
  family's documented range), by the 12 enabled keys, by consumer_imports
  exactly 90 (the union census — each family's imports actually bound), and
  by worker 4/4 / contract_errors 0 / cleanup retired / memory.last 163840.
- **Per-family verdicts after dll8**: InitOnce, Mutex, FileIO, CRT startup —
  individually console-proven earlier, re-proven combined; Module enum, VM —
  console-proven in combined runs only; **Static TLS and SEH64 — first
  console proof** (dir-9 post-link route and dir-3 EXCEPTION route; no
  failure code recorded).
- **Untested/unproven remainder** (carried verbatim from the line's own
  documents): the SEH64 PS5-trampoline register contracts (host-verified
  only — the named review is NOT discharged by the PASS), H2 RSP parity /
  signal delivery / resume-execution / vectored handlers / C++ EH, no TEB
  emulation or loader lock or lazy TLS blocks (Static TLS), bookkeeping-only
  page protection (VM), `_seh_filter_dll` unexercised, 5 loader-unique
  module names, MSVCP140 ASSESSED-only. No Windows-loader parity and no
  arbitrary-DLL claim exists or is made.

## 3. Decision and execution

- **(a) dll7 fallback — rejected.** It was frozen strictly as the
  attribution aid for a SEH64-specific dll8 failure ("SEH64-fail +
  7fam-fallback = carve-out", TRACK-COMPLETE criterion 4). dll8 PASSED; the
  condition lapsed. A dll7 dispatch now answers nothing and would be
  busywork plus a redundant console window. The candidate stays
  compiled-never-dispatched (re-hashed EXACT this session: `83ee2ae9…`,
  334,656 B).
- **(b) Runtime integration into a composed candidate — rejected for now.**
  No plan on the line (TRACK-COMPLETE, DLL-TRACK-STATE §3/§6/§7, 7fam/8fam
  stage plans, batch runner session order, launch-10/11 matrices) calls for
  loading the owned-DLL candidate inside the composition-5/6 PRX, and
  nothing depends on it: the UE4SS acceptance ladder's oracles reference no
  DLL artifact (independence documented five facts, both directions), the
  acceptance path loads Lua mods not owned PE DLLs, lineages are separate,
  and the launch-10/11 and PC-acquisition work has no DLL-line input. The
  SEH64/UE4SS link is motivation (forward-looking substrate for real DLL
  mods), not acceptance. Integration = new successor work behind the
  trampoline review; parked.
- **(c) Closure — chosen and executed host-side.** Criteria 1–3 were already
  MET; criterion 4's dll8 branch resolves to PASS via the ledger; 5–8 are
  honored at recording time by this package (no claim inflation; provenance
  and append-only discipline untouched). Executed evidence: re-hashes this
  session — 8-family `c3aba79f` = `79281191…d804fb` (336,552 B, byte-identical
  to the console-PASSED upload and the controller manifest pin), 7-family
  `e72f5318` = `83ee2ae9…47cae`, 6-family `133c5e9e` = `96296c1c…e81c`; all
  EXACT vs the frozen receipts. Ledger chain verified read-only.

## 4. Outstanding items (honest, none blocking closure)

1. **Controller bookkeeping**: the plan-named post-PASS archive
   (`allfam8-console-receipt.json` + result copy under `C/console/`) does
   not exist on the host — only a fake-drive selftest copy exists
   (`worker-console-batch-runner/runs/fake-drive/dll8/`). The consolidated
   ledger verdict is the record of record; raw records 37–40 remain
   console-side. This worker cannot and did not fabricate them.
2. **SEH64 PS5-trampoline pre-execution review** — the one named open
   successor item, required before any reliance on SEH64 beyond the
   exercised fixtures (real DLL mods raising exceptions).
3. Host-only family caveats and MSVCP140 (ASSESSED-only) as documented
   successor surface.

## 5. Goal-relevance verdict

**INDEPENDENT of the menu path.** The DLL-compatibility line shares no
artifact, oracle, or gate with the launch-10/11 menu/spawn path in either
direction (DLL-TRACK-STATE §7 facts hold unchanged post-PASS). The line is
**COMPLETE and PARKED**: no further console work proposed. Re-entry point if
real in-runtime DLL mods are ever attempted: the successor queue in
NEXT-STEP.md, starting with the trampoline review as a new bounded worker
with new IDs — never a replay of this line's dispatches.

## Honest limits of this report

- The dll8 console PASS is quoted from the ledger entries and the frozen
  plans; this worker holds no raw console records and ran no console check.
- The results semantics were derived from the vendor source and frozen
  ledgers, not re-measured on target.
- Nothing here upgrades any score or claims parity/completion beyond the
  component evidence named above; gameplay elsewhere in the project remains
  a prototype.

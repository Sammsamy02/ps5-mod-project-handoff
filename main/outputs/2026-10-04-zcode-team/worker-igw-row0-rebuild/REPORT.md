# REPORT — worker-igw-row0-rebuild (2026-10-05)

Task: launch-11 SECONDARY path prep per worker-igw-unlock-trace PROPOSAL.md — rebuild
the exec-write probe with `"confirmed": 1` on ROW 0 ONLY (IGW-1 T2: X→RX at 0x7878000,
prot 4→5), rows 1-5 staying `confirmed: 0`. DONE, with all gates green and the delta
bounded at exactly one semantic byte.

## Verdicts delivered

1. **Trace claims — verified first-hand, all true.** Gate `exec_write_probe.c:166-168`
   (`t->confirmed == 0 → XWP_SKIP_NOT_CONFIRMED`), builder default
   `build_exec_write_probe.py:136`, no `confirmed` key in the staged table
   (sha `9cac04a1…` == launch-9 receipt), build-id seed at :446-457.
2. **Staged table (byte-minimal).** `TRANSITIONS.json` in THIS dir
   (`2b0a3635…`, 1186 B) = original + ONE inserted line (`"confirmed": 1,` row 0
   only); rows 1-5 and schema untouched (JSON-key-asserted). Pre-edit snapshot kept
   as `TRANSITIONS.json.orig`. Original probe-build dir untouched (re-verified).
3. **Builds (both variants, deterministic double-build, all gates green).**
   - ARMED `igd1-e030a09eb4d1`: fself `e1541af6…` (20013 B), inner `cc544d6e…`
     (69464 B), contract `8cc757b3…` — deliverable for the verdict.
   - SKELETON `igd1-5f1a25c33203`: fself `d8d16ed7…` (19933 B), inner `8de0eb54…`.
   - Builder gates: frozen lineage hash, forbidden symbols, ZERO raw syscalls,
     contract divergence, 0F 05 pair, determinism run1==run2 on 4 artifact classes
     × 2 variants. Checker rc=0 both variants (armed: 8 strong imports with the
     single typed sceKernelMprotect; skeleton: 7, mprotect absent). Host suite PASS
     (93+6 checks; mocked mprotect; every JSONL record validated).
   - SHT-strip upload constants computed for the new fself: readback 67832 B, body
     sha `e3f13d19…` (method validated against the documented launch-9 constants).
4. **Binary delta bounded (BINARY-DELTA.md).** Inner ELF 69464 B → 69464 B,
   727 differing bytes in 48 runs, all classified: THE change is exactly 1 byte at
   `0xc0c0` (row 0 `confirmed` u64 LSB 0→1); everything else is build-id string
   churn (`igd1-6ebab07f2111`→`igd1-e030a09eb4d1`), its deterministic string-pool
   re-layout (4 note strings shifted −18 B, zero content changes), pointer/
   relocation updates to the same logical strings, and the 24 B GNU/SIE note
   digests. Wrapper diff fully derived (32 B inner-sha binding + propagated
   segments). Generated `xwp_transitions.c` diff: exactly `0u`→`1u` on row 0.
5. **Safety contract intact, binary-verified.** Rows 1-5 `confirmed` read back as 0
   (rows 1/4/5 fully byte-identical to launch-9; rows 2/3 differ only in relocated
   note_ptr); budget-8 sites identical; zero syscalls; armed semantics unchanged;
   machinery sources diff-clean; walkguard fold NOT touched (fold-0 stands).
6. **Staging proposal (STAGING-PROPOSAL.md).** Pairs the rebuilt PRX with a
   probe-loading ini: Option A (RECOMMENDED) restores the preserved 9-entry
   canonical `fd9c7393…` (440 B) — verified FINAL-INI-OK today against the layout
   owner's current tool; Option B is a spec'd new minimal 3-line ini (162 B,
   `0bfa59a6…` TARGET) that requires the layout owner's new pinned-bytes decision
   first. files-first-config-last ordering, presence guard, `.bak`, one bounded
   restore, byte-exact readbacks — all USER-COORDINATED, never during launch-10's
   window. Expected evidence + verdict table included: T2 rc==0 → `W_WORKS`
   (walkguard ACTIVE fold admissible); T2 rc<0 → `W_FAILS_R_WORKS` (D-1-only
   route); fail-closed 5/6 refusals and ABSENT handled honestly; rows 1-5 gate on
   the T2 verdict per the probe's own transition table.

## Honest limits

- Proposal-only staging: no console contact, no launch, no claims about launch
  outcomes. `native_executed=false`, `in_game_results=false` in both receipts.
- `sceKernelMprotect` remains NOT console-proven; the loud provenance flag is
  carried verbatim in the rebuilt `import-contract.json`.
- The launch11-matrix note "verify_final_ini_layout.py rejects 9-entry bytes" is
  OUTDATED relative to worker-ninth-slot-ini-adoption: the current tool pins the
  9-entry canonical and accepts `fd9c7393…` (rc=0 run recorded in
  STAGING-PROPOSAL.md §1).
- No ini was written anywhere; Option B bytes are a spec, not a staged artifact.

## Deliverables (this dir)

| file | sha256 (16) | what |
|---|---|---|
| `TRANSITIONS.json` | `2b0a363530b431e9` | staged table, row 0 confirmed=1 ONLY |
| `TRANSITIONS.json.orig` | `9cac04a1d250adbc` | pre-edit snapshot (== original dir) |
| `modbridge_exec_write_probe_armed.prx` | `e1541af612f97bb4` | THE rebuild deliverable, 20013 B |
| `modbridge_exec_write_probe.prx` | `d8d16ed7a222bedf` | skeleton rebuild, 19933 B |
| `build-receipt-armed.json` | `c2551118e2f62497` | armed build receipt |
| `build-receipt-skeleton.json` | `6c5a1b5377e4d3ad` | skeleton build receipt |
| `determinism/run-armed-{1,2}/`, `run-skeleton-{1,2}/` | — | double-build trees (run1==run2 verified) |
| `src/`, `tests/`, `build_exec_write_probe.py`, `check_xwp_artifact.py` | — | byte-identical staging copies |
| `CLAIM.json` / `REBUILD.md` / `BINARY-DELTA.md` / `STAGING-PROPOSAL.md` / `REPORT.md` | — | this delivery |

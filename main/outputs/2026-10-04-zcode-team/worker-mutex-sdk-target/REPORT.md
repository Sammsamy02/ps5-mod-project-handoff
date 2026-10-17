# REPORT — worker-mutex-sdk-target (2026-10-04)

Deep-budget assignment: run the FULL SDK composition (not --host-only) of the
mutex candidate, verify the receipt, and prepare the console target runbook.
~26 tool calls used of 60. No nested subagents; no console contact; no network;
all writes confined to
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-mutex-sdk-target/`.

## 1. Headline

**The full SDK composition does not currently build.** The real build.py was
executed once with the exact requested flags; all host controls passed again
(zero drift from the passed host validation, build 6), and the target path
failed at its 34th command with one precise compile error. Root cause was
identified with the real toolchain and a minimal one-line fix was probe-
verified — proposal only, nothing outside my directory was modified. The
console target run is fully prepared and is BLOCKED on that fix.

## 2. Run result (exact)

Command (once): `python3 outputs/ps5-mod-bridge/windows-candidate/build.py
--entry --clock --workers --sync --init-once --mutex --report-path
/data/modbridge-owned-dll-candidate.jsonl --output
.superpowers/native-dependency-spike/windows-mutex-candidate-1`

- Fresh output build_id `78bf885d5f374c86968ed8bf65497d25`; receipt status
  **FAILED**, 34 commands completed, `warnings: {}` (zero warnings).
- **Host evidence green (re-proved in-process):** modes normal+release;
  normal final record: imports 51/10, `mutex {enabled:1}` + `init_once
  {enabled:1}` + `synchronization {enabled:1}`, worker 2/2 with begin
  `[5,1,258,259]`, result `[0x81234567, 0x1122334455667788, 0xe17395ab, 0]`,
  results `[86,93]`, cleanup `{pending:0, retired:1}`, first_failure all
  zeros; worker cleanup-failure controls green (stage 25, native_status 4).
  Tree byte-identical to build 6's 97-file source manifest.
- **Target failure:** `native-owned_runtime` →
  `owned_runtime.c:77:16: error: 'ms_abi' calling convention is not supported
  for this target` (the MFR-3 `combined_close_handle`, guarded
  `MB_OWNED_WORKERS && MB_OWNED_MUTEX`).

## 3. Root cause (verified with the real toolchain, not inferred)

- `prospero-clang` defaults to `x86_64-sie-ps5`, which rejects `ms_abi`
  (minimal probe `msabi-probe.c` reproduces the exact error). build.py
  compiles native_c with nflags (no `--target`), so every native TU gets the
  PS5 target.
- All prior `ms_abi`/`win64cc` uses live in abi_c TUs compiled with bflags
  (`--target=x86_64-unknown-freebsd9`), which accepts `ms_abi`.
- This never fired before: the InitOnce target run (1a6b37b7) compiled
  `combined_close_handle` out (no `--mutex`), and host compiles accept
  `ms_abi` (all host-only builds unaffected).
- **Fix probe-verified:** recompiling the real `owned_runtime.c` against the
  failed run's generated headers (full mutex profile) with exact bflags →
  exit 0, zero stderr, 41560-byte object
  (`owned_runtime.bflags-probe.o`/`.stderr`). `-fshort-wchar` is not
  load-bearing for this TU.
- **Proposal (one line, NOT applied):** compile `owned_runtime.c` with
  `bflags` in the native-object loop (`build.py.proposal.diff`). Layout
  equality of the two targets is already build-enforced (build.py:202-206,
  passed in 1a6b37b7). Removing `ms_abi` is not viable — the function must
  call the fixture's Win64 `CloseHandle` pointer (RCX vs RDI).

## 4. Receipt verification (task 2) — as far as physically possible

Status FAILED; artifact/import_providers/undefined-delta fields were never
reached (they are written after the link). The last verified state is:
34 commands, all exit 0, warnings empty. The post-fix re-run must produce:
`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`, artifact
`owned-dll-candidate.elf` (new sha; >241784 bytes of 1a6b37b7),
`import_providers` fully mapped (1a6b37b7 baseline: 42 undefined imports,
all single-provider), zero warnings. Baseline comparison for the deltas:
`SDK-COMPOSITION-RESULT.md` §2.

## 5. CONSOLE-TARGET-RUNBOOK.md (task 3) — prepared, blocked at step 0

- Staging: upload ONCE as `MUTEX_DLL_TEST.elf` (new alias; catalog-checked),
  `/manage:check`, FTP readback sha/bytes == receipt artifact; single
  `/loadpayload:` dispatch under standing authorization (changed
  implementation basis — mirrors CONSOLE-LOG items 8-9 discipline);
  append-only result collection filtered by build_id.
- "Decryption-oracle" values: no such symbol exists anywhere in the tree; the
  term maps to the known-answer constants in the result record (worker
  `0x81234567` / cookie `0x1122334455667788` / last_error `0xe17395ab`,
  results `[86,93]`, begin `[5,1,258,259]`, cleanup retired, first_failure
  zeros) — all tabulated exactly in the runbook §2.
- Expected deltas vs InitOnce target run: consumer_imports 46→**51** (+5
  KERNEL32 mutex imports), helper_imports 10 unchanged, new
  `mutex:{enabled:1}` key, init_once/synchronization enabled, all other
  fields identical, cleanup retired (the old stage-40 BUSY hang must stay
  gone). Full table in runbook §4.
- Triage table for new failure stages: runbook §5 — stages 9 (census), 24
  (-60..-78 mutex fixture codes, plus -40 reject leakage), 25, 39
  (mb_win_mutex_open), 40 BUSY + phase-4 stall (liveness hazard, no
  re-dispatch), delivery errors; hex→int32 conversion discipline included
  (the build-4 "-100" was a misread of 0xFFFFFFBC = -68).

## 6. Honesty bounds

- No receipt/status beyond the failed 34-command state is claimed; no ELF
  exists from this worker; no console dispatch is authorized or performed.
- Host controls passing here is fresh in-process evidence from THIS run, not
  a re-read of build 6 (though the source manifest is byte-identical).
- The fix is probe-verified at compile level only; full link + receipt
  acceptance remains the controller's patched re-run with a NEW build id.
  This track is NOT blocking the game path (DLL track only).

## 7. Artifacts (this directory)

- `CLAIM.json`, `REPORT.md` (this file)
- `SDK-COMPOSITION-RESULT.md` — run + root cause + fix verification detail
- `build.py.proposal.diff` — one-line proposal (NOT applied)
- `CONSOLE-TARGET-RUNBOOK.md` — staging, oracle values, deltas, triage
- `msabi-probe.c`, `owned_runtime.freebsd9-probe.o/.stderr`,
  `owned_runtime.bflags-probe.o/.stderr` — probe evidence

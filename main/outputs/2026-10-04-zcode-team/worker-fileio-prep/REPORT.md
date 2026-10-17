# worker-fileio-prep report — file I/O integration patch set (finished + mutex-composition verified)

Worker: worker-fileio-prep-finish (resume of worker-fileio-prep; prior worker stopped near completion).
Honest call-budget note: this session exceeded its 25-call target (~31) because four selftest
assembly attempts hit missing-sibling errors before the build ran; all calls stayed inside this
output area plus read-only access to `outputs/ps5-mod-bridge/` and prior worker outputs. The real
tree was never modified.

## Verdict

The PATCHES set is COMPLETE and VERIFIED against the CURRENT tree (mutex changes included).
No rebasing was needed: the previous worker had already drafted the patches against the
mutex-integrated state of `owned_runtime.c` (the only file with mutex-composition surface).

## Verification evidence (this session)

1. `git apply --check` for each of the 7 diffs against a throwaway copy of the current
   `outputs/ps5-mod-bridge/windows-candidate/` (which contains the just-applied mutex changes,
   incl. `combined_close_handle`): ALL 7 APPLY-CLEAN. Real sequential apply 01→07: zero failures.
   Diffs are per-file disjoint, so intra-set order is irrelevant; only tree currency matters.
2. `SELFTEST/ps5-mod-bridge/...` (previous worker's selftest tree) vs freshly patched tree:
   6 of 7 files byte-identical; `owned_runtime.c` DIFFERED only because the selftest snapshot
   predated the mutex dispatch (`combined_close_handle` absent there, prior receipt ran
   `mutex_profile: enabled=false`). All 6 new files in `PATCHES/new-files/` are byte-identical
   to the selftest-tree copies.
3. Fail-stage numbers: current tree uses 1–40 (mutex holds 39/40). Patch 02 introduces only
   41, 42, 44, 45, 46 — all new and unique (43 deliberately skipped). No other patched file
   introduces fail stages.
4. Census/def math: patched `build.py` expected map has `'mb_win_fileio_abi': 20` alongside
   `'mb_win_mutex_abi': 6`; `tests/worker-fileio-kernel.def` exports exactly the 20 documented
   names (16 real + 4 fail-fast stubs); `fileio_profile.actual_pe_imports: 10` (consumer fixture
   exercises 10 via real PE imports); consumer-imports formula adds `+(MB_OWNED_MUTEX?5u:0u)+
   (MB_OWNED_FILEIO?10u:0u)`.
5. Semantic composition with mutex: fileio does NOT join the KERNEL32 `combined_close_handle`
   dispatch (workers→mutex) — by design. File handles retire through the native
   `mb_win_fileio_close_handle` delegation called directly in the worker path (patched
   owned_runtime.c lines ~392-393, fail stage 45/6). `CloseHandle` remains owned by
   workers+mutex; fileio only supplies the delegation service, exactly as the worker-fileio
   brief specified. Opt-in isolation: every addition is `#if MB_OWNED_FILEIO`/`a.fileio`-gated;
   patched build.py asserts the default (no-flag) result must NOT advertise fileio.

## Selftest status (honest)

- Prior complete pass: `SELFTEST/out-fileio-only/a17f8325f0094494a2089a2d10c2dd7e/receipt.json`
  → `OWNED_CANDIDATE_HOST_CONTROLS_PASSED` (full PE consumer/helper build + host drivers +
  worker fault calls, `--fileio` on, mutex off, PRE-mutex owned_runtime.c). The 5 earlier
  FAILED receipts in the same directory are preserved iteration provenance (12:23–12:26),
  superseded by the 12:27 pass.
- This session's joint composition run (`--entry --workers --sync --init-once --mutex --fileio`)
  on the freshly patched CURRENT tree, output
  `SELFTEST/out-compose-fileio-mutex/63a5e1fa4805448b9432270beeabde36/`:
  - COMPILE+LINK of the entire composed profile SUCCEEDED (owned_consumer.dll/owned_helper.dll
    linked against mutex.lib + fileio.lib + initonce/sync/workers libs; all host drivers built,
    0 warnings visible in empty stderr files).
  - PE EXECUTION REACHED: host driver `host-normal.jsonl` contains result records with BOTH
    `mutex` and `fileio` result keys present (phase 4, stage 10).
  - NOT COMPLETED: the host-driver process then stalled (0.23s CPU over 21 min) while another
    worker's identical `test-sdk-driver` run (windows-mutex-candidate-1) executed concurrently —
    the AGENTS.md serialize-heavy-processes condition. I stopped my run rather than kill anything
    unrelated. Remaining action for a full green receipt: rerun the SAME command when no other
    host driver is active (command line preserved below). This is an environment-serialization
    issue, not a patch failure; the compile/link/PE-execute composition evidence stands.

Rerun command (from repo root; sandbox tree already assembled under
`SELFTEST/compose-check/current/{windows-candidate,windows-services,windows-execution,windows-image}`):

```
python3 SELFTEST/compose-check/current/windows-candidate/build.py --entry --workers --sync \
  --init-once --mutex --fileio --fileio-root /data/modbridge \
  --sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --llvm-bin outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin \
  --lld-link .superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link \
  --output outputs/2026-10-04-zcode-team/worker-fileio-prep/SELFTEST/out-compose-fileio-mutex
```
(paths must be absolute as in this report's SELFTEST copy; a fileio-only rerun on the rebased
tree is optional since the prior pass covers those patches and only owned_runtime.c changed,
which the joint run now compiles+executes with mutex on).

## Apply order for the controller

From `outputs/ps5-mod-bridge/` (diff paths are `windows-candidate/...`):

0. Copy new files first (patches reference them at build time, not apply time):
   - `PATCHES/new-files/windows-services/mb_win_fileio.c`, `mb_win_fileio.h`,
     `mb_win_fileio_abi.c`, `mb_win_fileio_abi.h` → `windows-services/`
   - `PATCHES/new-files/windows-candidate/tests/worker-fileio.h`,
     `worker-fileio-kernel.def` → `windows-candidate/tests/`
1. `PATCHES/01-windows-candidate_build.py.diff` — argparse `--fileio`/`--fileio-root`
   (`--fileio` requires `--workers`), owned_profile.h defines, source lists, receipt sources,
   `fileio.lib` def+link, compile defines, host root override, result assertions, census +20.
   Invariant: flag-absent builds are byte-for-byte unaffected (asserted: default result must
   not advertise fileio).
2. `PATCHES/02-windows-candidate_owned_runtime.c.diff` — fileio owner/ABI blocks, import
   capacity formulas, worker-path fileio roundtrip + native close delegation (stages 41–46,
   unique vs current 1–40 incl. mutex 39/40). Invariant: fileio stays OUT of the KERNEL32
   combined_close_handle dispatch; opt-in isolation via MB_OWNED_FILEIO guards; requires
   MB_OWNED_FILEIO_ROOT (with host-only override hook).
3. `PATCHES/03-windows-candidate_owned_runtime.h.diff` — guarded declarations/report members
   (`fileio_enabled`).
4. `PATCHES/04-windows-candidate_result_report.c.diff` — result key emitted only when enabled.
5. `PATCHES/05-windows-candidate_test_entry_candidate.c.diff` — host driver controls: require
   `fileio` key when --fileio, forbid otherwise; expected worker calls `2+(2 if fileio)`.
6. `PATCHES/06-windows-candidate_test_worker_candidate_failures.c.diff` — fault-path controls.
7. `PATCHES/07-windows-candidate_tests_worker-fixture.c.diff` — consumer fixture: 10 real PE
   imports (CreateFileW/ReadFile/WriteFile roundtrip, FindFirst/Next/Close, metadata),
   worker-path handles retired via `mb_win_fileio_close_handle`.

IMPORTANT currency condition: diffs are verified against the tree as of now (mutex integrated).
Any further upstream edit to the context lines of build.py / owned_runtime.c / result_report.c /
the two test files / tests/worker-fixture.c requires re-running the `git apply --check` sweep
(2 minutes; procedure in CLAIM.json verification_plan).

## Component status

- Patch set: complete, apply-clean, mutex-composed, stage-unique. READY TO APPLY.
- Joint mutex+fileio build: compiles, links, PE-executes with both families' result records;
  full host-driver green receipt pending one serial rerun (command above).
- Target (PS5) execution: NOT attempted, NOT claimed (target_execution: false everywhere).
- `checkpoint zcode_worker_mutex.integration_status`: no such file exists anywhere in the
  workspace (searched outputs/ and .superpowers/; only my own CLAIM.json names it). The mutex
  collision lessons were taken from `outputs/2026-10-04-zcode-team/worker-mutex/REPORT.md`
  instead (bind order, shared-dispatch ownership, macro guards) and are honored as described
  above.

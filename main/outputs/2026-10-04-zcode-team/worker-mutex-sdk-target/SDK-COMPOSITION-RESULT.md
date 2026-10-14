# SDK-COMPOSITION-RESULT.md — worker-mutex-sdk-target (2026-10-04)

Full SDK composition run of the mutex candidate. Real build.py executed once;
everything else read-only. Result: **host controls all green again, target
composition FAILED at one precise point**; root cause identified, reproduced
with the real toolchain, and a minimal fix verified by probe compile (proposal
only — nothing outside my output dir was modified).

## 1. Command executed (once)

```
python3 outputs/ps5-mod-bridge/windows-candidate/build.py \
  --entry --clock --workers --sync --init-once --mutex \
  --report-path /data/modbridge-owned-dll-candidate.jsonl \
  --output .superpowers/native-dependency-spike/windows-mutex-candidate-1
```

Fresh output dir (build.py mints a uuid subdir):
`.superpowers/native-dependency-spike/windows-mutex-candidate-1/78bf885d5f374c86968ed8bf65497d25/`
build_id `78bf885d5f374c86968ed8bf65497d25`. Receipt status: **FAILED**
(34 of ~60 commands completed).

## 2. Host controls: ALL GREEN (matches build 6 exactly)

The run re-proved, in-process, everything build 6 (68690d14, the passed
`OWNED_CANDIDATE_HOST_CONTROLS_PASSED`) proved:

- modes `normal` + `release`: phases [1,2,3,(4),5], cleanup `{pending:0, retired:1}`,
  `first_failure` all zeros in normal mode; release preserves the original
  cleanup-failure record.
- normal final record: `counts.consumer_imports 51 / helper_imports 10`;
  `mutex {enabled:1}`, `init_once {enabled:1}`, `synchronization {enabled:1}`;
  worker 2/2, `begin [5,1,258,259]`, `result [2166572391, 1234605616436508552,
  3782448555, 0]` (= 0x81234567 / cookie 0x1122334455667788 / last_error
  0xe17395ab / final wait 0); `results [86,93]`.
- worker cleanup failure controls `failure` + `failure-release` green
  (expected `stage 25, native_status 4`; worker 2/2; result[0]=0x81234567).
- `warnings: {}` — zero warnings across all 34 commands.
- The tree is byte-identical to build 6's receipt source list (97 files, zero
  divergent hashes) — no source drift between host validation and this run.

## 3. The failure (exact)

Command label `native-owned_runtime` (build.py:218 — first ms_abi-bearing TU in
the native target-compile loop):

```
owned_runtime.c:77:16: error: 'ms_abi' calling convention is not supported
for this target
   77 | __attribute__((ms_abi)) static int32_t combined_close_handle(void *handle){
```

## 4. Root cause (verified, not inferred)

- `prospero-clang` defaults to target `x86_64-sie-ps5` (verified via `-v`).
  On that target `ms_abi` is rejected — reproduced minimal probe
  (`msabi-probe.c`) fails with the identical error.
- build.py compiles `native_c` (which includes `owned_runtime.c`) with `nflags`
  — no `--target` — so every native TU gets `x86_64-sie-ps5`.
- `combined_close_handle` (owned_runtime.c:72-81, guarded
  `MB_OWNED_WORKERS && MB_OWNED_MUTEX`) is the controller's MFR-3 combined
  CloseHandle from the mutex integration. It is the FIRST `ms_abi` use in a
  native_c TU; all earlier `ms_abi`/`win64cc` uses live in `abi_c` TUs, which
  build.py compiles with `bflags` (`--target=x86_64-unknown-freebsd9`), and
  that target accepts `ms_abi` (all abi IR steps passed in this run's
  predecessors and in 1a6b37b7).
- Why it never failed before: the InitOnce target run (1a6b37b7) compiled this
  function out (no `--mutex` → guard false), and host compiles (macOS
  x86_64) accept `ms_abi`, so all host-only builds (build 6) never touched it.

## 5. Fix verified by probe (proposal only, nothing applied)

Recompiling the exact same real `owned_runtime.c` against the failed run's
generated `owned_profile.h`/`owned_fixtures.h` (full mutex profile: WORKERS 1,
SYNC 1, INIT_ONCE 1, MUTEX 1) with the abi_c-style target:

```
prospero-clang --target=x86_64-unknown-freebsd9 -fshort-wchar -std=c11 -Wall
-Wextra -Werror -O1 -O2 -fPIC -march=x86-64 -ffreestanding -fno-builtin
-I 78bf885d…/ -c owned_runtime.c -o owned_runtime.bflags-probe.o
```

→ **exit 0, zero stderr, 41560-byte object** (evidence:
`owned_runtime.bflags-probe.o` + `.stderr` beside this file). The identical
object was also produced without `-fshort-wchar`, so that flag is not
load-bearing for this TU.

Minimal proposal (diff: `build.py.proposal.diff`): in the native-object loop
(build.py:216-218), compile `owned_runtime.c` with `bflags` instead of
`nflags`. Safety:

- Layout compatibility of the two targets is already a build-enforced
  invariant: build.py:202-206 requires native-layout (default target) and
  bridge-layout (`x86_64-unknown-freebsd9`) to have identical datalayout and
  layout values — passed in 1a6b37b7.
- The same headers already compile under bflags in every abi_c TU.
- Removing `ms_abi` is NOT viable: the function must call the fixture's
  `CloseHandle` pointer, which is an ms_abi (Win64) function pointer; Win64
  passes the first argument in RCX vs SysV RDI, so the call needs a real
  ms_abi call site.
- Alternative (larger, not recommended): move the thunk into `call_workers.c`
  (an abi_c TU that already contains the other win64cc call) and export it.

Residual: I did not link the full ELF (that requires patching build.py, which
is outside my write bounds). Final acceptance remains the controller's patched
re-run with a NEW build id.

## 6. Evidence files (this directory)

- `owned_runtime.bflags-probe.o` / `owned_runtime.bflags-probe.stderr` —
  verified-fix probe (exact proposed flags).
- `owned_runtime.freebsd9-probe.o` / `.stderr` — first probe (without
  `-fshort-wchar`), same result.
- `msabi-probe.c` — minimal reproducer of the target rejection.
- `build.py.proposal.diff` — one-line proposal diff (NOT applied).

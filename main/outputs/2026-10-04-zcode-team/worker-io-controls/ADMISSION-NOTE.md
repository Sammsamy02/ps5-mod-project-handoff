# ADMISSION NOTE — worker-io-controls (launch10-io staged units)

Date: 2026-10-05. Worker: worker-io-controls (staged unit work only).
Scope honored: ALL writes under `outputs/2026-10-04-zcode-team/worker-io-controls/`
plus `/tmp/io-controls-region.txt` (splice scratch). No shared-lineage edits, no
composition runs, no console work, no game claims.

## Assignment

Complete replacement bridge (baseline `runtime_start_bridge_r14_diag_spanpub.cpp`
sha256 `2f7108f3…`) + initializer/SeedValidation as needed, implementing:

1. Batched IO controls `bridge_io_controls()` — called at module_start right
   after the spanpub block, independent of the seeds path, ALWAYS runs.
2. The copyin probe kept on the RECONCILED transport: Sony-native
   `sceKernelOpen/sceKernelWrite/sceKernelClose` (PROVEN in-game, session 1047:
   0x20000 real FName-pool bytes into `/data/plugins/modbridge-copyin-sink.bin`,
   sink sha `6f849a31…`, FSELF `f46502cb`, bridge `cadeb4ea`) + CLOSE-BEFORE-
   READBACK (the sandbox FS does not publish unflushed writes to a second
   handle — 1047's only failure; 1059/1072's ENOENT-at-open was the post-1047
   131 KiB sink file-state, not a provider refutation).
3. Initializer audit fixes: NoneId==0 false-refusal gate, rescuable-class-only
   probe consumption, capture metadata bound to (addr, full range, generation).

## Delivered shas (sha256)

Staged units (`staging/`):

| file | sha256 (first 16) |
|---|---|
| runtime_start_bridge_r14_diag_spanpub.cpp | `497348b8d247d531` |
| UnrealInitializer.cpp                     | `d245bffb5382cfc6` |
| SeedValidation.hpp                        | `c2e6e66331cf45de` |

Baselines (assignment-start bytes, recovered — see "Baseline recovery"):

| file | sha256 (first 16) |
|---|---|
| runtime_start_bridge_r14_diag_spanpub.cpp | `2f7108f3bc2adde0` |
| UnrealInitializer.cpp                     | `4a2b9400b70e61ec` |
| SeedValidation.hpp                        | `234ee5ddd1fb33fc` |

Diffs (`diffs/`, unified, baseline→staged, named with shas):

| diff | sha256 (first 16) |
|---|---|
| runtime_start_bridge_r14_diag_spanpub.2f7108f3-to-497348b8.diff | `cfd43ddf44e20423` |
| SeedValidation.hpp.234ee5dd-to-c2e6e663.diff                    | `2ce9b3c376162c7d` |
| UnrealInitializer.cpp.4a2b9400-to-d245bffb.diff                 | `2b794b516ec0658a` |

Host test (`host-test/`):

| file | sha256 (first 16) |
|---|---|
| test_io_controls.cpp        | `cd95a2fe6c4a7179` |
| test_rescue_path.cpp        | `37b5a4a6273b8e0e` |
| build_and_run.sh            | `b2aa60e3b6b00b1b` |
| extract_region.sh           | `1d0c944fcfbb9598` |
| io_controls_extract.inc (generated, verbatim bridge region) | `ba4814e26cd5f124` |
| host-test-run.log           | `6b00aa399d666624` |
| rescue-path-run.log         | `85939873b055719d` |

## What the bridge change contains (2f7108f3 → 497348b8)

- Header staging note documenting the 1047/1059/1072 provider reconciliation.
- Includes: `<chrono>`, `<cstdio>`, `<cstring>`.
- The v5.2 copyin region REPLACED by the v5.3 region (same probe name,
  signature, classification semantics, R-4 capture slots):
  - Write path is Sony-native `sceKernelOpen(O_WRONLY|O_TRUNC|O_NOFOLLOW, 0600)`
    → page-chunked `sceKernelWrite` (write-return accounting, page-granular
    chunks) → `sceKernelClose(fd)` BEFORE the `fopen`/`fread` first-page
    read-back. No POSIX open/write/close/read on the copyin path. The main
    sink is NEVER unlinked (truncate-per-probe law); the sink open drops
    O_CREAT (the file pre-exists; Control C proves creation on a sibling).
  - Error-shape law: `errno` zeroed immediately before each IO op and captured
    immediately after (a stale errno is never reported); classification covers
    BOTH Sony conventions — (a) `-1`+errno and (b) direct negative-errno
    return (`rc < -1` → code `-rc`). EFAULT → Refused; anything else / partial
    → Unavailable.
  - New `bridge_io_controls()` (extern "C", called in the scan wrapper right
    after `bridge_spanpub_publish`):
    - A: marker read of the EXISTING sink via stdio BEFORE any truncation.
      `io_ctrl_a` (1 readable / 0 fopen failed / 2 short / 3 empty),
      `io_ctrl_a_rc` (fread return; 0xFFFFFFFF when fopen failed),
      `io_ctrl_a_errno` (fresh), `io_ctrl_a_head` (first 16 bytes as 4 LE u32
      words, in order).
    - B: 32-byte canary ("MBIO" magic, format v1, boot `t_ms` u64-LE nonce,
      nonce-derived filler) written via the Sony route, closed, re-read, and
      byte-compared. `io_ctrl_b` (1 verified / 0 open failed / 3 write short /
      4 read-back mismatch / 5 read-back open failed / 6 read-back short),
      `io_ctrl_b_rc`/`io_ctrl_b_errno` (of the deciding op, fresh per op),
      `io_ctrl_b_readback` (4 u32 words).
    - C: creation test on the independent sibling
      `/data/plugins/modbridge-io-create-test.bin` (O_CREAT), 8-byte write,
      close; `io_ctrl_c` (1 created / 0 open failed) + `io_ctrl_c_rc`/
      `io_ctrl_c_errno`; best-effort POSIX `::unlink` of THAT sibling only
      (may fail in-game; non-fatal). Independent of D.
    - D: ONLY when B verified — the copyin probe against the validated
      window-E candidate (0x400000+0x7dd2a40, 64 bytes, igf7-verified unique
      required anchor — the one game address this TU can name at module_start
      without fabricating a pool constant). `io_ctrl_d` mirrors the probe's
      `pool_copyin_probe_fresh` class; `io_ctrl_d_rc`/`io_ctrl_d_errno`/
      `io_ctrl_d_sink` mirror its telemetry (new `g_last_probe_telemetry`).
      When B fails, D is SKIPPED: no `io_ctrl_d*` tag, no probe call.
- The `pool_copyin_probe_*` tag set and the `bridge_write_copyin_capture`
  accessor are unchanged in shape (compat), except `pool_copyin_probe_errno`
  now carries the effective code under both error shapes (documented).
- HOST-TEST-EXTRACT markers wrap the region (externs through io controls) so
  the host test compiles the exact shipped bytes. No conditional compilation.

## What the initializer change contains (4a2b9400 → d245bffb)

- Extern declarations for `bridge_kernel_write_copyin_probe` and
  `bridge_write_copyin_capture` beside the spanpub accessors.
- Audit fix 1: the break gate is now `shape.ok && words.byte_property_id != 0`.
  `words.none_id != 0` was a FALSE REFUSAL — UE5 FName pool semantics make
  NoneId == 0 the legitimate value ("None" is stored FIRST at entry 0 of
  Blocks[0] → id 0; FNAME-POOL-RE.md R3 + session-1047 live pool head).
  Timeout text now names ByteProperty honestly.
- Audit fix 2: the copyin rescue consumes the ONE probe attempt ONLY for
  rescuable coverage classes (CaptureTruncated / NotCovered) AND only with a
  live pool (ByteProperty id ready). Structural failures (null / misaligned
  Blocks[0]) never spend it — they keep waiting under the budget.
- Audit fix 3: the rescue binding is (addr, full range 0x20000, generation):
  an address/range change invalidates the binding (generation++); the SAME
  binding gates every downstream sink-capture read (`rescue_capture_bytes`
  lambda — refuses on any coverage miss; NEVER a direct block-read fallback).
- R-4: on a copyin-admitted boot the None anchor and entry validation consume
  SINK CAPTURE bytes; the span-membership path keeps direct volatile reads
  unchanged. The success log line states the admission provenance honestly
  (copyin-over-listing-gap vs published-span membership).

## SeedValidation.hpp (234ee5dd → c2e6e663)

Comment-only: the transport-law block now records the reconciled Sony-native
route (supersedes the v5.2 "sceKernelOpen/Close spellings are NOT in the
contract" wording) with the 1047 evidence. Zero functional delta; every pure
function is byte-identical (host tests compile the staged header directly).

## Host-test evidence

`sh build_and_run.sh` (compiler: Apple clang, c++17 -Wall -Wextra -O1; zero
warnings): **100/100 assertions pass, exit 0.**

- Part 1 `test_io_controls` (81 assertions): compiles `io_controls_extract.inc`
  VERBATIM (extracted by `extract_region.sh` from the staged bridge — single
  source of truth) against mocked `sceKernel*`/stdio with a temp-dir
  translation of `/data/plugins/`. Covers: A states (missing/readable/short/
  empty + head words), B all six codes, C both codes + real unlink effect,
  D run-only-after-B-verified (no tag, no candidate write otherwise), D
  refused/unavailable under BOTH error shapes (−1+errno and direct
  negative-errno), partial-transfer accounting, no capture slot filled on a
  non-admitted probe, R-4 capture hit/content/miss, invalid-request class (3),
  fresh errno per operation (stale errno 999 poisoned before runs — never
  reported; success paths report 0), A<B<C<D event order, canary nonce
  (two boots differ; the t_ms field itself differs; replaying a previous
  boot's canary as the read-back is REJECTED with code 4).
- Part 2 `test_rescue_path` (19 assertions): compiles the STAGED
  SeedValidation.hpp — NoneId==0 passing shape + engine invariant still
  enforced, Structural never rescuable, CaptureTruncated/NotCovered+Readable
  admitted, Refused/Unavailable honest refusals, capture decode of the real
  pool head (None id 0, ByteProperty id 3), out-of-capture slice refuses,
  wide-entry-0 terminal mismatch.

## OBSERVED EVENT DURING THE RUN (raw provenance, for controller reconciliation)

At 23:31:22 local — WHILE this worker was running — the files
`outputs/2026-10-04-zcode-controller/launch10-build/final-units/
runtime_start_bridge_r14_diag_spanpub.cpp` and `SeedValidation.hpp` were
overwritten by an actor outside this worker with bytes IDENTICAL to this
worker's then-current staged copies (sha256 `497348b8…` / `c2e6e663…`).
This worker wrote ONLY under `worker-io-controls/` (every tool call in this
session's record) and did not touch `final-units`. The `final-units`
UnrealInitializer.cpp was left at its baseline (`4a2b9400…`, mtime 09:55).
Because of that overwrite, this worker's diffs are generated against the
assignment-start baselines recovered from independent lineage snapshots:

- bridge `2f7108f3…`:
  `outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2-aside-1791261102/lifecycle/runtime_start_bridge.cpp`
- SeedValidation `234ee5dd…`:
  `outputs/2026-10-04-zcode-team/worker-copyin-unit/staging/SeedValidation.hpp`
- initializer `4a2b9400…`: the untouched `final-units` file itself.

No action was taken on `final-units` by this worker; flagging for the
controller to reconcile the writer. The staged units here remain the
deliverable of record.

## Explicitly NOT claimed

No composition, no console, no in-game evidence of any kind from this
staging. The IO controls and probe are boot-time diagnostic/probe code whose
in-game behavior is established only by the NEXT real boot's `io_ctrl_*` /
`pool_copyin_probe_*` log evidence. Not admission-grade for any quality
domain; no >8 score implied anywhere.

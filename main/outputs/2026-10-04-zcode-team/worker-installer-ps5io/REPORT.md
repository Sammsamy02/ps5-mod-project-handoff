# REPORT.md — worker-installer-ps5io

Date: 2026-10-04. Output area (everything I wrote):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-installer-ps5io/`

## What was delivered

1. **ps5_io adapter** (`src/ps5_io.h`, `src/ps5_io.c`): the mfb_io / mfx_io /
   mfa_io callback implementations worker-7's vendored core expects, bound to
   the pinned PS5 payload SDK kernel file API. Console TU binds exactly
   sceKernelOpen/Read/Write/Close/Lseek/Mkdir/Stat/Rename/Unlink/Fsync/
   ClockGettime (signatures mirror the pinned SharpProspero
   Interop/Kernel/KernelFile.cs + KernelClock.cs; same convention as
   native-module-probe/probe.c; SceKernelStat laid out to the pinned C#
   field offsets). Explicit PS5IO_* flag semantics (RDONLY/WRONLY/CREAT/
   TRUNC/APPEND/DIRECTORY; read path adds O_NOFOLLOW|O_CLOEXEC like the
   vendored default), mode via symbolic bits (files 0644, dirs 0755), paths
   bounded at 4096 (= MB_ARCHIVE_MAX_PATH_BYTES), reads/writes chunked at
   16 MiB, uniform negative-errno convention, pread as lseek+read (no pinned
   pread export), mkdir_p, stat (size/dev/ino/is_dir), fsync + fsync_dir,
   monotonic now_ms.
2. **Console compile receipts**: `build_console.sh` compiles both TUs with
   the SDK's prospero-clang (driven by the pinned LLVM 21.1.8 llvm-config),
   then does the windows-candidate/build.py-style provider check — every
   undefined symbol nm-verified as defined by pinned target libs
   (`receipts/console-build.log`): all 11 sceKernel* from the libkernel
   family; compiler-emitted memcpy/strlen/memset from libc_stub_weak /
   libSceLibcInternal; bridge's ps5io_* resolved by the sibling object.
   **No unpinned-signature calls; no unresolved symbol.**
3. **Archive preparation bridge** (`src/ps5_prepare_bridge.[ch]`): the
   installer prepare stage -> pinned intake ABI (vendored `intake.h`)
   call path: quarantine dir opened + device/inode identity via ps5_io ->
   mb_archive_source_v1 -> limits/budget defaults (deadline + cancel
   forwarded) -> mb_archive_scan_v1 -> row selection (default regular-only,
   policy hook) -> mb_archive_materialize_v1 with caller journal/replay
   buffers -> result the installer records as `prepared` (manifest name,
   inventory sha256, target token derived with the vendored mfb_sha256).
   Intake reached only through the `ps5_intake_api` vtable so the real
   libarchive 3.8.7 closure link stays controller-composed. Journal-survives-
   failure rule honored (`journal_retained`).
4. **Host tests** (`test/`): everything POSIX-mappable — fd semantics
   (create/trunc/EOF/reopen), errno fidelity (-ENOENT/-EEXIST/-EINVAL/
   -ENAMETOOLONG/-EBADF/-EIO paths), bounded IO (40 MiB single logical write
   chunked; exact pread incl. past-EOF refusal), mkdir_p, stat, join
   bounds, now_ms monotonicity, and the mfb/mfx/mfa callback contracts
   (interleaved bundle offsets, bad bundle index, os_error propagation).
   Bridge stub tests prove scan-before-materialize sequencing, verbatim
   source passing, regular-row selection, journal-capacity rule (an
   off-by-one I had was caught and fixed), quarantine fd/device handoff,
   token consistency + determinism, deadline/cancel forwarding, and every
   failure rule (scan fail stops before materialize; materialize fail
   retains journal; argument/quarantine/cancel refusals).
   Results: `receipts/host-test.log` — ps5io pass=64 fail=0; bridge
   pass=28 fail=0; both suites also clean under ASan+UBSan (a genuine test
   buffer overrun I wrote was caught by ASan and fixed).
5. **RUNTIME-ACK.md**: the minimal status contract — who writes
   `<mod root>/runtime-status.txt` (only the runtime, via the vendored
   mfa_status_*), when `running` becomes true (last complete state line ==
   `mods-started`), crash/restart/session semantics, and the exact five
   emission points the UE4SS bootstrap must add (controller integration
   point; nothing in the bootstrap tree calls mfa_status_* today).
6. **CONSOLE-E2E-PLAN.md**: staged 0-6 no-developer-intervention acceptance
   (baseline, download, prepare, install/transaction, activate via game
   launch, running-from-ack-only, uninstall), with per-stage commands,
   expected records states and honest failure modes.

## Honest limits

- **Nothing ran on a console.** Console work is compile + symbol-provider
  evidence only (`target_resolution_proven=false` in CLAIM.json): fsync/
  fsync_dir effects, O_DIRECTORY opens on the console FS, SceKernelStat
  offset correctness at runtime, and the lseek+read pread equivalence are
  unproven on target.
- **Host build is the test surface**: PS5IO_HOST compiles the same semantic
  layer against POSIX; console-only translation units (flag mapping, stat
  layout, clock) are receipt-verified but not host-executable.
- The bridge's real archive link (libarchive closure objects, mbedtls sha)
  is the controller's composition; my tests stub the intake API. Buffer
  sizing hints come from intake.h constants, not from a real scan.
- **Not in scope / open**: worker-7's driver-level directory enumeration
  (`rmtree`, opendir/readdir in modinst.c) has no binding here — console
  needs a getdents-class binding or a service-layer port; noted as the one
  filesystem gap between the tested slice and a full console service.
  Download stage remains the owner interface. UE4SS-side status emission is
  specified, not implemented.
- One deviation from my own comment claim ("no libc in the console TU"):
  the compiler emits memcpy/strlen/memset; all three are pinned-lib exports
  (nm-verified) — source comments corrected to state this plainly.
- `sceKernelFStat`, `sceKernelRemove`, `sceKernelGetPid` are NOT exported by
  the pinned libs and are NOT called (stat/unlink/libc-getpid used instead).
- ~50 tool-call budget observed; scope trimmed consciously (no full
  modinst.c port, no loadlist-driven mfa_activate host fixture) rather than
  faked.

## Files

- `src/ps5_io.[ch]`, `src/ps5_prepare_bridge.[ch]` — my code.
- `vendored/` — byte-identical copies (modforge_* from worker-7, intake.h
  from the pinned archive tree; sha256s + origins in `vendored/VENDORED.txt`).
- `test/test_ps5io_host.c`, `test/test_prepare_bridge.c`, `test/run_tests.sh`.
- `build_console.sh`; `receipts/console-build.log`, `receipts/host-test.log`;
  `build/*.ps5.o`, host test binaries.
- `RUNTIME-ACK.md`, `CONSOLE-E2E-PLAN.md`, `CLAIM.json`, this report.

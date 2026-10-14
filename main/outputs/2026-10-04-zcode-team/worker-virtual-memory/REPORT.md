# worker-virtual-memory — REPORT

Worker: worker-virtual-memory (ZCode team, 2026-10-04-zcode-team)
Scope: virtual-memory service family from the retained DLL census, host-tested,
cross-compiled under the pinned prospero-clang SDK. No target/console execution,
no network, no nested subagents. Budget: ~45 tool calls of 80.

## Census evidence (exact imports)

Both retained censuses agree
(`outputs/2026-10-04-zcode-team/worker-dll-mod-retention/{baseline-dll-census.json,ue4ss-zip-census.json}`):

- UE4SS.dll (sha256 94c2ebd3ca9f…) imports from KERNEL32.dll, family "heap":
  `VirtualAlloc`, `VirtualFree`, `VirtualProtect`, `VirtualQuery`, `VirtualQueryEx`.
- dwmapi.dll imports none of the virtual-memory family.
- Adjacent observation: UE4SS.dll also imports `GetSystemInfo` (family
  "uncategorized") — left to its own family; this owner publishes the values a
  GetSystemInfo bridge needs (see wiring point 4).

## Delivered files (all under this worker directory)

- `CLAIM.json` — first write, scope + rules acknowledgment.
- `sources/mb_win_vm.h` — family contract: MEM_*/PAGE_*/win32 error constants,
  `mb_win_vm_basic_info` (Win64 48-byte MEMORY_BASIC_INFORMATION with
  PartitionId field), owner/config/ops types, full behavioral contract comments.
- `sources/mb_win_vm.c` — owner implementation (~370 lines, freestanding C11).
- `sources/tests/test_win_vm.c` — 164 checks, two backends: malloc + call
  recorder (state-machine evidence) and POSIX mmap/mprotect (real host page
  protection); `-DMB_WIN_VM_LAYOUT` emits the cross-target layout global.
- `verify_vm.py` — receipt-producing verifier (host sanitized+plain runs,
  pinned prospero-clang native object, dual-target IR layout equality, SDK
  export coverage).
- `receipts/b242de2e1ddc46c1986d3fd650720775/` — full run evidence.

## Verification evidence (receipt b242de2e1ddc46c1986d3fd650720775)

- status `VM_FAMILY_HOST_AND_NATIVE_OBJECTS_PASSED`; 10 recorded commands.
- Host: `164 checks, 0 failures` under ASan+UBSan (`-arch x86_64`) and plain.
- Pinned toolchain: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang`
  with `LLVM_CONFIG` from `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`.
- `mb_win_vm.o` (6096 bytes, sha256 eaa8bbe20107e50b…) compiles clean with
  `-std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fPIC -march=x86-64`
  and has **zero undefined imports** (self-contained freestanding owner).
- Layout proof: `@mb_win_vm_layout` identical under the PS5 default target and
  `--target=x86_64-unknown-freebsd9 -fshort-wchar`: MBI = [48, 0, 8, 16, 20,
  24, 32, 36, 40] (size 48, offsets match Win64), owner 80 bytes, ops 40,
  pointer 8; target datalayout strings identical.
- SDK declaration coverage asserted (trivially: owner needs nothing).

Test coverage areas: open/close/config validation; alloc lifecycle
(reserve|commit, write/read-back pattern, query fields, release rules);
reserve-then-commit (reserved-state query, interior commit rounding,
zero-fill on first commit, re-commit preserves data and changes protection,
decommit → re-zero, release requires exact base); conflicts (foreign address,
span-overflow commit, non-NULL reserve, protect on uncommitted/free, guard
modifier rejection, NULL old_protect); protect changes (old-protection
return from first page, multi-page runs, backend refusal leaves bookkeeping
unchanged, alloc unwinds reservation on commit refusal); query (committed /
reserved / free runs, low gap, past-end gap to address limit, buffer-too-small,
NULL buffer, bogus process handle, beyond-limit address, QueryEx delegation);
alignment (granularity-aligned bases, one-byte reserve rounds to a full run,
custom 8192/8192 page/granularity config end to end); mmap enforcement
(real mprotect transitions exercised on live pages).

Defects caught and fixed during verification (kept in history, not hidden):
1. ASan heap-use-after-free in `mb_win_vm_free` release path (node fields read
   after the bookkeeping release) — fixed by snapshotting before release.
2. Query originally extended same-state runs forward only; real VirtualQuery
   reports the maximal run in both directions — owner fixed, tests adjusted.
3. Free-gap BaseAddress initially rounded up; Windows rounds down to the
   containing granularity-aligned gap start — owner + tests + header fixed.

## Semantics implemented (Windows-faithful subset)

- `VirtualAlloc`: NULL-base RESERVE (span granularity-rounded, backend span
  aligned up), RESERVE|COMMIT, interior page-rounded COMMIT inside exactly one
  live region; zero-fill on first commit; re-commit only updates protection;
  plain PAGE_* validation (guard/cache modifiers rejected); win32 error codes
  87/487/1455 through the optional error slot.
- `VirtualFree`: RELEASE requires exact region base and size 0; DECOMMIT
  rounds to pages, never fails on uncommitted pages (documented win32),
  pages stay reserved, re-commit re-zeroes; combined flags rejected.
- `VirtualProtect`: page-rounded range fully committed in one region;
  `*old_protect` = first page's protection; new protection applies per page;
  optional backend enforcement with rollback-free bookkeeping on refusal.
- `VirtualQuery`/`VirtualQueryEx`: maximal same-state/protection runs (both
  directions) inside regions; free gaps bounded by next region or the
  128 TiB canonical user limit; MEM_PRIVATE type; Ex validates the
  current-process pseudo-handle only; returns 48 on success, 0 + error 87
  otherwise.

## Known limitations (honest scope)

- No caller-chosen reservation base (backend picks host addresses); hint
  form returns INVALID_ARGUMENT/87. No PAGE_GUARD exception model,
  MEM_RESET/UNRESET, MEM_MAPPED/IMAGE typing, file-backed sections, large
  pages, or NUMA policies.
- Protection is bookkeeping unless `ops.protect` is supplied; decommit does
  not return host pages to the OS before release (commit-charge is modeled,
  not charged against an OS).
- Single-process model; `VirtualQueryEx` accepts only the pseudo-handle.
- All entry points are externally serialized; the owner has no locks,
  TLS, or CRT (matches house rules); it is a prototype-grade emulation, not
  a Windows kernel claim.

## Integration wiring (10 points)

1. Placement: copy `sources/mb_win_vm.{h,c}` beside the other windows-services
   owners (`outputs/ps5-mod-bridge/windows-services/`); files there are
   read-only to this worker, so the integrator performs the copy.
2. Backend binding: instantiate `mb_win_vm_ops` with the SAME backend
   context as the process `mb_win_heap_ops` (allocate/release), plus a
   protect shim (host `mprotect`/`VirtualProtect` equivalent) and an error
   slot writer mirroring the heap's per-thread errno-slot pattern
   (`GetLastError` storage prepared per thread before DLL calls).
3. Import table: KERNEL32.dll thunks map `VirtualAlloc`→`mb_win_vm_alloc`,
   `VirtualFree`→`mb_win_vm_free`, `VirtualProtect`→`mb_win_vm_protect`
   (extra `uint32_t* old` out-param), `VirtualQuery`→`mb_win_vm_query`,
   `VirtualQueryEx`→`mb_win_vm_query_ex` (pass the handle through; only the
   -1 pseudo-handle is valid).
4. `GetSystemInfo`: feed `SYSTEM_INFO.dwPageSize/dwAllocationGranularity`
   from `mb_win_vm_system_info` so the census's GetSystemInfo import and this
   owner can never disagree about page geometry.
5. Win64 ABI thunks: five `win64cc` wrappers translating to the native C ABI;
   MEMORY_BASIC_INFORMATION is 48 bytes/8-aligned on Win64 — layout equality
   across PS5-target and FreeBSD9 `-fshort-wchar` compiles is asserted, so
   the thunk may pass the struct by pointer unchanged.
6. Lifetime: follow the heap-bridge pin pattern — install the vm config
   before module open, keep the owner alive until every session ends;
   `mb_win_vm_close` returns BUSY while regions live, so DLL teardown must
   release its reservations or the module end must explicitly accept the
   documented leak.
7. Error publication: after each failing call the bridge copies the code
   recorded by `ops.error` (87/487/1455) into the calling thread's
   GetLastError slot; success never writes the slot (heap contract parity).
8. Concurrency: serialize all six entry points with the same bridge
   lock/unlock used for installed heap calls; no reentrancy into the owner
   from callbacks; callbacks must not throw/longjmp.
9. Regression gate: rerun `python3 verify_vm.py` after any edit (fresh
   receipt each run; never reuse this receipt) and require
   `VM_FAMILY_HOST_AND_NATIVE_OBJECTS_PASSED` before integration; host-only
   mode (`--host-only`) is available for quick loops.
10. Deferred follow-ups (proposal-only, not blocking): caller-chosen base
    support via a backend map op, PAGE_GUARD one-shot guard pages, actual
    OS commit-charge accounting, MEM_IMAGE typing once image sections are
    tracked, and PE-binder registration of the five KERNEL32 thunks alongside
    the existing heap/CRT binders.

## Rules compliance

Writes confined to
`outputs/2026-10-04-zcode-team/worker-virtual-memory/` (task text wrote
`2026-10-4`; the team's existing directory `2026-10-04-zcode-team` was used to
match every sibling worker). Existing files elsewhere only read. CLAIM.json
written first; this REPORT.md last. No network, no console, no target
execution, no nested subagents.

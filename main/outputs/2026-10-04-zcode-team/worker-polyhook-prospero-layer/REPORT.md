# REPORT — worker-polyhook-prospero-layer (2026-10-04)

Mission: build the required work on the best-case branch — the Prospero
platform layer PolyHook lacks regardless of the kernel verdict
(ue4ss-polyhook-platform-port-1/report.md:20-22) — host-side: protection
translation, old-prot lookup via the proven VMMAP walker, the executable
allocator, host tests on the proven fixture patterns, and a compile gate
against the composition's polyhook unit argv. Deliver the integration diff as
a proposal; composition absorption stays a launch-11 matrix row. Deliverable
order honored (CLAIM.json first, this REPORT last). ~70 tool calls. No
console contact, no nested subagents, no target execution, nothing linked,
no file outside this worker directory written.

## 1. Bottom line

The missing verified contract is now implemented as a PURELY ADDITIVE layer:
one new header (`PolyHookProsperoPlatform.hpp`) and one new TU
(`PolyHookProsperoPlatform.cpp`) define every PLH symbol the port report
listed as missing — protection translation both directions, old-protection
lookup from the current KERN_PROC_VMMAP map, fail-closed MemAccessor
read/write/copy, the bounded near-address executable allocator, and
DebugBreak — with ZERO edits to canonical or port-overlay PLH sources (their
`#if` chains select no branch under `POLYHOOK2_OS_PROSPERO`, so there is no
ODR conflict). Host-tested 71/71 with the proven synthetic-fixture patterns
plus the real PLH MemoryProtector RAII; compile-gated green under the exact
composition argv; the composition patch is a two-new-files-plus-one-list-line
diff. Nothing here runs on target and nothing settles the exec-writability
verdict — the layer is fail-closed on every path where the verdict is
unknown.

## 2. What was built (integration/files/, composed in
integration/polyhook-prospero-platform.diff, sha256 5f4ef82a…b9bb70)

- **Protection translation** (MemProtector.cpp branch): `TranslateProtection`
  both directions, constexpr, Linux-branch semantics under the pinned SDK
  numbering (PROT_NONE 0 / R 1 / W 2 / X 4, sys/mman.h:52-55). No verdict
  dependency — arithmetic only.
- **Old-prot lookup + mem_protect** (MemAccessor.cpp branch): before any
  transition, a CURRENT bounded sysctl collection (igf recipe: mib
  {CTL_KERN 1, KERN_PROC 14, KERN_PROC_VMMAP 32, getpid()}, 65536 first
  probe, bounded doubling, 4 MiB cap) is walked by a byte-close C++ port of
  the proven mode-1 `ig_vmmap_walk` (variable kve_structsize stride, offsets
  0/8/16/56, per-record sanity, 16-KiB census) and gated by the exact proven
  trust gate; the raw kve_protection bits of the single covering span are the
  old protection (the igf-lineage restore-discipline lookup). 16-KiB page
  rounding per pinned PAGE_SHIFT 14 (sysconf fallback only when its return
  is non-positive/invalid — sysconf in-game is itself unproven, documented).
- **Fail-closed access gates** (MemAccessor read/write/copy): every
  dereference requires a trusted current-map R (read) / W (write) span;
  refusals happen BEFORE any dereference (igf4 `covered_not_readable`
  discipline) and surface as PLH-observable failure (UNSET/status=false/
  written=0). Host RAII proof: PLH's MemoryProtector destructor then skips
  the restore (MemProtector.hpp:43) and hook() fails gracefully instead of
  crashing. Stricter than the Linux branch (which gates only the memcpy
  source); divergence documented.
- **Executable allocator** (Misc.cpp branch): bounded ascending page-step
  search inside [start,end) that never hints into an existing mapping and
  never uses MAP_FIXED; raw syscall 477 (SYS_mmap) RWX first; on refusal,
  PROT_READ|PROT_WRITE with an explicit `exec_transition_required` flag —
  the later publish goes through the same raw SYS_mprotect 74 path whose
  success IS the open verdict. Out-of-range results are unmapped and the
  search advances (Linux-branch precedent), hard-capped (4096 tries, then
  scheme fallback inside PLH). A refusing backend yields 0 and PLH falls
  through its detour schemes — never a crash, never an unbounded loop.
- **Raw backend (target-only, compiled not executed)**: inline x86-64 SysV
  `syscall` shims with carry-capture for 74/73/477, libc mmap()/mprotect()
  deliberately NOT used (weak kernel_* statically resolve to 0 in the
  composed PRX toolchain, the AUDIT-RESULTS §2.4 trap class); sysctl
  collector; 4 MiB scratch + 16384-span retention (lineage caps). Raw result
  words land in a Diag surface for post-mortem records; nothing decodes an
  errno (igf5 honesty bound).
- **Backend seam**: five function pointers (collect_vmmap / raw_mprotect /
  raw_mmap / raw_munmap / page_size), the live_module_os_ops.hpp pattern —
  identical PLH-visible logic on host and target, mocks injectable, real
  bindings target-only.
- **PolyHook2DebugBreak**: deliberate no-op on Prospero (int3 would fault the
  game); only reached on PLH's internal invariant paths, which log.

## 3. Host tests (host/, 71 checks ALL PASS, receipted)

Patterns inherited from worker-anchor-validation/tests/vmmap_host.c: synthetic
kinfo_vmentry buffers with variable structsizes, including the exact live
spans `[0x400000,0x787c000)` prot=4 and `[0x970c000,0x9a34000)` prot=3.
Coverage: translation round-trips; walk/trust over live and mixed maps;
insane-structsize stop; old-prot lookup + alignment + exactly-one-syscall;
every refusal path (hole-crossing, uncovered, untrusted, collect failure,
non-zero transition -> UNSET with no restore, truncated map); real-PLH RAII
restore-once-with-X and refusal-skips-restore; gate clamps and refusals with
real host-backed fixture memory; allocator policy matrix; page-size fallback;
exec_publish both ways. One fixture-realism correction was forced by an actual
hang: spans the gates dereference must be backed by REAL host buffers (on
target the kernel map is true, so R-span derefs are real; the fake-address
fixture was memcpy'ing from unmapped host space). The exec-only span stays at
its recorded real address and is only ever refused against — exactly the
igf4/igf6 discipline.

## 4. Compile gate (compile-gate/gate-1/, green)

Exact composition argv (verified byte-for-byte against
baseline-polyhook/polyhook-MemProtector/command.json): prospero-clang++
-std=c++23 -O0 -fno-lto -fPIC -fms-extensions -ferror-limit=8
-Wno-unknown-pragmas -DASMJIT/ASMTK/ZYDIS/ZYCORE_STATIC, patched/include
first, same dependency roots, same LLVM_CONFIG. Five units + a branch-control
TU compile exit 0 (0 errors, 0 warnings); control static-asserts
POLYHOOK2_OS_PROSPERO + POLYHOOK2_ARCH_X64 selection; llvm-nm proves all 14
symbol families the port report listed as missing are now defined;
`linked: false, executed: false` preserved. The gate caught four real
defects before absorption (guard-before-include, u_int needs sys/types.h,
SDK headers inside a namespace, constexpr for static_asserts) — all fixed,
then host re-run to keep the receipt honest.

## 5. Verdict dependencies (documented, not resolved)

- The layer is REQUIRED on the best-case branch (W works) and is the safe
  behavior on the worst ones; it settles nothing about the kernel.
- `mem_protect`/`exec_publish` success = the W-verdict itself; raw syscall
  74 carries igf5's crash correlation, and this layer does not change that
  risk — in-game arming stays IGW-1/controller/user gated.
- Allocator: RWX mmap is attempted first; the RW fallback + `publish` path
  is exactly the W-branch dependency. If BOTH_FAIL holds, PLH cannot obtain
  writable+executable memory and detour arming must stay disabled — the
  allocator returns 0 and PLH's scheme fallback engages.
- safe_mem_read of game text needs RX at minimum: under W-fails-R-works the
  gates unblock FLAG-2/FLAG-3 reads and PLH's disassembly phase, but PLH2
  has no no-write hook mode (EVIDENCE §6) — detours still need the
  dispatch-data alternatives.

## 6. Honesty bounds

All syscall numbers are FreeBSD-generic parsed from the pinned SDK
(sys/syscall.h:81-82,398); only getpid 20 and sysctl 202 are console-proven;
73/74/477 are unverified on this kernel and this worker never issued one. No
claim that mmap/mprotect succeed or fail gracefully on Prospero. AsmJit
JitRuntime viability untouched (port report line 22). No hook activation, no
quality score, no completion claim. Composition absorption (apply diff +
launch-11 matrix row) and any in-game verification are someone else's move.

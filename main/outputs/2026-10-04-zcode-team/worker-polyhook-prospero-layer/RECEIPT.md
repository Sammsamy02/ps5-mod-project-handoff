# RECEIPT — worker-polyhook-prospero-layer (2026-10-04/05 local)

Deliverable order honored: CLAIM.json first, then implementation, host
receipts, compile-gate receipts, integration diff, this RECEIPT, REPORT.md
last. No console contact, no nested subagents, no target execution, nothing
linked, no file outside this worker directory written.

## Deliverables (this directory)

| file | sha256 |
|---|---|
| CLAIM.json | (first deliverable; proposal-only claim set) |
| integration/files/patched-include/polyhook2/PolyHookProsperoPlatform.hpp | d4ec9b1b849ae077202e58d047663d42fb14c0f458bf1f6bf40f7e3b79e8f5a9 |
| integration/files/sources/PolyHookProsperoPlatform.cpp | 1776079210b39dbd7fe9bf0ce1e15c57f77dc38fe577f3b533e814333a53acbe |
| integration/files/build.py.proposed | (proposed poly-list change only) |
| integration/polyhook-prospero-platform.diff | 5f4ef82a9168451f185649fe8eeb6001837b8b8d09ed16f3e6ccd72231b9bb70 |
| host/prospero_platform_host.cpp | c34010f3aa3dbf10c36dbe3193b2594552aae641f845495d767f3c5199e9acfc |
| host/host-checks.txt | 8b0193b2a35ea356a2568ab6d917701ceea4034151abd08a6ae01fba16eeca04 |
| host/host-receipt.json | (71 checks, ALL PASS) |
| compile-gate/gate_compile.py | (gate script; exact composition argv) |
| compile-gate/gate-1/result.json | (all_compiled true, nm_missing [], linked false, executed false) |

## Host checks (host/host-receipt.json)

Apple clang 17.0.0, macOS 15.7.8 arm64. 71 checks, 0 failures: translation
round-trips; proven mode-1 walk over the exact live igf spans (prot=4 exec /
prot=3 RW, ksz 144/1160 mix, insane-structsize stop, trust gate); old-prot
lookup with 16-KiB page rounding and exactly one raw_mprotect per transition;
all fail-closed refusals; PLH MemoryProtector RAII restore/skip-restore chain;
accessor gates with REAL host-backed fixture memory; allocator policy incl.
RWX-refusal -> RW + exec_transition_required and bounded attempts; page-size
fallback; exec_publish. Fixture-realism note in the receipt: every span the
gates dereference is backed by real host buffers; the exec-only span is only
ever refused against, matching igf4/igf6 discipline.

## Compile gate (compile-gate/gate-1/)

Exact composition polyhook unit argv (prospero-clang++ -std=c++23 -O0
-fno-lto -fPIC -fms-extensions ... -DASMJIT_STATIC -DASMTK_STATIC
-DZYDIS_STATIC_BUILD -DZYCORE_STATIC_BUILD, patched/include first, same
dependency include roots, same LLVM_CONFIG; verified against
baseline-polyhook/polyhook-MemProtector/command.json). Five units +
branch-control compile exit 0, 0 errors 0 warnings:
  PolyHookProsperoPlatform.cpp (new) — 26 072-byte ELF object
  MemAccessor.cpp (proposed new unit) / MemProtector.cpp (unchanged) /
  Misc.cpp (port overlay, the composition's non-baseline selection) /
  PolyHookOs.cpp (unchanged)
Branch control static-asserts POLYHOOK2_OS_PROSPERO + POLYHOOK2_ARCH_X64
selection and constexpr translation sanity. llvm-nm over the new object:
all 14 expected symbol families defined (TranslateProtection x2,
MemAccessor::mem_copy/safe_mem_write/safe_mem_read/mem_protect,
boundedAllocSupported, boundAlloc, boundAllocLegacy, boundAllocFree,
getAllocationAlignment, getPageSize, PolyHook2DebugBreak,
plh_prospero::active_backend). linked: false, executed: false.

## Gate-found corrections (why the gate earned its place)

1. `PLH::ProtFlag`-block guards must follow `#include "polyhook2/PolyHookOs.hpp"`
   (the macro comes from the header, not the compiler).
2. SDK `sys/sysctl.h` needs `<sys/types.h>` first (u_int, types.h:52).
3. SDK headers must never be opened inside namespace plh_prospero
   (stdio/time declaration poisoning).
4. Translation functions made constexpr (branch-control static_asserts).

## Not claimed

No kernel behavior on target (syscalls 73/74/477 unverified; only 20/202
console-proven); no in-game effect; no quality score; composition absorption
left to the launch-11 matrix row.

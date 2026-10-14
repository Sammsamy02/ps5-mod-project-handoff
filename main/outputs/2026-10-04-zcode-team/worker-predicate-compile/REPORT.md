# REPORT.md — worker-predicate-compile
Team 2026-10-04-zcode-team · 2026-10-04 · batch1 GUObjectArray predicate compile for in-PRX use

## Result: COMPLETE (compile + host-test level; no real-PRX/console claim)

All five task steps executed inside the claimed bounds (~20 tool calls of the
<=60 budget). Every claim below has its raw evidence in this directory.

## 1. Vendored predicate — API verified

`src/townfall_layout_probes.{hpp,cpp}` are byte-identical copies of
`worker-5-layout-validator/src/` (cp + sha256 verified):

- hpp: `270671d2a6da0a9012ca2153b29eff4d37e6af56652fd48eabea86e421934c52`
- cpp: `9a68d03b161b08fd9a039b966ffc9ad32feed64b356dfb297f44320a21fe4439`

Verified API surface (read from the vendored sources, not from reports):
- `struct tlp::MemSpan { uint64_t start; uint64_t size; const uint8_t* data; }`
  (hpp:97) — start inclusive, size bytes, host pointer never touched beyond
  `[data, data+size)`.
- `GuaProbeResult probe_guobject_array(const MemSpan*, size_t, uint64_t image_base, uint64_t goa_vaddr) noexcept`
  (hpp:211) — vaddr supplied as ARGUMENT, combined with base under overflow
  check (cpp:318).
- Both hypotheses evaluated independently per call: Layout A =
  `FUObjectArray_Outer` (GC indices +0x0/+0x4, embedded TUObjectArray +0x10),
  Layout B = `TUObjectArray_Direct` (cpp `run_chain(..., tua_off=0x10/0x0)`).
- Verdicts `NotProbed/Validated/Inconclusive/Refuted` (hpp:108) and the full
  RCode set 0x1001..0x110A (hpp:115), combine rules honest per hpp VERDICT
  DISCIPLINE (exactly-one-Validated -> Validated; both -> Inconclusive
  `GOA_E_LAYOUT_AMBIGUOUS`; both-Refuted -> Refuted; else Inconclusive),
  implemented at cpp:359-400.
- Dependencies: NONE outside `<cstddef>/<cstdint>`. The cited TUObjectArray /
  FUObjectArray offsets and chunk constants are inlined as named constants
  WITH citations in the provenance blocks (hpp:13-79, cpp:19-31). Per the
  task bound, NO coherent-port headers were vendored — nothing was needed;
  the empty `llvm-nm -u` on the PS5 predicate object is hard proof.

## 2. C wrapper `src/predicate_call.{h,c}`

- `predicate_call.h` — pure-C consumable; fixed-layout POD `pc_gua_verdict`
  (96 bytes, all scalars, memcpy-safe: verdict, matched layout, per-layout
  sub-verdicts, up to 8 combined rcodes, the full GuaObserved scalar set).
  `PC_V_*`, `PC_LAYOUT_*`, `PC_R_*` mirror the C++ enums' fixed underlying
  values (`enum class : uint8_t/uint16_t`).
- `predicate_call.c` — takes `const void* image, size_t length, uint64_t
  image_base, uint64_t goa_vaddr, pc_gua_verdict* out`, builds ONE MemSpan
  `{start=image_base, size=length, data=image}`, calls
  `tlp::probe_guobject_array`, maps the result to the POD. Returns 0, or -1
  with `verdict=NOT_PROBED, codes[0]=PC_R_CALL_E_BAD_ARGS` on null/zero-length
  args.
- Honest limitation (documented in header + CONSUME.md): the single-span form
  validates fully only when the chunk pointers fall inside the passed region;
  real module use wanting multi-span coverage should call
  `tlp::probe_guobject_array` directly with the worker-1 span list.
- The `.c` keeps its extension per contract but is C++-compiled (it calls the
  C++ entry point); consumers pass `-x c++` (clang deprecates implicit
  `.c`-as-C++ under `-Werror` — this exact error was hit and fixed here).

## 3. Host test — 29/29 PASS

`tests/test_predicate_call.cpp` reuses the sibling fixture pattern (Arena
put32/put64, `ImgOpts`-style builders) compressed into ONE contiguous
synthetic **96-byte image** (the wrapper exposes one span; the sibling's
multi-span arena cannot drive it). Coverage, each citing its chain step:

- args: null image / null out / zero length -> rc -1 + `PC_R_CALL_E_BAD_ARGS` (3 PASS)
- Layout A image -> combined **Validated**, layout A matched, sub-verdicts
  A=Validated / B=Refuted, observed GC indices/counts/pointers/chunks_scanned correct (8 PASS)
- Layout B image -> combined **Validated**, layout B matched, A refuted at
  chain step P1 (`A.Objects = u64@0x10 = {MaxElements,NumElements} =
  0x100020000`, canonical+aligned but outside the span), GC indices -1 (6 PASS)
- Refuted: counts insane (`GOA_R_COUNTS_INSANE`), non-canonical pointer
  (`GOA_R_PTR_NONCANONICAL`), needed chunk unreachable
  (`GOA_R_CHUNK_UNREADABLE`); layout None on all (7 PASS)
- Inconclusive: truncated span -> `GOA_E_ADDR_UNREADABLE` both layouts (4 PASS)
- `sizeof(pc_gua_verdict)==96` printed in the run log.

Fixture corrections made during bring-up (both proven from the predicate
chain, not tuned to pass): MaxChunks must be >=2 for MaxElements=0x20000
(P2 bound `Max <= MaxChunks*64Ki`); on B-images A refutes at P1 before the P3
GC-order check. The compact fixture places the label at vaddr 0 (the wrapper
takes vaddr as an argument; the real 0x93ba110 profile fact is the anchor
probe's call site with the real mapped image).

Run log: `tests/test_predicate_call` output `ALL TESTS PASSED` (29 PASS, 0
FAIL) — reproduced by rebuilding with
`c++ -std=c++17 -Wall -Wextra -Werror -Isrc -x c++ tests/test_predicate_call.cpp src/predicate_call.c src/townfall_layout_probes.cpp`.

## 4. PS5 cross-compile — clean, AMD64

`build_ps5.sh` (self-contained, emits `ps5/build-receipt.json` with argv/rc/
output per step) compiles both TUs with the PINNED lineage toolchain and the
EXACT v2/v3 CPPFLAGS (`worker-ingame-probe-v3/build_ingame.py` lines 36-59):

- `prospero-clang++` from `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin`,
  `LLVM_CONFIG` pinned to `.../LLVM-21.1.8-macOS-ARM64/bin/llvm-config` (same
  pinning as the v3 `run()` helper);
- flags: `-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
  -fvisibility=hidden -ffreestanding -fno-builtin
  -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti
  -fno-threadsafe-statics` (v3 `CPPFLAGS` verbatim).

Evidence (ps5/build-receipt.json):
- both compiles rc=0 under `-Werror` (no warnings possible-or-present);
- `llvm-objdump -f`: BOTH objects `file format elf64-x86-64, architecture
  x86_64` (AMD64, as required for the PS5 target);
- `llvm-nm`: `predicate_call.o` defines exactly `predicate_call_guobject_array`
  and references only `_ZN3tlp20probe_guobject_arrayEPKNS_7MemSpanEmmm`;
  `townfall_layout_probes.o` has ZERO undefined symbols;
- `ld.lld -r` combine (same style as the PRX pipeline) -> `predicate_bundle.o`:
  elf64-x86-64, `llvm-nm -u` EMPTY — fully self-contained, no libc/libstdc++
  imports.

Object sha256:
- `ps5/townfall_layout_probes.o` `2acd32177659ddce5d74190d74fac109cfc909fcdc669a9e6bb80ff5608b9ca9`
- `ps5/predicate_call.o`        `66d6ae32a20a6952a637678fccf53cbbc01bd23aa479af1d7af333809da5129e`
- `ps5/predicate_bundle.o`      `5d1132c20d4d614c6c16d5a249e0f5ccf5944f5891443a6935b087e00c17d475`

## 5. Delivery for the anchor-validation probe

Exact include path + flags + link options + runtime contract:
`ps5/CONSUME.md`. Summary:

- include: `-I outputs/2026-10-04-zcode-team/worker-predicate-compile/src`
- compile flags: the v2/v3 CPPFLAGS verbatim (above), toolchain
  `prospero-clang++` with `LLVM_CONFIG` pinned to LLVM 21.1.8; add `-x c++`
  for `predicate_call.c`
- or simplest: add `ps5/predicate_bundle.o` to the probe's combine/link inputs
  (self-contained; exports one global `predicate_call_guobject_array`;
  tlp:: symbols hidden under `-fvisibility=hidden`, matching v2/v3 discipline)
- runtime: `predicate_call_guobject_array(image, length, image_base,
  0x93ba110ULL, &v)` with worker-1 image_base facts and the worker-2 profile
  vaddr; rc/verdict semantics in CONSUME.md.

## Honest scope

PROVEN: predicate API (from vendored sources), wrapper behavior on synthetic
96-byte images (29/29), warning-free pinned-toolchain compile to AMD64 ELF64
relocatables, self-contained link.

NOT PROVEN (no claim made): real-PRX integration, loading in the running game
module, live-memory validation of GUObjectArray on the actual townfall image,
any elevated layout-fact status for the real target. Those need the anchor
probe's own runs against real worker-1 captures.

## Bounds compliance

No network/console; no nested subagents; only files under
`outputs/2026-10-04-zcode-team/worker-predicate-compile/` were written;
vendored inputs read-only (hashes recorded). ~20 tool calls of <=60.

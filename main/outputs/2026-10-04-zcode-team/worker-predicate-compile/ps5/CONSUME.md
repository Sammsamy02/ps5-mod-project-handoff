# CONSUME.md — anchor-validation probe consumption contract
(worker-predicate-compile, team 2026-10-04-zcode-team)

## What to consume

| File | What |
|---|---|
| `../src/predicate_call.h` | Pure-C header for the wrapper POD + entry point |
| `../src/predicate_call.c` | Wrapper implementation (C++-compiled; `-x c++`) |
| `../src/townfall_layout_probes.{hpp,cpp}` | Vendored predicate, byte-identical to worker-5 (sha256 in ../REPORT.md) |
| `predicate_bundle.o` | ld.lld -r combine of the two objects below; ZERO undefined symbols |
| `townfall_layout_probes.o` | predicate alone |
| `predicate_call.o` | wrapper alone (U: `_ZN3tlp20probe_guobject_arrayEPKNS_7MemSpanEmmm`) |

All objects are ELF64 **x86-64 (AMD64)** relocatable, built warning-free under
`-Werror` with the frozen v2/v3 CPPFLAGS.

## Exact include path

```
-I outputs/2026-10-04-zcode-team/worker-predicate-compile/src
```
(all paths relative to the `what` worktree root)

## Exact compile flags (frozen v2/v3 lineage — worker-ingame-probe-v3/build_ingame.py)

Toolchain:
```
outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++
# with env LLVM_CONFIG =
#   outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config
```

CPPFLAGS (verbatim v3 `CPPFLAGS`):
```
-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector -fvisibility=hidden
-ffreestanding -fno-builtin -fno-asynchronous-unwind-tables
-fno-exceptions -fno-rtti -fno-threadsafe-statics
```

If compiling the SOURCES instead of linking the objects:

```
prospero-clang++ <CPPFLAGS> -I .../worker-predicate-compile/src -x c++ \
    -c .../src/townfall_layout_probes.cpp -o townfall_layout_probes.o
prospero-clang++ <CPPFLAGS> -I .../worker-predicate-compile/src -x c++ \
    -c .../src/predicate_call.c        -o predicate_call.o
```

NOTE: `predicate_call.c` keeps its `.c` extension per task contract but is
C++-compiled (it calls the C++ predicate entry point directly). The explicit
`-x c++` avoids clang's "treating 'c' input as 'c++'" `-Wdeprecated` error
under `-Werror`. Add it to the probe's `CPP_SOURCES` equivalent, not
`C_SOURCES`.

Simplest consumption: add `ps5/predicate_bundle.o` to the probe's combine/link
inputs (same `ld.lld -r` style the PRX pipeline uses). It needs NO libc,
libstdc++, or other imports (freestanding, no exceptions/rtti; verified
`llvm-nm -u` empty).

## Runtime contract

```c
#include "predicate_call.h"   /* pure C */

pc_gua_verdict v;
int rc = predicate_call_guobject_array(
    image,          /* const void*: readable bytes (ONE contiguous region) */
    length,         /* size_t: readable byte count */
    image_base,     /* uint64_t: virtual base the bytes are mapped at (worker-1 live fact) */
    0x93ba110ULL,   /* uint64_t: GUObjectArray vaddr (worker-2 profile fact) */
    &v);            /* always written: POD verdict, 96 bytes, memcpy-safe */
/* rc==0: v.verdict is PC_V_VALIDATED / PC_V_INCONCLUSIVE / PC_V_REFUTED
   rc==-1: bad args (image/out NULL or length 0); v.verdict==PC_V_NOT_PROBED,
           v.codes[0]==PC_R_CALL_E_BAD_ARGS */
```

- vaddr semantics: ELF p_vaddr file-image offsets; absolute label = image_base
  + goa_vaddr (proven by worker-vaddr-semantics for this capture lineage).
- The ONE-span contract means chunk pointers must fall inside the passed
  region for full validation; outside-but-headered evidence yields
  Inconclusive `GOA_E_CHUNK_EXTENT_UNVERIFIABLE`/`GOA_E_ADDR_UNREADABLE`, and
  the REAL module use should instead call `tlp::probe_guobject_array` directly
  with the full worker-1 span list (C++ link) — the C wrapper is the
  minimal single-span convenience form.
- `predicate_bundle.o` exports exactly one global: `predicate_call_guobject_array`.
  With `-fvisibility=hidden` the tlp:: symbols are hidden-local to the PRX,
  matching the v2/v3 export discipline.

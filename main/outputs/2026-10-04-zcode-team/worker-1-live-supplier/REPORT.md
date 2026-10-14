# worker-1-live-supplier — Retained live-module facts supplier

Date: 2026-10-04. Slot 2 of the ZCode team taking over the Codex handoff.
Output area (only place written):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-1-live-supplier/`

## What was implemented

A typed, retained live-module facts supplier for the UE4SS PS5 port, per
NEXT_ACTIONS.txt item 2, built ONLY on APIs that are actually pinned in this
workspace. Layering (all files under `src/`):

| File | Role |
|---|---|
| `live_module_facts.hpp/.cpp` | Generic core: validation, retention state machine, capability recording. NO OS calls, NO firmware constants, NO title names, NO dereference of any reported address. |
| `live_module_os_ops.hpp` | Host-injectable OS boundary (`LiveModuleOsOps` function-pointer table). |
| `sce_module_abi.hpp` | Private spellings of the pinned prosperodev module ABI (352-byte `SceKernelModuleInfo`) with LP64 static_asserts. |
| `live_module_os_ops_sce.cpp` | The ONLY TU binding real SDK symbols; `make_sce_live_module_os_ops()` builds the ops table. |
| `host_tests/test_live_supplier_host.cpp` | 11 host tests with injected fake ops. |
| `verify.py` | Host tests + pinned-SDK compile of this worker's files only. |

Supplier semantics (class `lmf::RetainedLiveModuleFactsSupplier`):

- **Image identity**: this image's own `_DYNAMIC` array (linker-provided
  standard ELF symbol) → its `DT_DEBUG` entry → `struct r_debug` → `r_map`
  link-map walk (bounded 4096 steps). DISCOVERED FACT: `dlinfo` +
  `RTLD_DI_LINKMAP` is declared in the SDK `dlfcn.h` but exported by NO
  library in the pinned SDK (verified with `nm` over every
  `target/lib/*.so` and `*.a`) — using it would not link, so the standard
  ELF `DT_DEBUG` route is used instead; all structs involved
  (`Elf64_Dyn`, `r_debug`, `link_map`) are pinned SDK headers. The anchor
  image is the first link-map entry matching the firmware/profile-supplied
  suffix (e.g. "eboot.bin" — data, not code), or an explicit caller name
  (corroborated against the walk when present). `dlopen(NULL)` is never
  used as an identity anchor (it is the payload root per the handoff).
  `sysctl(KERN_PROC_PATHNAME)` independently records the observed
  executable path of the live process.
- **Retention (lifetime)**: a genuine dynamic-loader reference —
  `dlopen(anchor_name, RTLD_NOLOAD)` (never loads new bytes), released exactly
  once by `dlclose` (idempotent `release()`, destructor balances a forgotten
  release). `validate_retention()` re-probes with a second NOLOAD retain and
  immediately balances it. A failed retain yields NO facts
  (`Status::RetainFailed`) — never a fake "enabled" state.
- **Mapped segment correspondence + sizes**: when a `SceKernelModule` handle
  is available, `sceKernelGetModuleInfo` returns the authoritative ≤4 segment
  descriptors (address/size/prot, prot kept opaque); each segment is
  cross-checked against `sysctl(KERN_PROC_VMMAP)` mappings
  (`SegmentCorrespondence`: intersecting count + full-coverage flag). Without
  a handle, the mapping containing the observed link-map base is still
  recorded (`anchor_mappings`) and absence is explicit. Image span
  (min address..max checked end) is computed with overflow checks.
- **Capability queries**: `CapabilitySet` records per-API observed outcomes
  (`NotBound`/`Failed`/`Ok`) for pid, exec path, link-map walk, retain,
  module info, vmentry, exec-handle probe, plus `module_info_degraded` and
  `vm_mappings_truncated`. Firmware facts enter ONLY through
  `LiveFirmwareProfile` (ABI size, segment capacity, fw version 0=unknown,
  `allow_loader_handle_as_module_handle`, exe-name suffix).

## API provenance (every OS API used)

| API | Header (pinned SDK) | Exact signature | Symbol/export evidence |
|---|---|---|---|
| dlopen | `target/include/dlfcn.h` | `void *dlopen(const char *, int);` (line 117); `RTLD_NOLOAD 0x02000` (line 47) | `nm -D target/lib/libkernel.so` → `T dlopen` |
| dlclose | `target/include/dlfcn.h` | `int dlclose(void *);` (line 115) | `nm -D` → `T dlclose` |
| _DYNAMIC | standard ELF linker-provided symbol of this image; declared `extern "C" Elf64_Dyn _DYNAMIC[];` | resolved at final link from the payload's own dynamic section | `nm target/lib/crt1.o` references `_DYNAMIC` (U) |
| Elf64_Dyn | `target/include/sys/elf64.h` | `typedef struct { Elf64_Sxword d_tag; union { Elf64_Xword d_val; Elf64_Addr d_ptr; } d_un; } Elf64_Dyn;` (line 136) | data layout only |
| DT_DEBUG | `target/include/sys/elf_common.h` | `#define DT_DEBUG 21` (line 559) | value set by rtld at load (TARGET experiment 1) |
| r_debug, link_map | `target/include/sys/link_elf.h` | `struct r_debug { int r_version; struct link_map *r_map; ... }` (line 68ff); `typedef struct link_map { caddr_t l_addr; const char *l_name; const void *l_ld; struct link_map *l_next, *l_prev; }` (lines 57-65) | reached via DT_DEBUG pointer |
| sysctl | `target/include/sys/sysctl.h` | `int sysctl(const int *, u_int, void *, size_t *, const void *, size_t);` | `nm -D` → `T sysctl`; `KERN_PROC_PATHNAME 12` (line 894), `KERN_PROC_VMMAP 32` (line 903) |
| kinfo_vmentry | `target/include/sys/user.h` | `struct kinfo_vmentry { ... uint64_t kve_start; uint64_t kve_end; int kve_protection; ... }` (line 466ff) | converted to `OsVmMapping`, kernel layout never leaves the SCE TU |
| getpid | `target/include/unistd.h` | `pid_t getpid(void);` | `nm -D` → `T getpid` |
| sceKernelGetModuleInfo | NOT in any SDK header; pinned verbatim at `outputs/ps5-mod-bridge/native-module-metadata/upstream/libkernel.h` line 159 (prosperodev MIT, commit 3cdd09c, retained as provenance) | `int sceKernelGetModuleInfo(SceKernelModule handle, SceKernelModuleInfo *info);` | `nm -D target/lib/libkernel.so` → `T sceKernelGetModuleInfo`; 352-byte struct ABI + provider contract already validated by prior-art `native-module-metadata` (28 tests, host-compiled with this same SDK) |

NOT used despite being declared: `dlinfo`/`RTLD_DI_LINKMAP`
(`target/include/dlfcn.h` lines 52, 124) — exported by NO pinned library
(`nm` over every `target/lib/*.so` and `*.a` finds no `T dlinfo`), so a
target link would fail. Recorded as a discovered SDK gap.

SCE TU undefined symbols after compile (complete import set, from receipt):
`_DYNAMIC dlclose dlopen getpid memset sceKernelGetModuleInfo sysctl`.
Every one is either exported by the pinned libkernel.so or linker-provided
(`_DYNAMIC`); `memset` is compiler-generated.

## Evidence

Host-side (this machine, pinned tools):

```
python3 outputs/2026-10-04-zcode-team/worker-1-live-supplier/verify.py
→ ALL LIVE SUPPLIER HOST TESTS PASSED          (11 tests, see list in source)
→ live_module_facts.cpp + live_module_os_ops_sce.cpp compiled clean with
  outputs/.../sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++
  -std=c++17 -Wall -Wextra -Werror -fPIC -c   (0 errors, 0 warnings)
→ objects verified ELF64 LE ET_REL AMD64; receipt:
  build-receipts/run-1/receipt.json (status HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED)
  live_module_facts.o        sha256 bc7c6a43c747098587fa02a865f127670af34fcb177b4520f88bff59d569c1d5
  live_module_os_ops_sce.o   sha256 76733b651e5d91c1f1bca9729fa3a1931506e24e1c8343df89ba78492f2d14a5
  liblmf_live_supplier.a     sha256 14e4d5cfc70bdc47c19e8556167f16114dfb2e373c94f0d15aead4dbcd427d66
```

Toolchain notes: pinned LLVM dir ships no `llvm-ar` and the SDK's
`prospero-ar` wrapper requires an absent `LLVM_BINDIR` (same gap prior art
recorded); the convenience archive is therefore made with host `/usr/bin/ar`
— evidence is the two verified objects, not the archive.

Host tests cover: full success via bound exec-handle probe; degraded mode
(no bridge → module info never called, retention+identity still real);
loader-handle-cast path; retain failure → zero facts; no anchor → NO
dlopen(NULL) fallback; explicit name with/without walk corroboration; the
five ModuleInfo rejection rules (size echo, provider error, untermitted
name, zero segments, overflow end); retention lifecycle (probe balancing,
exactly-once release, destructor balance); move semantics; vmentry
absence/truncation honesty; invalid ops/profile rejection. All fake
addresses are deliberately unmapped host values — the core provably never
dereferences them.

Target (PS5 / Townfall process): NONE. Nothing was uploaded, executed or
observed on the console. Everything above is host-side compile/contract
evidence only. In particular there is NO proof yet that, inside the live
game process, the link-map walk sees the game image, that RTLD_NOLOAD can
retain it, or that KERN_PROC_VMMAP is permitted for a payload process.

## Unresolved facts + smallest REAL runtime experiments

Each experiment is a single standalone payload run inside the live game
process (controller-owned dispatch, one JSONL record each; no source changes
to shared files):

1. **DT_DEBUG population + link-map visibility of the game executable**
   (blocks identity, not compile): run `lmf_sce_link_map_snapshot`
   in-process, print whether DT_DEBUG exists/is nonzero, then every
   `l_name`/`l_addr`. Success = DT_DEBUG filled AND an entry matching the
   eboot path appears alongside `sysctl(KERN_PROC_PATHNAME)`. If DT_DEBUG is
   not filled by the PS5 rtld, identity needs the exec-handle route (fact 3).
2. **RTLD_NOLOAD retainability of the game image** (blocks retention): after
   fact 1, `dlopen(eboot_name, RTLD_NOLOAD)`; print handle vs null, then
   `dlclose`. If null, a handle-validated lease needs fact 3/4.
3. **`sceKernelGetExecutableModuleHandle` signature** (exported; no pinned
   declaration): standalone probe printing the raw return as int64 plus
   errno, then `sceKernelGetModuleInfo(ret_as_handle)` with a 352-byte
   buffer. Success criteria: nonzero small handle AND size echoed = 352 AND
   plausible segments. The ops slot already exists for the verified binding.
4. **`sceKernelGetModuleList` size unit** (bytes vs elements) — pinned
   observation `native-module-list-abi-observation-1` says unresolved: call
   with capacity N handles passing `size = 4*N` then `size = N`, compare
   `*available` and which variant yields handles valid under
   `sceKernelGetModuleInfo`. Result unlocks enumeration (still unused here).
5. **KERN_PROC_VMMAP permission + count for a payload process**: call
   sysctl, print rc/errno and entry count. If denied, mapping
   correspondence stays absent (never guessed).
6. **Loader-handle ↔ SceKernelModule-handle domain equality** (gates
   `allow_loader_handle_as_module_handle`): compare the dlopen NOLOAD handle
   value with the fact-3 handle for the same image. Until proven equal, the
   profile flag stays false and module info stays degraded by default.
7. **Firmware profile values for 13.60** (`fw_version`, ABI stability):
   from `kernel_get_fw_version()` (pinned `ps5/kernel.h` line 59) or console
   settings observation — recorded into profile data by the profile owner,
   never hardcoded here. NOT probed by this worker (no kernel access used).

Also exported by libkernel.so but with NO pinned signature and therefore
never called: `sceKernelGetModuleInfoFromAddr`, `sceKernelIsModuleLoaded`,
`sceKernelGetProcessName`,
`sceKernelInternalMemoryGetModuleSegmentInfo`, `sceKernelGetModuleInfo2/
ForUnwind/Internal`, `get_module_info_list`. prosperodev declares
`sceKernelGetModuleInfoByName` but the pinned libkernel.so does NOT export
it (verified by `nm -D`), so it is unusable despite the declaration.

## Typed contract for the controller

Conversion to the existing (immutable) loader context, i.e.
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp`
`RC::Platform::NativeLoaderFacts`:

- `game_executable_path` ← `facts.executable_path` (observed sysctl path)
  or `facts.image_name` (link-map name); if both absent, the capture failed
  and must NOT be fabricated. Recommend sysctl path as authoritative.
- `mapped.image_base` ← `facts.module_info.image_span_begin` (checked min
  segment address) when `module_info.obtained`; else `observed_base` with an
  explicit weaker-provenance note. `mapped.format` = PS_MAPPED_IMAGE_ELF64
  per the existing factory requirement.
- `mapped.text_base` ← NOT supplied here: identifying the executable segment
  requires parsing the mapped ELF program headers (a dereference this
  supplier intentionally never performs) or a future pinned VirtualQuery
  ABI. Keep it unresolved rather than guessing.
- `modules`/`ModuleInput.lifetime` ← the supplier instance itself: keep
  `RetainedLiveModuleFactsSupplier` alive (it owns the loader reference) for
  the whole scan lease, then call `release()`. A supplier that returned
  `RetainFailed`/`NoImageAnchor` must not be converted.
- Gate: only convert when `facts.status == Ok` (full facts) —
  `ModuleInfoNotObtained` is legal for retention+identity-only use but must
  not be presented as validated engine facts to the factory.
- Firmware profile construction for 13.60 belongs to the controller/profile
  owner from experiment outputs above; `make_sce_live_module_os_ops(nullptr)`
  + `LiveFirmwareProfile{352,4,fw,bridge_flag,"eboot.bin"}` is the only
  target-side entry point.

## Deviations / notes

- All brief paths were present and as described; nothing materially missing.
- The brief's "module query/load/listing" listing APIs exist as exports but
  only `sceKernelGetModuleInfo` has a pinned signature+struct, so enumeration
  stays gated exactly like prior art; the supplier instead anchors identity
  on dlinfo/sysctl, which ARE fully pinned — this is the honest superset.
- Shared sources (LoaderContext, ProgramFactory, build.py, TRANSPORT_CHECKPOINT,
  receipts) were not modified; integration is proposed above.

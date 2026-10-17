# CAP13 — Genericity audit of the DLL compatibility layer

Agent KF (ZCode), 2026-10-13, cap13 packet. **Mac-only audit + evidence. No
console contact.** This report applies the user's nine genericity requirements
to every existing cap13 component, then states what the first Mac end-to-end
proof (REPORT/E2E-PROOF.md) actually demonstrated, separated from what is only
import coverage (REPORT/UNSEEN-MOD-TEST.md).

Evidence labels (house style, inherited from the design docs):
- **[LOCAL-SOURCE]** — fact from a file in this worktree (path:line).
- **[LOCAL-VERIFY]** — measured on this Mac today (command/output quoted or
  summarized; raw logs under `REPORT/evidence/`).
- **[INFERENCE]** — my reasoning.
- **[TRAINING-KNOWLEDGE]** — external, version-sensitive; pin before use.

---

## 0. Verdict box

1. **The loader layer is genuinely generic.** `native-loader/src/pe_loader.cpp`
   contains no game name, no mod name, no hardcoded offset, no per-file
   special case. Every decision is derived from the file itself (headers,
   section table, data directories, import/export tables).
   [LOCAL-SOURCE: pe_loader.cpp — full read; grep for game/mod names across
   `native-loader/` finds zero hits outside comments.]
2. **Genericity is cleanest exactly where the user demanded it**: the PE mapper
   refuses by *format property* (TLS directory, CLR descriptor, non-x64
   machine, RWX partition, unresolvable import), never by identity.
3. **The compatibility layers are generic by policy tables, not by code**: the
   Win API shim is a registry keyed by `(dll, symbol)` with tiered modes;
   the UE4SS compat layer is census-driven (rows come from each mod's import
   table, not from a per-mod hand-written list). But several staged rows are
   wrong or missing against the *measured* census — these are completeness
   bugs, not genericity violations, and they are itemized in §7 (F1, F3, F4,
   F14).
4. **The single largest honest gap is the ABI-bridge signature story**: today
   only all-integer signatures with ≤8 args can cross, via 5 hand-written asm
   thunk classes; the design's "per-signature thunks generated from a
   signature table" (M1 generator) **does not exist yet**
   [LOCAL-SOURCE: win64_bridge.h:8-18 — the header says so itself].
   Everything with a float in the argument list — including the CppUserModBase
   `on_update(float)` virtual and the `mbm_tick_fn(user, float)` host hook —
   is refused today. That is honest refusal, but it bounds what "generic"
   currently means. **(Update, same day: the all-integer envelope — both
   directions through the staged asm — now has execution evidence on Mac;
   see E2E-PROOF.md. The g2h s0 class passed; the s1..s4 stack-staging
   classes could not be validated under this Rosetta build (wedge, measured)
   and fall to the console rung.)**
5. **"Imports resolved" and "actually executed" are now separated by
   measurement, not assertion**: the Mac E2E (first run ever of this loader on
   real PE bytes) shows both states exist — e.g. `cap13-feature-probe.dll`
   imports resolve *class-wise* but its patching/EH/TLS feature set is exactly
   the refused envelope; the unseen-mod runs show load results varying with
   each DLL's own features, with zero loader changes. See the two proof
   reports.

---

## 1. The user's nine genericity requirements — where cap13 stands

| # | Requirement | Status today | Evidence |
|---|---|---|---|
| 1 | Separate genericity from compatibility completeness | **PASS by structure** — the mapper (generic) is a separate module from the shim registry (completeness) and the UE4SS surface (framework scope). This report and the two proof reports keep the separation explicit. | §2–§6 below |
| 2 | State supported PE/ABI/runtime features precisely | **PASS after this audit** — stated per component in §2–§6; previously spread across design §3–§5 and partially aspirational ("design-grade"). | §2–§6 |
| 3 | Explain unsupported cases precisely (not "doesn't work") | **PASS in the loader** (every refusal is a named string, e.g. the TLS refusal names gs/TEB; pe_loader.cpp:179-181, 126-128). **PARTIAL in the shim** — fail-closed stubs diagnose at execution, but two refusal *names* were wrong/missing (F1, F14). | pe_loader.cpp; win_api_shim.cpp:338-343 |
| 4 | Show how bridge signatures are obtained | **PARTIAL** — guest→host: hand-authored rows whose prototypes are documented Win32 signatures (not generated, not per-game); host→guest: a fixed arity family (call0..6) + 5 shared arity thunks. The census-driven *generator* is designed but not built. No hand-written per-function thunks exist anywhere. | win64_bridge.h:8-18, 51-96; win_api_shim.cpp:105-107 |
| 5 | C++ object/vtable/lifetime compatibility beyond export names | **DESIGNED, NOT PROVEN** — MSVC bare-array vtables, vptr@0, dtor-pair slot order, shim vftable images, allocator-pairing rule, MI/virtual-base refusal are all designed (design §4b/§6b) with a Mac pinning tool; **none of it is exercised by any run yet**, and `ue4ss_compat.cpp` is explicitly sketch-grade with `kLayoutPinned = false`. | ue4ss_compat.cpp:50-60; tools/pin_msvc_layout.sh |
| 6 | Report import coverage separately from demonstrated execution | **PASS now** — E2E-PROOF.md reports execution events; UNSEEN-MOD-TEST.md reports load/import coverage per DLL; this audit never converts one into the other. | both proof reports |
| 7 | Preserve original DLL files | **PASS in code + proven** — loader only patches its in-memory copy (pe_loader.cpp:206, 242-249, 314); the E2E hashes every input file before and after and asserts byte identity (E2E-PROOF.md §4). | pe_loader.h:3-7; E2E evidence |
| 8 | Use previously-unseen mods to validate generalization | **PASS now** — 12 KA-built DLLs + 2 real UE4SS-distribution binaries were run through the unmodified loader after this audit; results in UNSEEN-MOD-TEST.md. | UNSEEN-MOD-TEST.md |
| 9 | Don't infer working-mod percentages from imports alone | **PASS as discipline** — no percentage in cap13 claims execution; the design's own 60-80%/30% figures are labeled estimates over an *envelope*, and §5a explicitly says the census replaces estimates. This audit adds: the only execution claims allowed are the E2E events. | design §5a, §7a; E2E-PROOF.md §7 |

---

## 2. PE loader (`native-loader/src/pe_loader.cpp`, `pe_types.h`, `include/pe_loader.h`) — JH

### 2a. Genericity: PASS
- Zero game/mod names, zero hardcoded offsets. All inputs derived from the
  image: `Machine`, `Magic`, `ImageBase`, section table, data directories.
  [LOCAL-SOURCE: pe_loader.cpp:153-184]
- Refusals are property-based and each carries a reason string:
  - not PE / not PE32+ / not x64 (`Machine != 0x8664`) — pe_loader.cpp:156-166
  - CLR/COM descriptor → "managed images belong to capability #10" — :177-178
  - static TLS directory → names the TEB/gs wall — :179-181
  - 16 KiB RWX partition without J1 authorization — :108-133
  - unknown relocation type, corrupt tables, unresolvable imports (full
    missing list, no null-filled IAT) — :224-252, 255-320
- Policy knobs are explicit, not hidden special cases: `page_size`,
  `allow_rwx_partition` [pe_loader.h:35-42].

### 2b. PE features supported (precise)
- PE32+ (OptionalHeader Magic 0x020B) only, Machine 0x8664 only, DLL or EXE
  images; `NumberOfRvaAndSizes >= 10`.
- Data directories actually *consumed*: EXPORT (0), IMPORT (1), BASERELOC (5);
  TLS (9) and CLR (14) consumed only as refusal predicates.
- Relocations: `DIR64` (10) and `HIGHLOW` (3), `ABSOLUTE` (0) skipped; any
  other type refused.
- Imports: name and ordinal thunks (`PE_IMPORT_BY_ORDINAL`), INT with fallback
  to IAT-only when `OriginalFirstThunk == 0`; resolved against one
  `ImportResolver` (shim registry and/or previously mapped PE modules).
- Exports: name lookup only, exact string match; forwarded exports return
  unresolved (documented in code, pe_loader.cpp:370-371).
- Mapping: anonymous RW region of `SizeOfImage` rounded to `page_size`
  (default 0x4000 — PS5 page size; also macOS arm64 page size), sections
  copied, headers mapped at offset 0, per-16-KiB-page permission union with
  the W+X refusal rule.
- Lifecycle contract: images are never freed/unmapped (retained-module law).

### 2c. What is NOT supported (precise reasons)
| Case | Why it is refused, exactly |
|---|---|
| PE32 (32-bit) / ARM64 / any non-x64 | `Machine != 0x8664` at pe_loader.cpp:161; the PS5 game process is AMD64 and the bridge implements only Win64↔SysV x86_64. |
| Managed (.NET) PEs | CLR data directory present → cap10's domain; the mapper has no ECMA-335 machinery. |
| Static TLS directory | x64 PE TLS access compiles to `gs:[0x58]`/`gs:[0x30]` (TEB). Userspace on PS5 cannot set GS base; the loader cannot fabricate a TEB. Dynamic TLS *APIs* are emulated instead (shim). |
| 16 KiB RWX-requiring page partition | PS5 `mprotect` granularity is 16 KiB while PE sections are 4 KiB; a page whose section-union needs W+X is refused unless the J1 console probe proves RWX is permitted. Empirical notes from the E2E: (a) at 4 KiB granularity (the Mac/Rosetta host) the small samples separate into natural R/RX/RW pages and need no RWX page at all; (b) on PS5's 16 KiB pages these same images would trip the rule — see E2E-PROOF §1 and addendum §A.4.1. The `rwx_partition_required` census field (JG ask #4) turns this into a per-file load-time fact. |
| Unknown base-relocation types | Only DIR64/HIGHLOW are defined for x64; anything else means a format the mapper does not model (or corruption). |
| Any unresolvable import | Fail-closed with the complete `dll!symbol` list; no stubs, no null IAT. |
| Forwarded exports | Point inside the export directory; following them means loading another module — not modeled; reported as unresolved. |
| Bound imports, delay-import directory processing | Bound import data is stale Windows artifact, ignored; delay-load is handled by the mod's own `__delayLoadHelper2` re-entering through shimmed kernel32 (design §3d) — the loader itself never parses dir 13. |

**Audit finding F2 (medium, correctness envelope):** when the base-relocation
directory is *empty* but the slide is nonzero, the loader proceeds silently.
Windows semantics say a relocation-less image can only run at its preferred
`ImageBase`; this loader can never guarantee that. Empirically the small
lld-link samples execute correctly at arbitrary bases because their codegen is
RIP-relative with no absolute pointers in data — but that is a property of
*those* files, not something the loader verified. Recommended (DESIGN addendum
§A.2): refuse, or emit a named load-time diagnostic, when
`slide != 0 && reloc dir empty`, with an explicit override flag; optionally
also honor the COFF `RELOCS_STRIPPED` (0x0001) characteristic.

**Audit finding F19 (low, robustness):** import resolution bounds-checks the
descriptor (`FirstThunk >= SizeOfImage`) but not each thunk write
`image + d.FirstThunk + t*8` against the image end (pe_loader.cpp:314); a
corrupt unterminated thunk array writes out of bounds instead of refusing.
Design-grade robustness item; the analyzer's census catches malformed inputs
before the loader in any real pipeline.

**Audit finding F8 (low):** `find_export` matches names only; ordinal-only
exports (`NumberOfNames < NumberOfFunctions`) are unreachable by name. Fine
for the lifecycle-name contract (`start_mod` etc. are name exports); would
matter for mod→mod ordinal imports.

### 2d. ABI features
None at this layer (format only). No calling-convention knowledge, no
exception handling, no TLS emulation — correctly delegated downward.

### 2e. Bridge signatures
N/A at this layer. The loader hands resolved addresses to the bridge/adapter.

### 2f. C++ object model
None — correctly. C++ compatibility lives in the bridge/compat layers.

### 2g. Imports-resolved vs executed
The loader's contract is explicit: a successful `pe_load` means every import
*has an address* and the image *is executable memory*; it claims nothing about
execution. The Mac E2E is the first measurement of the execution half.

---

## 3. Win64↔SysV bridge (`src/win64_bridge.h`, `src/win64_bridge.S`) — JH

### 3a. Genericity: PASS
Convention-level only; no names, no offsets beyond ABI constants (0x28 shadow
space, register encodings). Works for *any* Win64 guest code and *any* SysV
host body within its declared envelope.

### 3b. Supported (precise)
- host→guest: `cap13_win64_call0..6` — SysV caller → Win64 callee; integer/
  pointer args only; return in RAX; 32 B shadow space built; stack args
  relocated. Win64 non-volatile set is a superset of SysV's, so nothing extra
  to save in this direction [win64_bridge.S:29-107].
- guest→host: stamped 22-byte trampolines (`bridge_stamp_tramp`) +
  5 shared arity thunks (`cap13_g2h_thunk4s0..s4`) covering all-integer
  signatures of 4..8 args; saves/restores RSI, RDI, XMM6-15 (Win64 caller
  expects them preserved; SysV callee may clobber) [win64_bridge.S:125-192].
- Aggregate returns ≤8 B in RAX. MSVC x64 `this`-calls are plain Win64
  (this in RCX) — covered by arg0 remap.

### 3c. NOT supported (precise)
- **Mixed int/float signatures**: Win64 assigns XMM0-3 by *float position
  within a shared 4-slot budget*; SysV interleaves XMM0-7 by position. A
  `f(int, float)` and `f(float, int)` cross differently; the shared thunks
  assume all-integer. **Concretely blocked today**: `CppUserModBase::on_update(float)`
  (the core per-frame virtual of the UE4SS route), `mbm_tick_fn(void*, float)`
  (the #9 host hook family), any Win32 API taking floats (e.g. `GetCursorPos`-class
  is fine, but `VirtualAllocEx`-type... no — precisely: any shim row or virtual
  slot whose prototype mixes GP and FP registers). The fix is the designed M1
  per-signature thunk generator, which **does not exist yet**
  [LOCAL-SOURCE: win64_bridge.h:13-18, win64_bridge.S:194-197].
- **9-16-byte aggregate returns**: Win64 hidden sret in RCX vs SysV
  RAX:RDX — refused until descriptors exist [win64_bridge.h:17-18].
- **>8 args** (>4 register + >4 stack): `bridge_select_thunk` returns nullptr
  → refused.
- **Varargs (guest-side `printf`-class)**: Win64 requires float varargs
  duplicated in the matching GP register; the bridge does not model it. Integer-
  only varargs would incidentally work; mixed would not. Not claimed either way.
- **Float return in XMM0**: SysV and Win64 both return FP in XMM0, so a float-
  returning *all-float-signature* call would work in principle — but there is
  no callN variant that classifies args, so this is out of the declared
  envelope; do not rely on it.
- **Trampoline residency**: g2h trampolines live in executable memory the
  adapter must own (on PS5 = the J1 exec-memory probe; on macOS the E2E maps
  one page). No JIT surface exists in the loader itself (deliberate, cap13 law).

**LOCAL-VERIFY (E2E):** the .S assembled and *executed correctly for the first
time today* under Rosetta x86_64 — both directions, including 7-arg guest→host
crossings (thunk4s3) and the trampoline stamping path. Details and counts in
E2E-PROOF.md §5. The README's "UNVERIFIED until M0" status is now superseded
for the all-integer envelope.

### 3d. Bridge signatures — how they are obtained (requirement 4)
1. **host→guest**: arity family selected by the adapter from the *exported
   lifecycle contract* (`DllMain` = 3 args; `start_mod` = 0; `uninstall_mod` = 1;
   `mbm_mod_entry_v1` = 3). No prototypes are invented per mod.
2. **guest→host**: each shim row carries its documented Win32 prototype in the
   comment/impl signature; the adapter stamps a trampoline whose shared thunk
   arity matches that prototype. There are **no hand-written per-function asm
   thunks and none derived from game/mod identity** — the genericity property
   the user asked for.
3. **Missing piece (honest)**: a *signature table* that mechanically derives
   thunk selection from each row's prototype (including float classification)
   is designed (M1) but not implemented; today arity + "all-integer" is
   asserted by the human author of each row. A wrong arity in a row would
   corrupt that crossing — the E2E validates the 13 rows it used, nothing more.

---

## 4. Win API shim (`src/win_api_shim.h`, `src/win_api_shim.cpp`) — JH

### 4a. Genericity: PASS (policy-generic), with completeness findings
- The resolver is a three-step exact-match on lowercased `(dll, symbol/ordinal)`:
  registered modules (UE4SS surface, mapped PE mods) → built-in rows → nullptr
  = refusal [win_api_shim.cpp:352-388]. No game/mod name anywhere; DLL names
  it knows (`kernel32.dll`, `vcruntime140.dll`, `ntdll.dll`, `UE4SS.dll`) are
  *framework/environment* names — the equivalence classes any Windows-on-X
  shim must provide. That is not per-mod special-casing.
- Tier policy in code matches design §5: MAP (T1-T3), NOOP-LOG, FAIL-CLOSED
  (T4 EH/SEH), and load-time REFUSE is correctly *not* in the shim (it lives in
  the loader/census).
- Dynamic TLS emulated with a slot table (NOT the refused static TLS): :149-178.
- `EncodePointer/DecodePointer` = one process cookie (self-consistent), no TEB.
- Path rules (drive-letter drop, `\`→`/`, `Binaries/Win64` collapse, write
  confinement to mod root) — the GA-shim twin, game-agnostic :270-291.
- Fail-closed EH rows log a precise diagnostic and stop the mod; they resolve
  *presence* without faking *use* — the correct answer to "presence ≠ use" :244-263.

### 4b. Completeness findings against the measured census (NOT genericity violations)
- **F1 (high, false-refusal bug):** the fail-closed row is registered as
  `"CxxThrowException"` but the real import name is `_CxxThrowException`
  [win_api_shim.cpp:340 vs SAMPLE/cap13-feature-probe.dll import table].
  Any mod importing the real name gets a spurious "missing import" refusal.
  One-character fix; proposed in DESIGN addendum §A.1.
- **F14 (medium):** `TlsFree` has no row although `TlsAlloc/Get/SetValue` do —
  the real UE4SS.dll imports all four (census in design §5a), so a TLS-using
  framework-class mod false-refuses on `TlsFree` alone.
- **F3 (high, latent crash):** `impl_CreateThread` passes the guest's start
  routine straight to `pthread_create` [win_api_shim.cpp:188-225]. The comment
  says "the bridge supplies the Win64-calling entry", but nothing in the staged
  tree stamps that trampoline — as written, a mod thread would enter guest
  Win64 code with SysV argument registers. The bridging step is a real
  adapter obligation that exists nowhere yet. Also `*out_tid` is the raw
  pthread_t cast to uint32 (design-grade placeholder).
- **F4 (medium):** `impl_CreateFileW` documents that "the bridge produced the
  UTF-8 transcription" of the guest's UTF-16 — but no such demarshaling exists
  in the staged bridge; the trampoline passes the raw `wchar_t*`. A mod file
  call would hand UTF-16 bytes to a UTF-8 path shimming function. T2 file
  surface is therefore *shape-only* today.
- **F10 (low):** `last_error_slot_value` is a single process-wide slot
  (M1 note in code) — `GetLastError` semantics are per-thread; concurrent mods
  would observe each other's errors.
- **F13 (low):** the design's allocator-pairing rule says to resolve "the
  guest's `free`/`operator delete` import"; for `/MT`-style freestanding mods
  (all KA samples) allocation actually rides `kernel32!HeapAlloc/HeapFree` —
  the pairing resolver must cover the Heap* family too, or shim-allocated
  handles handed to guests (HeapAlloc rows) can never be safely freed.
- **Coverage honesty:** the staged registry has 16 rows (10 T1 + 6 T4). The
  real framework census is 634 imports / 29 DLLs. Nobody should read the staged
  row count as coverage; design §5d's "85-95% of distinct symbols MAP/NOOP"
  remains an estimate until JG's per-mod census tables land (requirement 9).

### 4c. Imports-resolved vs executed (requirement 6)
The shim's rows mean "this symbol resolves"; the E2E counts actual crossings
per row. Example split for the unseen trio: `HeapAlloc/HeapFree/__CxxFrameHandler3/__std_terminate`
resolve on every mbm mod but are **never executed** in the exercised lifecycle;
`log` resolves and *is* executed N times. Full tables in the two proof reports.

---

## 5. UE4SS interface (`src/ue4ss_compat.cpp`) + adapter (`src/pe_adapter.cpp`) — JH

### 5a. Genericity
- `ue4ss_compat` is *deliberately framework-scoped* (its whole job is the
  UE4SS.dll export surface), and even so it is census-driven: "M1 generates
  this table from pe_analyzer's import census" — rows per mod come from that
  mod's own imports, no per-mod code [ue4ss_compat.cpp:80-94].
- `pe_adapter` is fully generic pipeline code; its only fixed string is the
  table name "pe-adapter" [pe_adapter.cpp:110].

### 5b. Honest state
- **Sketch-grade, by its own label** [ue4ss_compat.cpp:3-4]. Boundary-object
  work (MSVC `std::wstring` 32 B layout, base-ctor shim, shim vftable image,
  allocator pairing) is designed with a verified *pinning mechanism*
  (tools/pin_msvc_layout.sh ran on this Mac; design §8 table) but
  `kLayoutPinned = false` — the build refuses to compile until pin output is
  baked [ue4ss_compat.cpp:60].
- **F9 (medium, multi-mod):** `pe_adapter` holds ONE static `PeModState`
  (`g_state`, "one PE mod per adapter module (M1: map)") and a static
  `mbm_mod_v1` table. As staged, two PE mods in one process would collide.
  The #9 host supports multiple mods; the adapter must become a map keyed by
  manifest row before any multi-mod claim.
- **F5-impact (high for the MVP):** the adapter's `cb_on_update` is a
  placeholder returning `MBM_E_UNAVAILABLE` pending the pinned slot table —
  and even with the slot table, `on_update` dispatch is a mixed-signature
  crossing (float delta) that the current bridge refuses. The "first mod works"
  envelope therefore hinges on the M1 float-thunk generator, not just on the
  slot pin. (The mbm-C-table route executed in the E2E dodges this because its
  lifecycle entries are void*/int-only; its tick hook is where the same wall
  appears.)
- Virtual-slot dispatch requires the vendored-header slot indices; MI/virtual-
  base shapes are refused by census (design §4b). `dynamic_cast` across the
  boundary excluded by construction (shim vtables carry no COLs).
- Mod name/version/authors read from the manifest, never by parsing MSVC
  `std::wstring` internals [ue4ss_compat.cpp:39] — genericity-preserving choice.

### 5c. C++ object model compatibility (requirement 5) — designed, zero runs
| Aspect | Staged position |
|---|---|
| Guest vtables (host calls in) | MSVC = bare function-pointer array, vptr at 0, declaration order, dtor pair at declaration position; host dispatch = `win64_callN(slot, obj, …)`. |
| Host-provided vtables | Shim vftable image = array of trampolines; slot order + mangled names pinned via `-target x86_64-pc-windows-msvc` compile dumps (mechanism verified; content not yet baked). |
| Multiple inheritance / virtual bases / covariant returns | Refused at install via secondary-vtable symbol count + RTTI hierarchy descriptors in the census. |
| RTTI / `dynamic_cast` across boundary | Excluded (no COLs on shim vtables). Within the mod's own image, `dynamic_cast` is a data walk and works. |
| STL in boundary | Forbidden except the value-init'd MSVC `std::wstring` fields of the base subobject; everything else is plain C. |
| Object lifetime / allocator pairing | Shim never frees guest heap and vice versa; guest's own free/HeapFree resolved at load and called through the bridge (F13 naming note above). |
| Exceptions | Never cross: EH runtime is fail-closed; a mod throw stops the mod (law 3), never the game. |

**Nothing in this table has been executed.** The E2E proves the layers *below*
the C++ boundary (map/relocate/imports/bridge/plain-C lifecycle). That
separation is exactly requirement 1.

---

## 6. Managed route (`managed-loader/`) — JI, and analyzer (`TOOL/pe_analyzer.py`) — JG

### 6a. Managed route genericity: PASS (conservative by construction)
- Classification keys are *framework-shape* facts (AssemblyRef/TypeRef/
  CustomAttribute/ImplMap from ECMA-335 metadata), never game or mod identity
  [DESIGN/managed-dll-route.md §2b decision table].
- The admitted class is defined by a *capability closure* (mscorlib + host SDK
  + other admitted mods), not by a whitelist of products; everything else is
  refused **with the exact unmet reference list** — the honest-refusal pattern
  the user demanded (requirement 3) [design §7 support matrix].
- Framework names that do appear (BepInEx/MelonLoader/UnityEngine) appear
  solely as *classification and refusal reasons*; the loader never partially
  runs them ("no partial 'it loaded but Awake never fires' states", §2d).
  ModLoader.cs re-checks framework base classes managed-side as defense in
  depth [ModLoader.cs:250-260].
- Lifecycle is reflection-discovered from the mod's own types (§3b) — no
  per-mod registration code.
- **Gap to state (completeness, not genericity):** the entire runtime side
  (ResidentMonoOwner, provider, bridge) is staged source guarded behind
  `#error` walls; zero of it has been compiled or run. The managed route today
  has *design-level* genericity and *zero* execution evidence — the support
  matrix's only "ADMITTED" rows are unproven claims until rungs M1-M3 run.
- **F17 (low, honest labeling):** `HelloMod.cs` is correctly labeled OUR
  fixture everywhere; the open question "does any real mscorlib-only mod
  exist in the wild" is explicit (design §8.3). Requirement 8's unseen-mod
  discipline is therefore *structurally impossible* for the managed route
  until real corpus mods arrive — the native route's E2E/unseen discipline
  does not transfer, and no report should imply it does.

### 6b. pe_analyzer genericity: PASS
- Pure format parsing (PE + ECMA-335), machine-keyed classification, precise
  diagnostics; read-only (byte-identity law) [pe-analysis-design.md].
- It contains two *framework-name classifiers*: the `ue4ss` report block
  (imports of `UE4SS.dll`, `start_mod/stop_mod` exports) and the proxy-DLL
  hint (a fixed list of commonly-proxy'd system DLL names)
  [pe_analyzer.py:743-756]. These are triage hints keyed on the *modding
  framework*, not the loader consuming identity; the loader never reads them.
  Verdict: acceptable, but the audit requires they stay advisory — a future
  change that made the loader *decide* based on `looks_like_proxy_dll` would
  violate requirement 1. (The proxy-DLL hint already self-documents that it
  fires on UE4SS.dll itself because it imports dbghelp.)
- **F6 support (medium):** JH's §10 asks are still open — per-symbol C-vs-`?`
  mangling tags, `.pdata` handler-flag scan, gs-prefix byte scan, 16 KiB
  `rwx_partition_required` fact, secondary-vtable count. The E2E's RWX finding
  (§2c table) makes `rwx_partition_required` the most urgent of the five: it
  converts a console unknown (J1) into a per-file load-time fact.

---

## 7. Consolidated findings list

| ID | Severity | Component | Finding | Proposed action |
|---|---|---|---|---|
| F1 | HIGH (false refusal) | win_api_shim | `_CxxThrowException` registered as `"CxxThrowException"` — real-name mismatch vs every real MSVC DLL's import table | Fix row key (DESIGN §A.1); add to M1 census-generation test |
| F2 | MEDIUM | pe_loader | Empty reloc dir + nonzero slide proceeds silently; Windows semantics say such images are base-locked | Named refusal/diagnostic + override; honor RELOCS_STRIPPED (§A.2) |
| F3 | HIGH (latent crash) | win_api_shim | CreateThread hands raw guest fn to pthread_create; no bridged start routine exists anywhere staged | Adapter must stamp a host→guest trampoline for start routines; until then CreateThread should fail-closed, not half-work |
| F4 | MEDIUM | win_api_shim | CreateFileW assumes UTF-8 but no UTF-16 demarshal exists in the bridge | T2 demarshal step or fail-closed until it exists |
| F5 | HIGH (MVP-blocking) | win64_bridge | Mixed int/float signatures refused; blocks `on_update(float)`/`mbm_tick_fn` — the per-frame paths of BOTH routes | Build the M1 per-signature thunk generator before any "first mod works" claim |
| F6 | MEDIUM | pe_analyzer | Loader-consumed classifier facts (§10 asks) not yet emitted; `rwx_partition_required` most urgent | Add the five fields; E2E shows the RWX rule fires on small real images |
| F7 | LOW | win_api_shim | Ordinal imports against *registered modules* can't resolve (module lookup requires a symbol name) | Ordinal→name resolution via module export tables |
| F8 | LOW | pe_loader | Ordinal-only exports unreachable by `find_export` | Document + ordinal lookup if any mod needs it |
| F9 | MEDIUM | pe_adapter | Single static mod state — multi-mod collision | Map keyed by manifest row |
| F10 | LOW | win_api_shim | Process-wide GetLastError slot (per-thread in Win32) | Per-thread emulated slot (M1 note already in code) |
| F11 | LOW | pe_loader | RVAs into VirtualSize-beyond-raw tails refuse as "corrupt" (precise, but could name the real cause) | Better reason string |
| F13 | LOW | ue4ss_compat | Allocator pairing names free/operator delete; /MT mods use Heap* family | Census-driven pairing resolver |
| F14 | MEDIUM | win_api_shim | `TlsFree` row missing (siblings present; real UE4SS.dll imports it) | Add row |
| F18 | LOW | shim/loader | Delay-load works only if `Hooks.load_pe` is set; default refuses at call time (precise, but should be documented in the support matrix) | Document; wire load_pe in adapter |
| F19 | LOW | pe_loader | Unbounded IAT thunk-write on corrupt unterminated import arrays | Bounds-check like the other tables |
| F20 | MEDIUM (over-broad refusal; false-refusal class) | pe_loader | Static-TLS refusal keys on directory *presence*; the sampled corpus's lld-link-built DLLs all carry **EMPTY** TLS directories (raw start==end==0, callbacks==0 — a linker artifact, no thread-locals, no callbacks), while real UE4SS.dll carries a **REAL** one (0x2d8 bytes + callbacks). Presence-based refusal therefore rejects a whole admissible mod class. | Refine to `tls_reaches_teb = (StartAddressOfRawData != EndAddressOfRawData) || (AddressOfCallBacks != 0)` — refuse only REAL; EMPTY → admit with a named diagnostic (DESIGN addendum §A.3; measured in UNSEEN-MOD-TEST §1/§2) |
| F21 | INFO (Mac-host validation limit) | win64_bridge (test rig) | The anon-page stamped trampoline (`bridge_stamp_tramp`) and the s1..s4 stack-staging g2h classes cannot be validated under this Rosetta build: `movabs r11` from JIT'd anon-page code, and full >4-arg crossings, wedge the Rosetta exception server (measured 3/3). Staged sources are correct by inspection + disassembly; these paths need console validation. | E2E-PROOF §4/§6; addendum §A.4.3-4 |

---

## 8. What this audit does NOT claim

- No PS5/console behavior has been tested or claimed (J1/J2/J3 stand).
- No C++ boundary object, vtable, RTTI or EH behavior has been executed.
- No managed-route code has been compiled or run.
- No percentage of real mods "working" is derivable from any of this
  (requirement 9): the only executable-evidence population today is the set
  enumerated in E2E-PROOF.md/UNSEEN-MOD-TEST.md, of which the *executed*
  subset is listed event-by-event in E2E-PROOF.md §3.

**Post-script (same day, after the E2E ran):** the Mac end-to-end proof
referenced throughout now exists (E2E-PROOF.md): the staged loader, bridge
(s0 class both directions), and entry invoker executed real PE bytes with
98/98 checks passing, plus one documented Rosetta wedge class (F21). The
unseen-mod runs added finding F20 (EMPTY vs REAL TLS directories) — both
reports are companions to this audit and its findings table is the single
source of truth for gaps.

— agent KF (ZCode), 2026-10-13. Mac-only; no console contact; staged sources
read-only (the E2E consumes them unmodified).

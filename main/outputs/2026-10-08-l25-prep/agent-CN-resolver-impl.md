# Agent CN — l26 UFunction-resolution hardening (re-scoped; resolver replacement dropped)

Date: 2026-10-08. Staged in `outputs/2026-10-08-l26-runtime/typescan-r4/`
(consumption law: pin-identical originals under `original/`, edited candidates
under `candidate/`, CHANGES.json rows + hashes synced; port root untouched).

## 0. Scope history (why this is hardening, not a resolver)

Originally tasked to implement Claude's Children-walk UFunction resolver. Two
re-scopes followed, both grounded in completed diagnosis:

1. CA (`agent-CA-ufunc-trace.md`) + CC (`agent-CC-upstream-diff.md`) +
   `evidence/UFUNCTION-NULLPTR-FINDING.md` amendment: the resolution chain is
   byte-identical to upstream UE4SS 3.0.1 (CC diffed UE + UEPseudo@38e7171
   against the port); the observed console error was the null-UObject text
   from a null-wrapper (FindFirstOf returned nullptr at menu-time; the CDO is
   excluded by upstream's own IsValidObjectForFindXOf), and zzdiag-v2's
   CE-108255-1 was the unchecked `construct_uclass` deref on that null
   wrapper. CC's directive: do NOT replace the resolution chain. Coordinator
   re-scoped to hardening items 1-4.
2. CB (`agent-CB-fname-machinery.md`): add fix-2 (FNAME_Add member-name
   construction + Equals index-only compare — the exact primitives FindFirstOf
   proves on console every run; runtime FNAME_Find of asset/BP-registered
   names is the one never-proven primitive on the failing path).

The Children/SuperStruct replacement resolver, the LuaUStruct ForEach*
re-routing, the UnrealType.hpp:3183 null-check restore, and the
CG unified raw-read guarding were all DROPPED with the re-scope (CG's
near-null-iterator theory was superseded by the construct_uclass verdict; CC
explicitly says no TFieldRange changes indicated). They remain documented in
CG's report if the parent wants them later.

## 1. What was implemented (6 edits, 5 staged files)

All Prospero-only behavior changes are `#ifdef __PROSPERO__`-guarded; host /
non-Prospero bodies are byte-identical to upstream. No Lua-visible semantic
change except item 1 (crash → clean catchable error) — the miss branch still
returns the upstream compat wrapper.

| # | Item | Site (candidate) | Edit |
|---|------|------------------|------|
| 1 | Null-harden construct_fname / construct_uclass (zzdiag-v2 crash) | `candidate/src/LuaUObject.cpp` (pin :522-532; CA cited port :484-494) | Null-remote gate before the deref; throws the standard Call-metamethod error text `"Tried calling a member function but the UObject instance is nullptr\n"` (one harvestable string; the Lua traceback names the member, e.g. 'in method GetClass'). |
| 2 | Loud unready-FName-ctor notice (silent NAME_None) | `candidate/include/Unreal/NameTypes.hpp` construct_with_string (pin :239-241) + `candidate/src/NameTypes.cpp` | The silent early return now calls `NotifyFNameCtorUnreadyOnce()` — once per process, `[FN] ctor unready: NAME_None produced` — so a future Seeds/G1 refusal cannot silently degrade to NAME_None. Construction semantics unchanged (still returns NAME_None). |
| 3 | Observable miss branch (was: opaque "attempt to call a userdata value") | `candidate/src/LuaUObject.cpp` handle_unreal_property_value miss branch | Keeps the upstream special_invalid_ptr wrapper (CC-recommended; probing optional members must not throw) but logs `[UF] nofn [n:X] cls=[n:Y]` first — deduped per (class,name), 128-site cap, no ToString/no engine calls. `[n:0]` discriminates the NAME_None conversion failure from a real walk miss. |
| 4 | l22 record_function_lookup hook extended to the MISS branch (CA §4b / Claude hint-1 instrumentation) | `candidate/src/UObject.cpp` GetFunctionByNameInChain Prospero body | Same TFieldRange walk + early-exit; counts UFunctions visited, samples first-3 child FName comparison indices, logs on miss `[UF] miss w=[n:X] c=[n:Y] v=N 1=[a,b,c]` — deduped per (struct,name), 128-site cap. vis=0 => chain/offset/cast problem; many-visited-no-match => comparison problem. |
| 5 | CB fix-2a: FNAME_Add member-name construction | `candidate/include/LuaType/LuaUObject.hpp` prepare_to_handle (pin :821) | Prospero builds construct the member name with FNAME_Add (FindFirstOf-proven; Add's probe is the engine hash lookup and inserts on miss) instead of the miss-fatal FNAME_Find whose NAME_None silently skips the function lookup at the `property_name != FName(0u,0u)` guard. Host keeps FNAME_Find. |
| 6 | CB fix-2b: Equals (index-only) compare | `candidate/src/UObject.cpp` GetFunctionByName + GetFunctionByNameInChain Prospero bodies | `GetNamePrivate().Equals(FunctionName)` (the FindFirstOf comparator, NameTypes.hpp:342-348) instead of operator== (index+Number). Removes the number-form failure class CB ruled out only by disassembly of the currently bound ctor. Plain member names are Number==0 either way. |

Dropped/deferred (documented, not implemented):
- **CB fix-3 (ToString Children-scan fallback)**: routes FName::ToString
  through the Conv_NameToString engine ProcessEvent class never completed on
  console (CI row 8) — a crash risk inside the miss path that outweighs the
  residual index-form risk fix-2 already addresses. Revisit only if the v5
  probe shows misses persisting with healthy Add/Equals primitives, gated and
  bounded per CB.
- **UnrealType.hpp:3183 null-check restore** (CG fold item 2): out of the
  re-scoped list; CC says no TFieldRange changes indicated; note for the
  parent that it is a one-line `!UFieldClass ||` guard if ever wanted.

## 2. Staged rows (CHANGES.json `changes` + `files`; pins asserted identical)

| pin (original) | sha256 | candidate | candidate sha256 |
|---|---|---|---|
| `outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp` | 48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638 | `candidate/src/LuaUObject.cpp` | a44b262b43dbba2edd8915201dc82c64d677e4a7d28382ada5b6b5d6add7ab06 |
| `port/ue4ss/include/LuaType/LuaUObject.hpp` | 89cb7390085bfc12477195a3e3418cfdbe6537a4d31ef1836ba634718b401b82 | `candidate/include/LuaType/LuaUObject.hpp` | cbcb61cccf91c8360e66c33e57d21ed5354ace3c355e528f1d8023d8379b80c9 |
| `port/unreal/src/UObject.cpp` | 93825320514682d59fcd257173782446c3aca10dfdd4e3ab78cbb8fdb64813ec | `candidate/src/UObject.cpp` | ee5607e1bdd43e66fcbd3f3eccc08fe1e992d70809e504f90268a4a3343018c0 |
| `port/unreal/src/NameTypes.cpp` | 685856ef4d54ec00485c857d5e1c120f149510d3b767e1d913af4831b8f232e8 | `candidate/src/NameTypes.cpp` | cc2b5d12ab796688861635063b6053fc1d735477ae1e03218c7326367703d73a |
| `port/unreal/include/Unreal/NameTypes.hpp` | 5fa8d93e7c5d394f92c434e0a6db36d881953c5cce08d1e0e83e59cd22873e71 | `candidate/include/Unreal/NameTypes.hpp` | 3723dbe34353cca5f5f4c2d4f27d4afb61b3b0e857d89cc1f9976c6e1f1a6502 |

Scope text lives in the new `scope_l26_hardening` CHANGES.json key (the l25
worker's `scope` string is untouched). No new OS imports. CL's logwriter rows
(FileDevice.hpp / Output.cpp / PosixFile.cpp / LuaLibrary.cpp) landed in the
same CHANGES.json in parallel — zero file overlap with mine; I did not touch
any print/writer code or LuaLibrary.cpp.

## 3. Marker strings (wide-char, all < 60 chars)

- `[FN] ctor unready: NAME_None produced` — once per process, ctor-unready
  degradation notice (NameTypes.cpp).
- `[UF] nofn [n:{name}] cls=[n:{class}]` — member/function miss at the Lua
  call site; `[n:0]` = NAME_None conversion failure (LuaUObject.cpp).
- `[UF] miss w=[n:{want}] c=[n:{cls}] v={visited} 1=[{a},{b},{c}]` — walk
  miss with visited count + first-3 child indices (UObject.cpp).
- Error text (not a marker): the existing standard
  `Tried calling a member function but the UObject instance is nullptr`
  reused for the null-wrapper guards (construct_fname/construct_uclass).

## 4. Verification

Compile (work dir `/tmp/l26-harden-smoke`, recipe per Z13; logs + summary in
`results/L26-HARDEN-COMPILE.json`):

- Host clang++ (arm64), receipt argv + `-D__PROSPERO__ -D__SCE__
  -fshort-wchar` + Z13 stubs, candidate headers first: **3/3 exit 0**
  (lua-luauobject, unreal-uobject, unreal-nametypes).
- Real prospero-clang++ fidelity pass (receipt argv verbatim; only deviation:
  `/tmp/l26-harden-smoke/headers-l26.json` overlay = l25-A overlay with
  LuaUObject.hpp repointed to my candidate + NameTypes.hpp mapping added):
  **3/3 exit 0**.
- Object inspection: `[UF] nofn` in lua-luauobject.o, `[UF] miss` in
  unreal-uobject.o, `[FN] ctor` in unreal-nametypes.o (UTF-16 probes);
  `RC::Unreal::NotifyFNameCtorUnreadyOnce` defined external in
  unreal-nametypes.o.

Behavioral host fixture `tests/actual_ufunc_harden.cpp` (actual edited bodies
extracted with mocks; `results/HOST-TESTS-L26-HARDENING.json`): plain +
fatal ASan/UBSan both PASS —
- item 1: null wrappers throw the exact standard text before any deref
  (UBSan proves no null member call); valid wrappers construct normally;
- item 2: five unready constructions -> exactly ONE notice, NAME_None still
  produced (semantics unchanged); zero further notices after the ctor binds;
- item 4: 5-function chain miss logs `v=5 1=[A1,B2,C3]`, deduped on
  (struct,name), new name logs again, empty chain logs `v=0 1=[0,0,0]`;
  line lengths < 60 asserted;
- item 3: `[UF] nofn` dedupe + null-base tolerance + `[n:0]` discriminator;
- fix-2a: Prospero member-name construction recorded as FNAME_Add;
- fix-2b: stored {id=0x2A,num=2} vs wanted {id=0x2A,num=0} — operator== fails,
  Equals resolves; the instrumented walk resolves it (hit at visited=3, no
  miss logged).

## 5. Parent obligations / residuals

1. **Header routing (E3 lesson)**: both header replacements
   (`Unreal/NameTypes.hpp`, `LuaType/LuaUObject.hpp`) must reach ALL consumer
   TUs via the build's vfs overlay (`-I` prefix is not enough). The l26
   overlay used for my fidelity compile (`/tmp/l26-harden-smoke/headers-l26.json`)
   is the exact recipe: l25-A overlay + the two mappings.
2. **LuaUObject.cpp edition**: I pinned the menu-complete-build selected unit
   (48aa5605..., per CI's "SELECTED LuaUObject for the l24→l26 composition");
   the l25-A console build actually compiled the l22-diagnostics edition.
   Confirm which edition l26 composes — the edits are line-portable to either
   (same functions/text in both; only logging wrappers differ).
3. **Full-tree rebuild + packaging + console proof** remain the parent's, as
   always in this lineage. No console contact was made.
4. Layering per coordinator: tonight's observed errors are layer-1 (FindFirstOf
   itself returning null at menu timing — walk audit CO / v5 probe
   discriminates); this stage is layer-2 hardening so the moment objects
   resolve, the function path rides Add/Equals primitives that are already
   console-proven, with loud, bounded diagnostics on any residual miss.

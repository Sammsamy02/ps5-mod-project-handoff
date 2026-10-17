# APPLY ORDER + LAUNCH-2 PROBE EXPECTATIONS — 02b addendum (agent IK, 2026-10-11)

Companion note to the world-fix card. IE's manifest is NOT modified —
this is the reference for folding 02b into the labelled build and for
reading the launch-2 probe additions. Analysis basis: IF's
UEHELPERS-GETWORLD-AUDIT.md (§1.2 blocker, §4 test plan), IA's
01-diagnosis.md + 02/03 patches, round-4 decoded log.

## 1. Apply order (supersedes the 2-patch order; now THREE staged patches)

Apply to the frozen round-4 candidate tree
(outputs/2026-10-08-l26-runtime/typescan-r4/candidate/), in order:

1. `02-core-fix.patch`  (IA — LuaUObject.hpp GetName native binding;
   touches the header's post-GetFullName area + nothing else)
2. `02b-getworld-fallback.patch`  (IK — LuaUObject.hpp GetWorld member
   __PROSPERO__ fallback + one explicit `#include <Unreal/TypeChecker.hpp>`;
   touches the same header ~95 lines lower; SEPARATE FILE so the
   labelled build applies both cleanly — verified: `patch -p1` applies
   02 then 02b with zero fuzz, round-trips byte-identical to the
   staged edit)
3. `03-map-logger.patch`  (IA — LuaMod.cpp [FL-MAP] logger; disjoint file)

02/02b/03 touch disjoint regions (02: GetName block; 02b: GetWorld
block + one include line; 03: LuaMod.cpp) — any relative order works;
the 02→02b order is what was syntax-receipted.

## 2. What 02b changes (behavior contract for the build + reviewers)

- Host builds (#else, no __PROSPERO__): the GetWorld member is
  byte-identical to upstream.
- PS5 (__PROSPERO__): when `Unreal::UObject::VTableLayoutMap` HAS a
  GetWorld entry (future audited layout), the native virtual call
  runs exactly as before. When the entry is ABSENT (today's staged
  pair), the member no longer throws "Virtual UObject::GetWorld is
  unavailable"; it returns the outer-chain walk result instead —
  EXACT AActor::GetWorld semantics (unreal/src/AActor.cpp:38-52):
  first OuterPrivate behind the RF_ClassDefaultObject /
  RF_BeginDestroyed / GetObjectItem()->IsUnreachable() gate, then
  walk OuterPrivate until the outer's class FName ==
  TypeChecker::get_world_name(); return that outer as UWorld*, else
  nullptr (auto_construct_object(nullptr) = the same nil wrapper as
  the nullptr-object case today). Heals UEHelpers.GetWorld +
  GetWorldContextObject + the 6 transitively blocked UEHelpers
  exports + MiniMap's direct pc:GetWorld() (IF census row 7-14).
- Syntax receipts: `syntax-receipt-LuaUObject-02b.log` (02+02b
  composed header, prospero-clang++ -fsyntax-only on
  candidate/src/LuaUObject.cpp: EXIT=0, 0 errors, 26 warnings — same
  warning classes as IA's header receipt) and
  `syntax-receipt-LuaUObject-02b-only.log` (02b alone: EXIT=0).
  Env note: prospero-clang++ needs LLVM_CONFIG=<LLVM-21.1.8-macOS-ARM64
  dist>/bin/llvm-config on this Mac (prospero-llvm-config --bindir
  resolution); recorded in the receipt headers.

## 3. Launch-2 probe additions + expected readings (IE expectation reference)

Lua deploy artifacts (edited in place, no manifest change needed
beyond the size-exact loadlist rows):

- zzdiag-v11/main.lua — NOW 3919 bytes (was 4086; loadlist row must
  record ACTUAL 3919). Two changes:
  - mn() names the World via w:GetFullName() (was GetName — the
    round-4 nofn surface). Expected `[zz] [MAP] <full-name-48> @<n*6>s`
    once per map identity change; "?" only if FindFirstOf("World")
    misses/invalid. Full name shape "/Game/Maps/<Pkg>.<WorldName>".
  - NEW P8 after P7 in ids(): `[PRB-8] gw ok=<1|0> val=<40>` (+ the
    standard indented el() tail on throw) from ONE pcall'd
    require("UEHelpers").GetWorld() — the spawner's require shape.
    SKIP ladder widened to PRB-1..8 / PRB-2..8.
- ueselftest — M now 168 (part11 entry 166 'UEHelpers.GetWorld',
  pcall'd, SKIP-on-missing: no-uehelpers / no-getworld / nil-world /
  invalid-world; FAIL only on a real throw). main.lua still 3987 B,
  part11.lua 2712 B. validate.lua stages a HOST-ONLY UEHelpers stub
  (never deployed).

Reading grid (frozen round-4 binary, no patches — the PREDICTION):
  [PRB-8] gw ok=0, tail "Virtual UObject::GetWorld is unavailable" =
  IF §1.2 confirmed. 166/168 UEHelpers.GetWorld FAIL same tail.
  [MAP] still prints via GetFullName (native member, no vtable hop).

Reading grid (labelled build with 02+02b+03 applied):
  [PRB-8] gw ok=1 val=<world wrapper, 40> and ueselftest 166/168
  PASS "ok <world>" — in-world (PC arm). Menu-phase BEFORE save load:
  ok=1 with UEHelpers' invalid-object shape (both PC arms + GameInstance
  missed) — SKIP invalid-world in ueselftest; NOT a failure.
  [MAP] full name + [FL-MAP] map= agree (different sources: zzdiag
  6s cycle vs runtime 1 Hz watch / LoadMap detour).
  Divergence [PRB-8] ok vs [MAP] name → read the el() tail; a walk
  that reaches no World outer (mis-rooted object) returns nil wrapper
  → ok=1 invalid shape — that is the fallback's honest miss, distinct
  from the old throw.

Honest limits: the 02b fallback's liveness gate mirrors AActor.cpp's
(CDO/BeginDestroyed/Unreachable on the FIRST outer only, per that
code); objects whose outer chain never names a World (e.g. some
subobjects) return nil where the engine virtual might return a World
— acceptable for the UEHelpers/MiniMap consumers, which pcall-guard.
The native arm stays preferred whenever the layout gains the entry.

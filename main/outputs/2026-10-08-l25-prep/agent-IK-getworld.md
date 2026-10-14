# agent-IK — GetWorld fallback (02b) + launch-2 Lua probes

2026-10-11. Battery rules honored: staged patches + syntax checks +
Lua edits only; NO builds, NO console contact. All writes under
outputs/2026-10-08-l25-prep/ (staging + deploy artifacts + this
report); no live-tree edits.

## 1. 02b-getworld-fallback.patch — the native fallback (core deliverable)

File: `round5-staging/world-fix/02b-getworld-fallback.patch` (3,233 B,
unified diff vs the frozen round-4 candidate header
typescan-r4/candidate/include/LuaType/LuaUObject.hpp, sha256
560eaf6d… at both ends verified). Two hunks:

1. Include block: one explicit `#include <Unreal/TypeChecker.hpp>`
   (get_world_name for the walk; was only transitively included).
2. The `GetWorld` Lua member (:648-653): under `#ifdef __PROSPERO__`,
   when `Unreal::UObject::VTableLayoutMap` contains a GetWorld entry
   the native virtual call runs exactly as before (future audited
   layout preferred); when ABSENT (today's staged pair —
   compiled-audited-townfall-l25 stages no [UObject] GetWorld, IF
   §1.2) it no longer throws "Virtual UObject::GetWorld is
   unavailable" but returns the outer-chain walk: EXACT
   AActor::GetWorld semantics per `unreal/src/AActor.cpp:38-52` —
   first OuterPrivate behind the same RF_ClassDefaultObject /
   RF_BeginDestroyed / GetObjectItem()->IsUnreachable() gate, then
   walk OuterPrivate until the outer's class FName ==
   TypeChecker::get_world_name(), return it as UWorld*, else nullptr
   (auto_construct_object(nullptr) = today's nil-wrapper contract).
   `#else` (host): upstream member byte-identical. Heals UEHelpers
   rows 7-14 of IF's census (GetWorld/GetPersistentLevel/
   GetGameModeBase/GetGameStateBase/GetWorldSettings/
   GetWorldContextObject/GetAllPlayerStates/GetAllPlayers) +
   MiniMap's direct pc:GetWorld() (:526), which needs no UEHelpers.

COORDINATION (per task): separate 02b file; disjoint regions from
IA's 02 (GetName block ~:553) and 03 (LuaMod.cpp). Verified:
`patch -p1` applies 02 then 02b with zero fuzz and round-trips
byte-identical; also applies standalone.

Syntax receipts (established recipe — IA's exact prospero-clang++
-fsyntax-only command over candidate/src/LuaUObject.cpp, staged
include dir first):
- `syntax-receipt-LuaUObject-02b.log` — header with 02+02b composed:
  EXIT=0, 0 errors, 26 warnings (same classes as IA's receipt:
  -Wswitch, deprecated enum/conv), include trace proves the staged
  header compiled.
- `syntax-receipt-LuaUObject-02b-only.log` — 02b alone: EXIT=0.
- Env note (recorded in receipt headers): prospero-clang++ on this
  Mac needs `LLVM_CONFIG=<ps5-mod-bridge native-probe deps>/
  LLVM-21.1.8-macOS-ARM64/bin/llvm-config` for
  prospero-llvm-config --bindir to resolve.

## 2. zzdiag-v11 (deploy artifact, edited in place)

`zzdiag-v11/main.lua` — NOW 3,919 B (was 4,086; cap 4,096; size-exact
loadlist row must record 3,919; Z0 string unchanged "Z0 v11").
- mn() World naming switched to `w:GetFullName()` (was GetName — the
  round-4 `[UF] nofn w=[n:29D61(GetName)] c=[n:1E6C3(World)] v=4`
  surface; GetFullName is a native metatable member, IF §4.2).
- NEW P8 after P7 in ids(): `[PRB-8] gw ok=<1|0> val=<40>` + the
  standard indented el() tail on throw, from ONE pcall'd
  `require("UEHelpers").GetWorld()` — the spawner's lazy-require
  shape (console copy :15-16). SKIP ladder widened to 1..8 / 2..8.
- Whitespace/comment compression funded the +148 B (IC precedent;
  logic + every marker string byte-identical; one numeric-literal
  fix `(n*6)` required by Lua lexing after space removal).
- Validation: luac 5.5 -p clean; the preserved FJ stub harness runs
  all 8 scenario dumps + WD clean (EXIT=0) — P8 lines all read
  ok=0 with the fenced require-missing tail on host (expected: no
  UEHelpers module there); P0..P7 verdict shapes unchanged.
  Receipt: `round5-staging/world-fix/lua-receipt-zzdiag-harness.log`.
- `zzdiag-v11/EXPECTED-MARKERS.txt`: IK addendum (new size, naming
  change, P8 reading grid incl. the frozen-build prediction
  "ok=0 + tail 'Virtual UObject::GetWorld is unavailable'").

## 3. ueselftest — the free call slot DID fit cleanly

- `ueselftest/part11.lua` (2,712 B < 4,096): new entry 5
  'UEHelpers.GetWorld' between io-roundtrip and hygiene-audit —
  pcall(require,"UEHelpers") + pcall'd GetWorld with SKIP-on-missing
  semantics (no-uehelpers / no-getworld / nil-world /
  invalid-world = UEHelpers' menu-phase CreateInvalidObject shape);
  FAIL only on a real throw (the informative vtable tail on the
  frozen build); success sets S.hyd.gw (hygiene-audit now all-6).
- `ueselftest/main.lua` (3,987 B unchanged): M 167→168, LP(11,5)→6.
  Entry index 166; skip-class-list now 168/168. Resume indices ≥166
  shift by one (only matters across deploys mid-crash — noted).
- `ueselftest/validate.lua`: stages a HOST-ONLY UEHelpers.lua stub
  under STAGE (GetWorld → FindFirstOf("World"); never deployed) so
  the probe runs the same require/pcall path on host.
- Validation re-run: S1 SUMMARY 168 of 168 PASS, zero numbered
  FAIL/SKIP, second-run same-M, UEHelpers.lua staged (verified in
  the stage dir); S2 resume arithmetic M-5 holds; S3 holds (M-2-1,
  exactly 2 os.* FAILs + 1 io SKIP). ONE validator check fails —
  "S1 io-roundtrip host shape (w-only open-r-miss)" — PRE-EXISTING:
  reproduced identically on a fully reverted baseline copy (M=167,
  no stub, receipt lua-receipt-ueselftest-validate-baseline.log);
  this Mac's real host io now round-trips ("PASS rw roundtrip
  rm=true"), i.e. environment drift vs the GA-shim era, and the
  mod-side operator check line itself expects 'PASS rw roundtrip'.
  Not caused by, and not masking, my change; documented, not
  "fixed" (GB/IE harness ownership).
- `ueselftest/EXPECTED-MARKERS.txt`: all M references updated to
  168 + IK addendum with the console reading grid and the
  pre-existing-check note.

## 4. World-fix card note (IE reference; IE's manifest untouched)

`round5-staging/world-fix/APPLY-ORDER-AND-PROBE-EXPECTATIONS.md`:
apply order now 02 → 02b → 03 (any order works — disjoint regions;
02→02b is what was receipted), 02b behavior contract, launch-2
probe-reading grid for frozen vs labelled builds, and honest limits
(walk gate mirrors AActor.cpp; chains that never name a World return
nil wrapper — the fallback's honest miss, distinct from the old
throw; native arm preferred once the layout gains the entry).

## Files touched (all under outputs/2026-10-08-l25-prep/)

Created: round5-staging/world-fix/{02b-getworld-fallback.patch,
syntax-receipt-LuaUObject-02b.log, syntax-receipt-LuaUObject-02b-only.log,
lua-receipt-zzdiag-harness.log, lua-receipt-ueselftest-validate.log,
lua-receipt-ueselftest-validate-baseline.log,
APPLY-ORDER-AND-PROBE-EXPECTATIONS.md}, this report.
Edited: zzdiag-v11/{main.lua,EXPECTED-MARKERS.txt},
ueselftest/{main.lua,part11.lua,validate.lua,EXPECTED-MARKERS.txt}.
Live tree: untouched. Builds/console: none.

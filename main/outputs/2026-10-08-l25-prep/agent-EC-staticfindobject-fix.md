# agent-EC — StaticFindObject long-name path machinery fix (l26 layer-3)

2026-10-08. READ-ONLY analysis of the port + fix implemented and staged in
`outputs/2026-10-08-l26-runtime/typescan-r4/`. No console contact, no uploads,
no engine writes. Evidence: `/tmp/v7-run.log` (zzdiag v7, 2026-10-08 22:24).

## 1. Full trace, hop by hop (built lineage, not just the port root)

The running payload is the l25/l26 lineage (log carries `[l23] game-thread
self-test`, `[R3]`, `[L25][DeleteListener]` markers). `BUILD-INPUTS.json` +
`l26-build-variants/l26-A/pass1/objects/unreal-uobjectglobals.command.json`
pin the compiled unit to
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp`
(pre-fix candidate sha 4abf743b...). Line numbers below are that file
(pre-fix -> post-fix where relevant); port-root equivalents in parentheses.

| Hop | Site |
|---|---|
| Lua script literal | `zzdiag-v7/main.lua:29-31` — `StaticFindObject("/Script/Engine.PlayerController")` (overload #1, string only); U2 = UEHelpers `GetKismetStringLibrary()` -> `CacheDefaultObject("/Script/Engine.Default__KismetStringLibrary")` |
| Lua binding | `candidate/src/LuaMod.cpp:1627-1653` (port LuaMod.cpp:1529-1556): overload #1 `if (lua.is_string())` -> `UObjectGlobals::StaticFindObject(nullptr, nullptr, ensure_str(lua.get_string()))` |
| String conversion | `lua.get_string()` -> `std::string_view` (narrow, port first/src/LuaMadeSimple/LuaMadeSimple.cpp:755-760, index-1 clean); `ensure_str` -> `to_wstring` -> `Unicode::DecodeUtf8` element copy (port first/include/Helpers/String.hpp:188-197, 205-229; Utf16Utf8.hpp:59-90) — verified correct for ASCII incl. '.' and '/' |
| Template dispatch | `include/Unreal/UObjectGlobals.hpp:235` (`const StringType&` overload) -> `.c_str()` -> `FindObject(Class, InOuter, const TCHAR*, ...)` |
| FindObject entry | candidate:318 (was 278): global-object cache `GetGlobalObject` (ClassListener.cpp:144, plain map — no throw) |
| Hash fast path | candidate:336-354 (was 296-314): `ShouldUseHashTables()` gate -> `FUObjectHashTables::StaticFindObjectByPath`; miss falls through |
| **THROW SITE** | candidate:356-370 (was 316-324): the LOCAL lambda `GetPackageNameFromLongName`, `LongName.find(STR(".")) == npos` -> `throw std::runtime_error{"GetPackageNameFromLongName: Name wasn't long."}` at pre-fix candidate:321 (port :274); invoked from pre-fix :331 `FName(GetPackageNameFromLongName(ToCharTypePtr(InName)), FNAME_Add)` |
| (the walk that never ran) | `StaticFindObject_InternalNoToStringFromStrings` candidate:257-265 -> `...FromNames` :208-255 — the console-safe FNAME_Find outer-chain matcher, but `FindObject`'s long-name arm does NOT use it |

Uniqueness: `grep` over the entire built source set (all lineage trees +
retained pins) finds exactly ONE producer of that message — this lambda
(binary-rodata confirmation: l21-audit `inventory/bare-strings.txt` entry
`126bf93 GetPackageNameFromLongName: Name wasn't long.`, single occurrence).
The v7 24-char tail `GetPackageNameFromLongNa` = `el()`'s `e:sub(1,24)` of
this exact string; S1e/S2e/S3e/U2e are all this one throw.

## 2. Why it fails on console

- `GetPackageNameFromLongName` is NOT an engine-bound function on console —
  it is an in-port lambda. Nothing is "unbound"; the call THREW.
- The throw fires iff the name handed to `FindObject` contains '/' (so
  `FPackageName::IsShortPackageName` says long — plain '/' scan,
  PackageName.cpp:13-23, compiled verbatim from the port) but contains no
  '.'. I.e. the name at the lambda was a PACKAGE-shaped path
  ("/Script/Engine") or a corrupted dot-less string — not the dotted literal
  the script passed. Every statically readable hop between Lua and the
  lambda was verified sound for dotted ASCII literals (binding, string_view
  lifetime, narrow->wide conversion, template dispatch, ToCharTypePtr size
  identity — PS5 short-wchar makes wchar_t/char16_t/UTF-16 bits identical),
  so the dot-loss originates in the non-auditable seam: the RETAINED
  prebuilt bridge objects (e.g. `lms-luamadesimple.o` retained from
  coherent-full-1) which predate the short-wchar conversion fixes, and/or a
  package-part re-entry from a helper. This cannot be pinned further
  without a console-side probe; what is pinned is the trigger form.
- Independent of the dot question, the old long-name arm could never resolve
  on this console even for well-formed names: it builds `PackageName`/
  `ShortName` with **FNAME_Add through the bound engine ctor** — the GATE-1
  numbered-duplicate behavior (documented in
  tests/actual_findxof_fname_fix.cpp header: Add of an interned name yields
  a fresh index != stored, so index-only `Equals` can never match) — and
  gates the final compare on `Object->GetFullName()` (pre-fix :497), i.e.
  `FName::ToString` through the KismetStringLibrary CDO, which itself needs
  this very lookup (chicken-and-egg; NameTypes.cpp:165 throws "FName::ToString
  was not ready"). Two engine-dependent mechanisms, both console-broken.

## 3. The fix (staged per the consumption law)

All in `outputs/2026-10-08-l26-runtime/typescan-r4/`:

1. **`candidate/src/UObjectGlobals.cpp`** (row already in CHANGES.json,
   refreshed `candidate_sha256` = `83e20b3804114b9320d0f29ad02e33b02585bd0f0e914734d2378ac97c27a9fe`;
   frozen `original/` untouched, hashes verified):
   - **:273-321 new `SplitLongNameIntoPathParts`** — in-port split, no
     engine calls. Package part KEEPS its slashes (`"/Script/Engine"` is one
     FName — the exact part shape UnrealInitializer.cpp:2022/2042 already
     feeds `StaticFindObject_InternalNoToStringFromStrings`); object/
     subobject tail split on '.' and ':':
     `/Script/Engine.PlayerController` -> `{"/Script/Engine","PlayerController"}`;
     `/Script/Engine` -> `{"/Script/Engine"}` (no dot, NO THROW);
     `/Game/Maps/L1.L_Persistent:Prop` -> 3 parts.
   - **:356-370 de-fanged `GetPackageNameFromLongName`** — dot-less long
     names degrade to an empty package filter instead of throwing.
   - **:377-425 console-safe long-name resolution arm** (`bIsLongName &&
     !InSearcher`): every part constructed **FNAME_Find-only** (live pool
     readable per GATE-1 pin notes); **loud refusal**
     `"[StaticFindObject] long-name part 'X' not in FName pool (FNAME_Find miss); refusing to scan"`
     when a part is absent (names the exact part — the diagnostic the S-probes
     needed; literal "None" allowed through); resolution via the EXISTING
     `StaticFindObject_InternalNoToStringFromNames` outer-chain walk; hit ->
     exact-class gate + `CacheGlobalObject` + return; clean miss falls
     through to the scan, which can no longer throw. Hash-tables fast path
     and searcher-driven callers untouched; no header changes; no new engine
     calls anywhere in the arm.
2. **`CHANGES.json`** — row hash refresh + new
   `scope_l26_layer3_staticfindobject_path` (full rationale + receipts).
3. **`tests/actual_staticfindobject_path_fix.cpp`** — CD/CS-pattern fixture
   (house style of `actual_findxof_fname_fix.cpp`): console-mock FName pool
   (Find stored/silent-{0,0}; Add-of-interned -> numbered dup), mock object
   graph (package/class/CDO/function), extracts the ACTUAL production
   bodies. RED: original lambda throws on "/Script/Engine" (the exact v7
   S*e reproduction, message asserted); original FNAME_Add PackageName gets
   a dup index that never Equals the stored package. GREEN: dotted path,
   CDO path, 3-part chain, package-only path resolve; bogus part and
   dot-less garbage refuse loudly naming the part; clean miss returns
   nullptr.
4. **Receipts** in `results/`: `L26-STATICFINDOBJECT-PATH-HOST-COMPILE.json`
   (candidate + pre-fix control), `L26-STATICFINDOBJECT-PATH-PROSPERO-
   COMPILE.json`, `HOST-TESTS-L26-STATICFINDOBJECT-PATH.json`.

## 4. Verification results

| Check | Result |
|---|---|
| Host compile, candidate (findxof command shape: host clang++, `-D__PROSPERO__ -fshort-wchar`, vfs overlay + stubs) | rc=0, 3 warnings — all pre-existing header-level (Atomic.hpp x2, UnrealType.hpp) |
| Host control compile, frozen pre-fix original, identical command | rc=0, same 3 warnings (edit is warning-neutral) |
| Prospero cross-compile, candidate (findxof argv + LLVM_CONFIG) | rc=0, pre-existing header warnings only |
| Fixture plain | `staticfindobject-path-fix: ALL PASS`, exit 0 |
| Fixture asan+ubsan (fatal) | ALL PASS, exit 0 |

Not done (needs the normal parent pipeline): full-build composition, target
link, packaging, console run. The residual unknown — WHERE the dot is lost
between Lua and the lambda on console — is now irrelevant to resolution (the
new arm never needs the dot form) and any future dot-less/corrupt name
produces a loud, part-naming refusal instead of the cryptic throw.

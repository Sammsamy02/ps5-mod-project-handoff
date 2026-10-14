# Agent EK — FName→string consumer census: blast radius of the ToStringInternal bind (l27 pre-ship audit)

Date: 2026-10-08. READ-ONLY audit; the only file written is this report.
Scope: every consumer of the port's FName→string path in the port root
(`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`)
plus the staged l26/l27 candidates under
`outputs/2026-10-08-l26-runtime/typescan-r4/` (DA verified-query in
`candidate/src/UObjectGlobals.cpp`, CN hardening in `candidate/src/{LuaUObject,UObject,NameTypes}.cpp`,
EI's bind in `candidate/src/NameTypes.cpp` + `candidate/src/UnrealInitializer.cpp` Phase 4c
+ `include/EngineToStringProfile.hpp`, guard in `include/NativePropertyPointerGuard.inl`).

Bind under audit (already staged, [Seeds][L27] agent-EI, Claude-reviewed):
`FName::ToStringNative = Function<uint32_t(const FName*, CharType*, uint32_t)>`
bound to the engine's own `FName::ToStringInternal` @ image 0x23FA00 /
runtime 0x63FA00 (sha-pinned eboot 3b6cfad4…), admitted in SeedArming Phase
4c behind a 6-gate check (CodeCallOnly class, budget, live ByteProperty-id
pin 0x3, 32-byte prologue, behavioral probe == "ByteProperty", pool-decoder
dual-check). Any refusal → `assign_address` never runs → the legacy routes
keep FName::ToString, loudly (`[FN] tostring-bind disabled (mismatch/empty)`).

## 0. The chain (one paragraph)

Every FName→string conversion in this codebase terminates in
`ToStringInternalWrapper` (port `unreal/src/NameTypes.cpp:127-142`; staged
`candidate/src/NameTypes.cpp:210-239`):
`FName::ToString()` (const + non-const, :171-179 port / :268-276 staged) →
wrapper → (staged, Prospero) `ToStringNative.is_ready()` →
`ToStringInternalWrapper_UsingEngine` (1035-TCHAR stack buffer, anomaly
guard) → else Scan route (`ToStringInternal` FStringOut form, AOB/Lua bound
on non-seed targets) → else Kismet `Conv_NameToString` ProcessEvent → else
`throw runtime_error("FName::ToString was not ready but was called anyway")`.
`ToFString/ToFUtf8String/ToFAnsiString` (NameTypes.cpp:182-198 port /
:279-295 staged) call `ToString()`. `operator==(const CharType*)`
(NameTypes.hpp:327-330) calls `ToString()`. `UObject::GetName()`
(UObject.hpp:237-239) = `GetNamePrivate().ToString()`; `GetPathName`
appends `GetName()` per outer (UObject.cpp:537-562); `GetFullName` =
class `GetName()` + `GetPathName` (UObject.cpp:580-590); `FField::GetFullName`
does the same via `GetClass().GetName()` (FField.cpp:143-150). **There is no
other FName→string route**: grep over port + staged trees shows zero direct
callers of `FName::ToStringInternal`, `FName::ToStringNative`,
`FName::Conv_NameToStringInternal`, or
`UKismetStringLibrary_Conv_NameToString` outside NameTypes.cpp (the wrapper
itself), the bind sites (UnrealInitializer.cpp:787 port / Phase 4c staged;
Signatures.cpp:209), and CN's log-once notice.

Current in-world behavior (pre-bind, seeds target): the Scan route is
force-disabled (`config.fname_tostring_fstring = false`, staged
UnrealInitializer.cpp:1361-1368, "[Seeds] Unbound by design: fname_tostring
(Kismet fallback owns it)"), so production ToString = Kismet
`Conv_NameToString` ProcessEvent — which Phase 4c's comment records as
"returned EMPTY in-world (markers fired, indices valid, round-trip compare
vs empty → nothing found)". Empty output is then masked to the sentinel
`"UE4SS_None"` by both wrappers (NameTypes.cpp:115/123 port, :142/150
staged). If neither route is armed (pre-PostInitialize), ToString throws.

## 1. Consumer table

Legend: **direct** = calls FName::ToString/ToFString (or FName-typed
`.ToString()`) itself; **indirect** = formats object names through
UObject::GetName/GetFullName/GetPathName (all terminate in the same
wrapper). "Empty today" = observable behavior given the empty/"UE4SS_None"
ToString; "With bind" = expected behavior once ToStringNative is admitted.

### 1a. Core API (NameTypes)

| Site | Purpose | Empty today | With bind |
|---|---|---|---|
| NameTypes.cpp:171-179 (staged :268-276) `FName::ToString` ×2 | The choke point | "UE4SS_None"/empty or throw | real name, zero-alloc |
| NameTypes.cpp:182-198 `ToFString/ToFUtf8String/ToFAnsiString` | FString/FUtf8/FAnsi conversions (FSoftObjectPath, TopLevelAssetPath, Lua marshaling) | empty FString / throw | real strings |
| NameTypes.hpp:327-330 `operator==(const CharType*)` | FName vs literal compare (Signatures.cpp ctor confirmation :262+ uses `name == STR("ByteProperty")` — non-seed targets only; under seeds that confirmation is skipped) | always false (or throw) | true iff literal matches |
| NameTypes.cpp:200-206 (staged :297-303) `GetPlainNameString` | C-buffer name export | copies "UE4SS_None" | **see §2h — latent overflow, dead code** |

### 1b. UObject name formatting (indirect, via UObject::GetName)

| Site | Purpose | Empty today | With bind |
|---|---|---|---|
| UObject.hpp:237-239 `GetName` | every object name | "UE4SS_None" | real name |
| UObject.cpp:537-562 `GetPathName` | dotted object paths | paths of "UE4SS_None" | real paths |
| UObject.cpp:580-590 `GetFullName` | "Class Path/To.Obj" | garbage full names | real full names |
| FField.cpp:143-160 `FField::GetFullName/GetPathName` | property full names | garbage | real |

Indirect call-site breadth (port root, live code): **118 `GetName()` uses
across 11 files** (LuaUObject, LuaMod, SDKGenerator/Generator+Common+
UEHeaderGenerator, GUI/Dumpers+DataDumpers, UObject, UEnum, FField,
TypeChecker, UnrealInitializer-adjacent) and `GetFullName()` in 19 files —
notable behavioral consumers:
- `unreal/src/UObjectGlobals.cpp:144-149` `StaticFindObject_InternalSlow`
  (ForEachUObject + `GetFullName` compare): **finds nothing today** (empty
  never equals query) → works with bind. Note the deliberately
  ToString-free variants (`StaticFindObject_InternalNoToStringFromNames`
  :161-208, `…FromStrings` :210-217, staged :208-264) exist precisely to
  avoid this path early in init — they do not call ToString at all and are
  unaffected either way.
- `unreal/src/UObjectGlobals.cpp:365-372` long-name find arm: `Equals`
  index check + `GetFullName` string check — string check never passes
  today; repaired by bind.
- `unreal/src/UObjectGlobals.cpp:1101/1114/1161/1204`
  `GlobalScriptHooks` keyed by `Stack.Node()->GetFullName()`: hooks never
  match today (empty keys) and hook registration (:1161) keys garbage;
  with bind, keys are real both at registration and dispatch — matches
  become possible (registration and dispatch both post-init, so no
  mixed-key window; Lua mods start after init).
- `unreal/src/ClassListener.cpp:39` class-delete listener full-name map:
  keys become real names (map is built and consumed post-init; no stale
  pre-bind entries possible).
- `unreal/src/UFunctionStructs.cpp:190/197`, `ReflectedFunction.cpp:19/22`,
  `FProperty.cpp:73`, `GUI/Dumpers.cpp`, `GUI/DataDumpers.cpp`,
  `ObjectDumper/ObjectToString.cpp`, SDK generator family: error text,
  dumps, SDK headers — cosmetic-to-functional repairs.

### 1c. Direct FName consumers outside NameTypes (port root, live code; 61 sites)

| File:line | Purpose | Empty today | With bind |
|---|---|---|---|
| unreal/src/UnrealInitializer.cpp:1145,1159,1177,1413 | required-object wait/timeout log names | logs "UE4SS_None" | real names (log-only; the find itself uses the NoToString variants) |
| unreal/src/UEnum.cpp:375,382 | enum common-prefix derivation | prefix ""/garbage | real prefixes (SDK gen + Lua enum display) |
| unreal/src/FField.cpp:447 | checkf message for unknown field class | message garbage | real |
| unreal/src/FSoftObjectPath.cpp:204,215,218 (`ToFString` ×3 in `AssetPathToFString`) | soft-path → string (ResolveObject/TryLoad/GetAssetPathString) | empty paths → lookups fail | real asset paths |
| unreal/src/CoreUObject/UObject/TopLevelAssetPath.cpp:73,76 (`AppendString`, live block; :24-95 siblings are commented out) | "/Pkg.Asset" rendering | empty/garbage | real |
| unreal/src/TypeChecker.cpp:590 `Param->GetClass().GetName() == STR("ObjectProperty")` | type-scan bootstrap: capture FObjectProperty::StaticClassStorage | never equal → storage never set (downstream property-push degradation) | can match → repairs bootstrap |
| ue4ss/src/LuaType/LuaFName.cpp:122 | Lua `FName::ToString()` API (the FName→Lua-string marshal) | Lua sees "UE4SS_None" | real string |
| ue4ss/src/LuaType/LuaUObject.cpp:313,368,399,702,798,946,962,2357,2482 | "no registered handler for type 'X'" error text (UFunction params/returns, struct/array conversion, property access) | errors name the type "UE4SS_None" | errors name real types (diagnosability repair) |
| ue4ss/src/LuaType/LuaUObject.cpp:1626 | enum-name → Lua table pairs | all enum entries key "UE4SS_None" | real entry names |
| ue4ss/src/LuaType/LuaUScriptStruct.cpp:175,184,200 | struct property name/type in iteration + error text | garbage | real |
| ue4ss/src/LuaType/LuaUDataTable.cpp:111,185,353,376 | DataTable row-name keys/values to Lua | all rows key "UE4SS_None" | real row names |
| ue4ss/src/LuaType/LuaTMap.cpp:371,378; LuaTArray.cpp:176,226; LuaTSet.cpp:286 | inner/key/element type names (errors) | "UE4SS_None" | real |
| ue4ss/src/Mod/LuaMod.cpp:225,319,422,6828,6858 | hook return/param type warnings | "UE4SS_None" | real |
| ue4ss/src/ObjectDumper/ObjectToString.cpp:276 | TSet/TMap dump FName keys | "UE4SS_None" | real |
| ue4ss/src/GUI/Dumpers.cpp:173 | GUI JSON name dump | "UE4SS_None" | real |
| ue4ss/src/SDKGenerator/UEHeaderGenerator.cpp:623,639,640,697,1044,1291,1297,2344,2345,2953,3119 | SDK gen: enum names, delegate class names, CDO FName property values, config names, rep-notify funcs | wrong/duplicate SDK output | correct |
| ue4ss/src/SDKGenerator/Common.cpp:142,202,207; Generator.cpp:478,532; USMapGenerator/Generator.cpp:372 | enum/package name sanitizing, .usmap | garbage | correct |
| ue4ss/src/LuaType/LuaCustomProperty.cpp | (checked) — pure offset/class plumbing, zero name formatting | n/a | n/a |

Excluded as NOT engine-FName paths (verified): `LuaUObject.cpp:88`
(`FWeakObjectPtrName::ToString()` is a `constexpr static const char*` Lua
type-name, LuaUObject.hpp:220-232 pattern — not FName::ToString);
`LuaFText.cpp:85` and `UEHeaderGenerator.cpp:1429/1435` (FText::ToString);
`UnrealInitializer.cpp:269` (`ScanTargetToString` module-name helper);
`FTopLevelAssetPath::AppendString(FWideStringBuilderBase&)` etc.
(commented out in TopLevelAssetPath.cpp); fmt library's own ToString-family
(unrelated). UEHelpers is Lua-side only — no C++ occurrence in the port
tree; its Lua calls land on FindFirstOf/GetFullName/LuaFName.ToString above.

### 1d. Staged l26/l27 consumers (typescan-r4 candidate)

| Site | Purpose | Empty today | With bind |
|---|---|---|---|
| DA `candidate/src/UObjectGlobals.cpp:682-696` `FindXOfQueryMismatch` (`Name.ToString()` :686) | verified-query round-trip: construct FName from query, ToString-compare | **always mismatch → every FindFirstOf/FindAllOf string overload takes the fallback walk — and the walk's own compare (:702) is also ToString-based, so BOTH arms find nothing (the observed console nil)** | match → fast `Equals` walk; genuine mismatches still take the string walk |
| DA :698-706 `FindXOfStoredNameMatchesQuery` (`Struct->GetNamePrivate().ToString()` :702) | the string-walk comparator | never equal → walk finds nothing | real compare, walk works |
| DA :661-680 `LogFindXOfQueryFallback` (`Result.ToString()` :669, EB's `ts:'…'` tail) | miss-branch diagnostics | `ts:''` always | `ts:'<name tail>'`; interpretation note in §2g |
| CN `candidate/src/LuaUObject.cpp` `[UF] nofn [n:X] cls=[n:Y]` (:2360-2362 comment: "never ToString") and `candidate/src/UObject.cpp` `[UF] miss w/c/v/1=` (:684-686 same discipline) | miss-branch logging | **zero ToString dependency by design** (comparison indices only) | unchanged — the bind does not touch these logs |
| CN `candidate/include/Unreal/NameTypes.hpp` `NotifyFNameCtorUnreadyOnce` | ctor-unready notice | ctor-side, not ToString | unchanged |
| EI `candidate/src/NameTypes.cpp:196-207` `ToStringInternalWrapper_UsingEngine` | the bind's per-call user | n/a (new) | 1035-TCHAR buffer; anomaly guard `length >= kOutUnits || buffer[length] != 0` → once-notice + legacy route for that call |
| NativePropertyPointerGuard.inl:53-74 `fname_ends_in_property` (`name.ToString()` :58, gated :211-213 by `fname_index_within_verified_pool`) | type-scan bootstrap class check (suffix heuristic arm) | `len < 8` → always false → scan admits only the 39 known-name classes (fail-closed) | suffix test functional as designed (l24 semantics); pool-bound gate still in front; known-name arm still first |
| Seeds L14/L15 pool machinery (`rc::fnameabi::roundtrip`, pool word reads) | slot-2 ctor probe + Phase 4c dual-check | uses its OWN pool decoder, never FName::ToString | unchanged (it becomes the bind's referee) |

## 2. Danger check — does anything depend on the empty-string behavior?

**(a) "UE4SS_None" sentinel**: produced at NameTypes.cpp:115/123 (port) /
:142/150 (staged). Grep across port + staged trees: **zero comparators**
(`== STR("UE4SS_None")` etc. occur nowhere). It is display-only. With the
bind active the sentinel can still appear only via the legacy routes — no
consumer branches on it.

**(b) Length-0 fast paths**: the only one found is
`fname_ends_in_property`'s `len < 8 → false` (guard .inl:61). That is a
fail-closed FILTER, not a sentinel: empty simply disables the suffix
heuristic (known-39-names arm still admits). A working ToString restores
the designed l24 behavior; the verified-pool index bound (:146-167) still
front-runs the suffix test, so out-of-pool garbage cannot reach ToString
through the guard. The checked wrappers (:291-334) catch the pre-bind
"ToString was not ready" throw and record LayoutMissing — contained both
before and after.

**(c) Seeds "Unbound by design" annotations**: staged
UnrealInitializer.cpp:1368 (l26) / :1491 (l27, updated): "fname_tostring
SCAN route (seeds Phase 4c binds the engine ToStringInternal when
admitted, else the Kismet fallback owns it)". The annotation is
documentation of the scan-route bypass (`config.fname_tostring_fstring =
false`), not a behavioral guard — nothing reads it. The Seeds admission
and L15/L14 checks read pool words and use `rc::fnameabi`'s own decoder;
`IsNone()` is index-based (NameTypes.hpp:413-421); the ctor-unready guard
(CN item 2) watches ConstructorInternal, not ToString. **No seeds guard
can flip.** The bind is explicitly OPTIONAL in the admission contract
(refusal is non-terminal for the seed table), so a dual-check mismatch
cannot cascade into slot 0/2/3/4 arming.

**(d) DA verified-query**: empty ToString currently makes
`FindXOfQueryMismatch` return true for EVERY name (fallback walk always
taken) — and because the walk comparator is also ToString-based, results
are nil, not wrong-but-slow. There is no code path that BENEFITS from the
mismatch (the fast path is only skipped, never wrongly taken). With the
bind, correct names take the fast path — the exact condition DA's harness
(DA1_GOOD) tested. Exceptions from ToString are caught → mismatch →
walk (DA :688-695), unchanged.

**(e) Numbered-name semantics**: the engine form appends the external
`_N` suffix when Number != 0 (EngineToStringProfile.hpp ABI notes). DA's
`iequal(ToString, Query)` will mismatch for numbered names → string walk
→ correct. TypeChecker:590 and class-name consumers compare unnumbered
class names. No consumer assumes suffix-free output for numbered FNames
(grep found none comparing ToString of a possibly-numbered name to a bare
literal except DA, which is mismatch-safe by construction).

**(f) Early-init ordering**: the bind arms in SeedArming Phase 4c —
earlier than the Kismet CDO lookup (PostInitialize, :1012-1064). Consumers
running between those points previously THREW ("not ready") or were routed
around via the `StaticFindObject_InternalNoToString*` family; those
NoToString variants do not call ToString and remain correct. Nothing
catches-and-relies-on the early throw (the guard wrappers catch for
logging, then fail closed — same either way).

**(g) EB's `[FFO] query-fallback ts:'…'` decision tree**: while the bind
is DISABLED, `ts:''` means "Kismet route is the liar" (EB §4). While the
bind is ACTIVE, empty output is excluded by the anomaly guard + admission
probe, so `ts:''` should never occur; if it ever does, it now indicates
the engine route returned an impossible length (the once-notice
`[FN] tostring-bind anomaly` fires in that same window). Operators should
read `ts:` semantics jointly with the `[FN] tostring-bind …` markers.
Documentation nuance only — no code branches on it.

**(h) `FName::GetPlainNameString` (NameTypes.cpp:200-206 port / :297-303
staged) — the ONE real hazard found, currently dead code**: it
`memcpy`s `String.size() * sizeof(File::StringType::size_type)` bytes
(a multiplier bug — should be `sizeof(TCHAR)`; with 8-byte size_type it
over-reads the source 4x) into a `TCHAR[NAME_SIZE=1024]` with no bound.
Today (empty output) it copies ~80 bytes harmlessly; with the bind, a
maximal legal name (`1023 + "_NNNNNNNNN"` = up to 1034 TCHARs) would
overrun the destination and over-read the source. **Zero callers exist**
in port + staged trees, so it cannot fire today — but the bind makes long
outputs REACHABLE. Recommend: fix or reroute it (use the TCHAR*,uint32
form with `NAME_SIZE` directly, honoring its truncation) before anyone
ever wires it up. Flagging as the only consumer needing individual
attention.

**(i) Per-call cost**: the engine route zeroes a 1035-TCHAR (2070-byte)
stack buffer per call and `StringType{buffer, buffer+length}` copies once
— replacing a ProcessEvent + GMalloc-FString per call. Strictly cheaper
than the Kismet route; more expensive than the FStringOut scan route's
single allocation (non-seed targets keep that route — see §4 caveat).
UObject::GetName-heavy scans (StaticFindObject_InternalSlow walks all
~10k+ objects) get the largest win. No consumer overflows the stack
(2070 B is trivial).

## 3. Signature fit — can every consumer route through the truncating TCHAR*,uint32 form?

- **Buffer size**: `rc::engtostring::kOutUnits = 1035` = engine NAME_SIZE
  (1024) + 11. Legal pool names are capped at Len < 0x400 = 1023 TCHARs
  (entry header `Len:10`, profile `entry_header` "bits15:6 Len"; the
  engine core returns `ERROR_NAME_SIZE_EXCEEDED` at len ≥ 0x400 per CJ
  §2a) and the number suffix adds at most `_` + 10 digits = 11 units →
  1034 + NUL = 1035. **No legal name truncates**; the engine's own
  `cmovb` clamp truncates at OutSize-1 for hypothetical over-length, and
  the wrapper's anomaly guard (`length >= kOutUnits || buffer[length] !=
  0`, staged NameTypes.cpp:201) routes any such call to the legacy route.
  The profile carries no per-name census (static RE only), so the 1023
  cap + engine clamp is the binding guarantee — the same guarantee the
  engine's own `ToStringTruncate` relies on.
- **Consumer shape**: every consumer takes `StringType` (or FString via
  `ToFString`) — the wrapper owns the buffer and constructs the string;
  no consumer supplies its own buffer except `GetPlainNameString`
  (§2h, dead, needs the fix noted). **No live consumer needs > 256
  TCHARs**; the only >1024-TCHAR producer would be GetPlainNameString's
  missing bound.
- **ABI fit**: `(rdi=const FName* {ComparisonIndex@0, Number@4},
  rsi=TCHAR* Out, edx=uint32 OutSize) → eax=count`. Port FName is 8 bytes
  with ComparisonIndex@0/Number@4 (static_asserts in NameTypes.hpp:401-410
  non-case-preserving arm) — matches. `static_assert(sizeof(CharType)==2)`
  (staged NameTypes.cpp:198) pins TCHAR width. The form handles ANSI→wide
  in-place widening, the wide memmove, NUL termination, and the external
  `_N` suffix (EngineToStringProfile.hpp byte-verified decode) — i.e., it
  is NOT just the plain entry: it is the number-aware core behind every
  public non-allocating form (CJ §2b). Case-preserved/outline-number
  builds would change FName layout — out of scope, this title is
  !CASE_PRESERVING/!OUTLINE_NUMBER (CJ §1, EB §1).
- **Two Function<> members, no shared slot**: the FStringOut-form
  `ToStringInternal` (scan route, port UnrealInitializer.cpp:787,
  Signatures.cpp:209) and the TCHAR-form `ToStringNative` (Phase 4c) are
  separate statics with separate signatures — no ABI-drift hazard between
  the scan and seed routes; each route calls only its own form.

## 4. Dual-check blast radius — uniform degradation

Phase 4c refusal paths (staged UnrealInitializer.cpp:1232-1345): wrong
seed class, expired budget, ByteProperty-id drift, prologue mismatch,
probe mismatch/empty, pool-decoder disagreement — each logs loudly and
**never calls `FName::ToStringNative.assign_address`**. Then
`ToStringNative.is_ready()` (Function.hpp:104-106 — ready only after
assign) is false in `ToStringInternalWrapper` (staged :222), and the
wrapper proceeds in the EXACT legacy order: Scan route (dead under seeds —
scan disabled), Kismet Conv_NameToString (armed in PostInitialize), else
throw. Since every consumer enumerated in §1 reaches ToString only
through that wrapper (verified: no direct callers of any of the four
primitives outside NameTypes.cpp/bind sites), **degradation is uniform:
ALL consumers revert to today's behavior — "UE4SS_None"/empty via Kismet,
or the pre-init throw — none partially.** The per-call anomaly guard
(staged :201-205) is also uniform: it falls back to
`ToStringInternalWrapper_UsingEngine_Legacy` (:182-194), the same legacy
chain, for that call, with a once-per-process notice.

One nuance to keep in mind (not a defect): the engine route is preferred
OVER the Scan route when both could be ready (Prospero-only compile).
Under seeds the scan route is structurally off, so no interaction; on a
hypothetical non-seed Prospero build with an AOB scan also finding
`fname_tostring_fstring`, the engine seed bind would win deterministically.

## 5. VERDICT

- **Single choke point: YES.** `FName::ToString` (both cv) →
  `ToStringInternalWrapper` is the sole FName→string route; ToFString
  family, `operator==(CharType*)`, `UObject::GetName/GetPathName/
  GetFullName`, `FField::GetFullName`, all Lua marshaling, DA's
  verify+fallback, and the type-scan suffix test all terminate there. The
  dual-check refusal degrades every consumer identically to the legacy
  Kismet route; the anomaly guard degrades per-call identically.
- **Consumers needing individual attention: 1.**
  `FName::GetPlainNameString` (dead code, wrong memcpy multiplier +
  unbounded copy into TCHAR[1024]) — safe only because nothing calls it;
  fix or reroute through the truncating TCHAR*,uint32 form before it is
  ever wired. Everything else is repaired or unchanged by the bind.
- **No consumer depends on the empty-string behavior.** The only
  empty-dependent logic is fail-closed filtering
  (`fname_ends_in_property`, DA mismatch) that exists IN SPITE OF the
  emptiness, not because of it; the "UE4SS_None" sentinel has zero
  comparators; seeds guards and CN miss-branch logging never touch
  ToString.
- **Signature fit: complete.** 1035 units ≥ every legal name+suffix; all
  consumers take StringType from the wrapper; no live consumer needs
  >256 TCHARs; the engine clamps and the wrapper guards impossible
  lengths.

Counts: **61 direct engine-FName ToString/ToFString sites** in live port
code (§1c + §1a's 4 converters), **5 FName-API internal sites** (§1a),
**~170 indirect UObject name-formatting sites** (118 `GetName()` across
11 files + `GetFullName()` across 19 files) funneling through
UObject.hpp:237, plus **3 staged DA sites** and **1 staged guard site**
(typescan-r4), and **0 in CN's miss-branch logging** (index-only by
design). Excluded as non-FName: LuaFText, FText value dumps, Lua
type-name constants, FSoftObjectPath/FTopLevelAssetPath's own ToString
entry points (their FName members are counted where they convert).

Provenance: read-only. Files read: port root unreal/{src,include} +
ue4ss/src (grep-verified enumeration, no writes); staged
typescan-r4/candidate/{src,include} + include/NativePropertyPointerGuard.inl
+ EngineToStringProfile.hpp; l25-prep reports CJ/DA/CN/EB/CI +
CLAUDE-REPORT; worker-l3-profile game-profile json (pool constants).
Scratch: none. Only this report written.

— agent EK (ZCode), 2026-10-08.

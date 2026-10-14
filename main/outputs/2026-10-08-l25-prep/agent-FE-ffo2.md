# agent-FE — [FFO2] find-path instrumentation (UObjectGlobals.cpp)

Date: 2026-10-09 (task issued 2026-10-08 session)
Scope: ONLY `2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp`.
No other file touched (CHANGES.json, drivers, NameTypes.*, LuaFName.cpp untouched).

## 1. Match predicate found (the actual compare)

- The walk predicate is `FName::Equals`, defined at
  `candidate/include/Unreal/NameTypes.hpp:471-474`:
  `return ComparisonIndex == Other.ComparisonIndex;`
  **ComparisonIndex ONLY. Number is NEVER compared in the FindXOf walks.**
  Claude's "compare (index+Number)" suspicion: only an *index* mismatch can
  lose a match on this path; a Number difference cannot.
- Direct match: `Class->GetNamePrivate().Equals(ClassName)` where
  `Class = Object->GetClassPrivate()`.
  Was at UObjectGlobals.cpp:996 (FindFirstOf walk) / :1147 (FindAllOf walk) /
  :1097+:1105 (3-arg FindAllOf cache-scan arm) pre-edit; now via the hoisted
  `bDirectClassMatch` locals at lines 1094 / 1213 / 1304 / 1322 (same single
  evaluation, semantics unchanged for both builds).
- Derived-class match: `super_struct->GetNamePrivate().Equals(ClassName)` over
  `TSuperStructRange(Class)` — same index-only Equals per super.
- Hash-tables arm (when `UnrealInitializer::ShouldUseHashTables()`): no
  per-object name compare; `FindRootClassesByName` (UObjectGlobals.cpp:634,
  `Class->GetNamePrivate().Equals(ClassName)` at :639) collects root classes,
  then `FindObjectSearcher(Root, nullptr).ForEach` yields instances.
- Query construction seam: `MakeFindXOfClassName` (UObjectGlobals.cpp:700) —
  FNAME_Find then FNAME_Add fallback, unchanged.

## 2. Marker format landed

Helper block `#ifdef __PROSPERO__` at UObjectGlobals.cpp:973-1052
(`FindXOfFfo2Stats`, 16-slot dedupe table keyed on (ComparisonIndex, Number),
`LogFindXOfFfo2` at :1014). One line per distinct query name per session;
counters are per-call locals; whole logger body is try/catch.

- FindFirstOf (FName overload, both arms; emits at :1083 hash / :1123 walk):
  `[FFO2] q='<name>' id=0x<ComparisonIndex hex> num=<Number> scanned=N exact=E viaSuper=S first=0x<hex-or-0> arm=<walk|hash>`
- FindAllOf (FName 2-arg overload, both arms, emits at :1289 hash / :1334
  walk; plus the 3-arg string overload's cache-scan arm at :1247):
  `[FFO2] q='<name>' id=0x.. num=.. scanned=N exact=E viaSuper=S found=F first=0x.. arm=<walk|hash|cache>`
- Semantics: scanned = objects iterated (lambda invocations in
  ForEachUObject/searcher walks); exact = direct class-name Equals hits
  (independent of the IsValidObjectForFindXOf acceptance filter); viaSuper =
  matches found only via the TSuperStructRange chain; first = first accepted
  object pointer or 0; found = objects appended by this FindAllOf call.
  arm=hash: exact counts matched ROOT classes (FindRootClassesByName) and
  viaSuper is structurally 0 (no per-object super walk) — the tag prevents
  misreading that as a broken super chain.
- Extra fields beyond the directive's literal shape (deliberate, both serve
  the directive's own failure-mode discrimination): `num=` rules the
  '+Number' variant in/out directly; `arm=` disambiguates which walk produced
  the counters.
- q= rendered via the PURE in-port pool decoder
  `FNamePoolDecoder::try_tostring` (RC_UE_API declaration confirmed at
  NameTypes.hpp:262, EY's export; definition NameTypes.cpp:510). NEVER
  engine ToString. Decoder refusal -> q='' while id= still carries the index.
  Render capped at 64 chars, newlines->space, non-ASCII->'?' (one ASCII line).
- Existing [FFO] string-path markers (add-fallback :694, query-fallback :835,
  query-embedded-NUL :787) are byte-identical, untouched. The verified string
  wrappers (FindFirstOfVerified/FindAllOfVerified) still land in the
  instrumented FName overloads on the fast path, so the Lua bridge path is
  covered. Host builds compile the original code (all instrumentation is
  `#ifdef __PROSPERO__`-guarded; the only host-visible delta is the hoisted
  `bDirectClassMatch` bool, evaluation order unchanged).

## 3. Edited file hash

sha256(2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp) =
63e6e05773dc6d25a5dcd666c1f1cc33542517e26de355a74b180f5fce522b20
(1998 lines after edit)

## 4. Syntax evidence

- Command: prospero-clang++ (sdk-v0.43 wrapper, x86_64-sie-ps5 target,
  -std=c++23, full include/define set replayed from agent-ER's preserved
  working command in /tmp/l26-er-pooldecoder/compile/unreal-uobjectglobals.prospero-syntax.log)
  with `-fsyntax-only`.
- rc=0. Log: /tmp/l26-fe-ffo2/prospero-syntax.log (command + rc + full stderr).
- 5 warnings remain, ALL inherited from pre-existing third-party headers
  (Atomic.hpp -Wswitch x2, UnrealType.hpp enum-conversion x2, FProperty
  -Wundefined-inline) — the identical "inherited only" set ER's receipt
  documented; zero diagnostics from the new code or the edited TU itself.
- Toolchain caveat (environment drift, not source): the local machine now
  resolves the SDK wrapper's llvm to brew llvm@18 only, under which the
  VENDORED x86-intrinsics headers (ue4ss-unreal-native-build-1/...
  /clang-x86-headers/xmmintrin.h) emit -Winvalid-constexpr ERRORS before any
  project code is parsed — they fire identically on ER's unmodified-era
  command with current PATH. The syntax check therefore adds
  `-Wno-invalid-constexpr` for that pre-existing header noise; no source or
  project flag files were changed for this.
- Also note: the pass1 command.json's `-ivfsoverlay .../l26-A/headers.vfs.json`
  path is stale (dir was renamed to l26-A.lfb-203415); ER's command's overlay
  set (all three vfs files verified present) was used instead and is the
  proven-working equivalent for this TU.
- No --execute, no console contact.

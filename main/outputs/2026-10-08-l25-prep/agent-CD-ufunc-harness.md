# Agent CD — l26 UFunction-resolution host harness (reproduction + fix validator)

Built and run 2026-10-08 (late). Work dir `/tmp/l26-ufunc-harness/` (self-contained;
source trees READ-ONLY — only read, sliced, sha-pinned). All assertions pass,
exit 0. JSON results: `/tmp/l26-ufunc-harness/results/run.stdout.txt`;
provenance: `results/manifest.json` (7 sources, 20 verbatim regions, per-region
sha256).

RUN COMMAND (extract → generate → compile → run full matrix + fix validation):

```
python3 /tmp/l26-ufunc-harness/extract_and_build.py     # exit 0 = all checks pass
/tmp/l26-ufunc-harness/bin/harness                       # re-run only; JSON on stdout
```

One command does everything: slices the verbatim regions from the port root +
l23 lineage by exact anchors, sha256-pins them, generates `harness.cpp` from
`template.cpp.in`, compiles with host `clang++ -std=c++20` (SeedValidation.hpp
included verbatim via `-I`), runs the 12-scenario matrix + guard/SeedValidation
invariants + fix-validation assertions, writes `results/`.

---

## 1. REPRODUCTION STATUS

**Reproduced.** The exact console error

```
Tried calling a member function but the UObject instance is nullptr
```

(fragments E0 "Tried callin" / E1 "n but the UO" / E2 "nullptr" from
`/tmp/zzdiag2.log`) is produced by the REAL port chain (verbatim-extracted,
see §2) under exactly ONE condition class:

**db is a NON-NIL, NULL-REMOTE UObject userdata — i.e. `FindFirstOf(...)`
returned nullptr and `auto_construct_object` wrapped the null** (LuaUObject.cpp
`if (!object) { UObject::construct(lua, nullptr); }`). Then:

1. `db == nil` is FALSE for the userdata (zzdiag "A3 ok" passes — the check is
   insufficient; `IsValid()` was never called),
2. `db:GetItemIDFromDeprecatedEnum("Bandage")` → `__index` → verbatim
   `prepare_to_handle` null-receiver branch constructs ANOTHER null-remote
   UObject userdata,
3. Lua calls it → verbatim `__call` metamethod → `!get_remote_cpp_object()` →
   the error at LuaUObject.hpp:517. Lua's luaL_traceback appends
   "in method 'GetItemIDFromDeprecatedEnum'" — matching console chunks E3/E4.

Census (12 scenarios, full table in run.stdout.txt):

| scenario | db remote | scan visited | final error | console-shaped |
|---|---|---|---|---|
| S1 healthy | valid | 3 (super chain) | none — node resolved, ProcessEvent ran | no |
| S2 Lua-string→ctor mismatch on classname | **null** | 0 | **exact console error** | **YES** |
| S2b both names misencoded | **null** | 0 | **exact console error** | **YES** |
| S3 member-name miss only (db valid) | valid | 0 (guard skips scan) | `attempt to call a userdata value` | no |
| S4 pool-index mismatch (db valid) | valid | 3, no match | `attempt to call a userdata value` | no |
| S5 Children@0x40 (0-visited) | valid | 0 | `attempt to call a userdata value` | no |
| S5b Next@0x20 (truncated) | valid | 2 | `attempt to call a userdata value` | no |
| S5c ClassCastFlags@0xd0 | valid | 3 | none (path unaffected for instance receivers) | no |
| S6 ctor returns Number=7 | valid | 3, no match | `attempt to call a userdata value` | no |
| S7 lowercase member | valid | 0 | `attempt to call a userdata value` | no |
| S8 FuncMap(+0x130) unbound [hypothetical] | valid | 0 | `attempt to call a userdata value` | no |
| S9 unbound native FindFunctionByName [hypothetical] | valid | 0 | `attempt to call a userdata value` | no |
| S10 ctor seed not ready | **null** | 0 | **exact console error** — EXCLUDED by console log L559 | shape-yes |
| S11 no live non-CDO instance | **null** | 0 | **exact console error** — open hypothesis | shape-yes |

**Root cause pinned experimentally:** the observed error text is reachable
ONLY through the null-remote-db path. EVERY failure of the UFunction
resolution machinery itself (name→FName miss, comparison-index mismatch,
0-visited or truncated children walk, FuncMap/unbound-native variants)
produces a DIFFERENT observable — Lua's `attempt to call a userdata value`
(the TrivialObject invalid wrapper from LuaObject.hpp L121-140 has NO `__call`
metamethod). This adjudicates the CA/CB divergence: CA's UTF-16 decode of the
fragments ("UO" = UObject, not UFunction) and null-wrapper conclusion are
confirmed dynamically; CB's ranked #1 (runtime FNAME_Find miss silently
skipping GetFunctionByNameInChain via the LuaUObject.cpp:2314 guard) is a real
latent defect class but CANNOT produce the observed text — harness S3/S4 prove
it yields attempt-to-call instead. CB's premise "(a) FindFirstOf works" traces
to the finding doc's validity-check artifact (db ~= nil passes on a
null-wrapper).

**Open upstream axis (not resolvable on host):** WHY FindFirstOf returned
nullptr on console. Both demonstrated sufficient causes keep the console facts:
- S2 class — Lua-string→ctor conversion mismatch (modeled at the ctor-seed
  boundary; NB the port tree carries `Helpers/String.hpp.orig-codecvt-buggy`;
  the ctor was verified only via the UTF16 "ByteProperty" LITERAL probe, never
  via the Lua-string path). Consistent with S2b: Kismet runtime chain breaks
  too while the direct full-name string scan still finds objects.
- S11 — no live non-CDO instance at probe time (contradicts PC parity of the
  same mod at the same state; weaker but unfalsified).
- S10 (ctor-not-ready) shape-matches but is excluded by console log
  "[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified".
Console discriminator for the next diag (zzdiag v4): log `db:IsValid()` and
`db:GetAddress()` after FindFirstOf, plus one FindFirstOf("PlayerController")
cross-check; also CA/CB's observability ask (property_name index, guard-skip
flag, visited count in the failure branch).

## 2. The chain, extracted verbatim (sha-pinned in results/manifest.json)

Port root `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
+ l23 lineage SeedValidation.hpp. Region files under `/tmp/l26-ufunc-harness/regions/`.

| region | source | lines (port) | role |
|---|---|---|---|
| `prepare_to_handle` | ue4ss/include/LuaType/LuaUObject.hpp | ~791-840 | `__index` body: member→`FName(member, FNAME_Find)` (:821), property lookup, null-receiver branch (THE producer of null-remote userdatas) |
| `handle_unreal_property_value` | ue4ss/src/LuaType/LuaUObject.cpp | ~2303-2335 | UFunction branch: `property_name != FName(0u,0u)` guard (:2314) skips scan on NAME_None; found→push, miss→TrivialObject invalid |
| `get_function_by_name` | unreal/src/UObject.cpp | ~642-672 | GetFunctionByName(InChain): TFieldRange<UFunction> children+super scan, `GetNamePrivate() == name` (index+number) |
| `receiver_as_struct` | unreal/src/UObject.cpp | ~595-604 | receiver class/struct selection (cast-flags) |
| `uobject_call_metamethod` | ue4ss/include/LuaType/LuaUObject.hpp | ~512-520 | `__call`: **the console error string** (:517) |
| `call_ufunction_prologue` | ue4ss/src/LuaType/LuaUObject.cpp | ~161-197 | func/context resolution + "without both UFunction and calling context" guard (:196) |
| `auto_construct_object` | ue4ss/src/LuaType/LuaUObject.cpp | ~438-482 | null-safe wrap: `!object → UObject::construct(lua, nullptr)` — non-nil null-remote userdata |
| `luamod_findfirstof` | ue4ss/src/Mod/LuaMod.cpp | ~1649-1657 | Lua FindFirstOf binding → `FindFirstOf(ensure_str(get_string()))` |
| `findfirstof_fname` | unreal/src/UObjectGlobals.cpp | ~434-479 | GUObjectArray scan, `Class->GetNamePrivate().Equals(ClassName)` (index-only) |
| `findfirstof_string` | unreal/src/UObjectGlobals.cpp | ~501-503 | `FName(ensure_str(...))` default FNAME_Add |
| `is_valid_object_for_findxof` | unreal/src/UObjectGlobals.cpp | ~220-224 | CDO/archetype/class exclusions |
| `staticfindobject_slow` | unreal/src/UObjectGlobals.cpp | ~137-159 | startup full-name STRING scan (known-good; KV vector) |
| `fname_construct_with_string`, `fname_public_ctors`, `fname_equality`, `fname_equals`, `fname_tounstable`, `fname_isnone`, `fname_entryid_*` | unreal/include/Unreal/NameTypes.hpp | ~62-128, 239-255, 303-315, 322-335, 342-348, 385-411, 413-421 | name→FName via ConstructorInternal seed; ==/Equals/IsNone semantics |
| SeedValidation.hpp (whole, via -I) | outputs/2026-10-08-l23-package/lineage/ | — | rc_seed pool layout/entry encoding/validators/seed classification |

Stubbed out-of-region deps (documented in harness.cpp): `Function<Sig>` mock,
Lua VM plumbing (mock Lua stack/userdata registry), LuaType metaclass-family
constructs, property pusher tail (LuaUObject.cpp:2337-2370 — unreachable with
field==nullptr), ProcessEvent tail. Mock engine objects use the staged
authenticated walk offsets ([UStruct] Children=0x48/SuperStruct=0x40/
ChildProperties=0x50, [UField] Next=0x28, [UClass] ClassCastFlags=0xd8 — note
the port header comment says 0xD0 while the profile says 0xd8; both recorded).

## 3. Mock fixtures (console-parity anchors)

- **FName pool per rc_seed layout**: blocks/stride/entry encoding exactly per
  SeedValidation (header `len<<6`, stride-2 slots, id = block<<16|slot).
  Console parity: interning "None" then "ByteProperty" yields id **0x3**,
  matching console log "[Seeds][L15] FName output verified: pool
  ToString=ByteProperty ... id=0x3" (zzdiag2.log L557-559) — the layout
  formula reproduces the observed id. Pool-struct words at +0x14040/+0x14044;
  `check_probe_id`/`check_none_anchor`/`entry_matches_ascii` all pass on the
  fixture; `check_pool_shape` admits it; the captured console Blocks[0]
  (0x22C0000000 vs envelope 0x1100400000) classifies CaptureTruncated and
  `resolve_listing_gap(..., CopyinProbeClass::Refused)` stays refused — the
  GATE-1 admission-blocked state, as on console.
- **Class graph**: `UObject ← ItemDatabaseSubsystemBase ← ItemDatabaseSubsystem`
  with children `GetInteractablePickupIDActorPath → GetItemIDFromDeprecatedEnum
  → SomeNotify(UField, filtered)` on the SUPER class (proves IncludeSuper) and
  `IsDatabaseReady` on the class itself; live instance + CDO (CDO excluded by
  the verbatim IsValidObjectForFindXOf).
- **Exec-only-text guard**: LOAD0 --X [0x400000,0x7879b1c) never registered
  readable (bridge parity); the ctor seed at 0x63d590 is CALL-only
  (rc_seed::classify_seed_address parity; calls counted, reads refused — both
  asserted).
- **KismetStringLibrary:Conv_NameToString** known-good vector: resolvable via
  the verbatim startup full-name string scan under ALL conditions (it never
  builds an FName) — separating lookup-method failure (S2b: runtime chain
  broken, direct scan works) from mod-specific failure.

## 4. FIX-VALIDATION ASSERTION SET (what a fixed build must pass)

Encoded as CHECKs in `main()` (run every invocation; also re-verified under
freshly built healthy fixtures). Any incoming fix runs the same command; exit 0
required.

- **FV1** `FindFirstOf("ItemDatabaseSubsystem")` through the Lua-string path
  returns a non-null remote (auto_construct wraps a real object).
- **FV2** db userdata: non-nil AND `IsValid()`-true (remote non-null, not the
  TrivialObject/invalid wrapper).
- **FV3** the member FName constructed from the Lua string resolves: resolved
  node's `GetNamePrivate()` id equals the interned id of
  "GetItemIDFromDeprecatedEnum" (non-zero).
- **FV4** `GetFunctionByNameInChain` returns the pointer-identical UFunction
  node — found through the SUPER class (IncludeSuper honored), with
  visited-count 3 (all functions, non-function children filtered).
- **FV5** the constructed UFunction userdata carries {func=node,
  calling_context=db}; the verbatim call prologue passes its guard; ProcessEvent
  receives (node, params) and the Lua call returns the mapped id.
- **FV6** Kismet vector: Conv_NameToString resolves BOTH via the direct
  full-name scan and via the same runtime chain the mod uses.
- **FV7 guards still refuse genuinely-bad inputs** (fix must not silence them):
  - (a) exec-only-text reads refused / seed calls allowed (WalkRx parity);
    LOAD1 classifies DataReadable, LOAD8 Refused;
  - (b) FNAME_Find on a non-interned name still returns NAME_None (no
    fabricated ids); FNAME_Add interns unknowns with fresh ids ≥3;
  - (c) a genuinely null-remote object still raises the LuaUObject.hpp:517
    diagnostic (the error is the SIGNAL, not the bug);
  - (d) SeedValidation intact: entry-0 None anchor, entry-3 ByteProperty,
    entry-bounds refusal (block > CurrentBlock), pool-shape span admission,
    CaptureTruncated classification, GATE-1 copyin-refused stays refused;
  - (e) case-flipped names stay misses under FNAME_Find (byte-exact pool
    parity; no case-folding side effects).
- **FV8 regression**: ByteProperty literal probe parity id==3==pool word
  (console log L557-559) on every scenario start.

CB's proposed fix directions (member name via FNAME_Add + Equals in
GetFunctionByName(InChain), string-compare fallback) are compatible with this
set — FV3/FV4 are outcome-level; note they harden the resolution machinery
(S3/S4/S6/S7 classes) but the OBSERVED console error additionally requires the
FindFirstOf/Lua-string link to hold (FV1/FV2), which is where the live failure
sits. Do not touch seed admission / slot-2 binding / GATE-1 (CB item 4,
agreed).

## 5. Caveats / honesty

- Host mocks prove control-flow/condition reachability with the REAL chain
  code, not console memory behavior. The S2 misencode model (widened NUL
  bytes) stands for the conversion-mismatch CLASS at the ctor boundary; the
  actual console defect (if S2-class) needs the console discriminator in §1.
- `special_invalid_ptr()` (UINTPTR_MAX-0x1000) does NOT trigger the 517 error
  (non-null) — only real-nullptr remotes do; the harness asserts this
  distinction structurally (db_isvalid vs the error path).
- l22-diagnostics LuaUObject.hpp:518 (console build) carries the identical
  string via L22Diagnostics::throw_lua_error — same site, logging wrapper only
  (CA §1, verified).
- The harness's GUObjectArray is the mock scan list; the real console
  FUObjectHashTables path is off (console log), matching the modeled branch.

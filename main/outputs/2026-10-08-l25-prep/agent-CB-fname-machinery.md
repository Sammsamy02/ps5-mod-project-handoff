# CB — FName-from-string machinery audit (l26 fix input)

READ-ONLY audit. Sources: port root
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
(hereafter `port/`), live l25 tree
`outputs/2026-10-08-l25-runtime/typescan-r4/original/` (merged initializer,
byte-identical lineage to the built variants), build units
`outputs/2026-10-07-l18-full-build/units/` (SeedTable.h, NativeFNameAdapter.hpp,
FNameAbiProfile.hpp, SeedValidation.hpp), decrypted eboot
`outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`
(sha256 3b6cfad4…3622fc; code linked-vaddr → file offset is **+0x4000**, data
≈ identity), console logs `harvest/harvest-l25-B-final-ue4ss.log` and
`evidence/zzdiag-v2-session.log`.

---

## 1. The three string→FName construction paths (post-init)

All three funnel through the **same single engine function**: the no-length
WIDE ctor bound as seed slot 2 (`FName::ConstructorInternal`), runtime
`base+0x23d590` (runtime 0x63d590).

Call chain shared by all paths:

- `port/unreal/include/Unreal/NameTypes.hpp:239-255`
  `FName::construct_with_string` → `ConstructorInternal(StrName, FindType)`.
- `port/unreal/src/NameTypes.cpp:20` defines it; the l25 merged initializer
  binds it to `native_fname_thunk` at
  `l25-runtime/typescan-r4/original/src/UnrealInitializer.cpp:1142`
  (`FName::ConstructorInternal.assign_address(&native_fname_thunk)`).
- `native_fname_thunk` (same file, :958-967) →
  `rc::fnameabi::invoke(g_native_fname_ctor, text, mode)`
  (`units/NativeFNameAdapter.hpp:39-49`; UTF16 conversion :16-38; ABI
  `void(*)(Name* out, const char16_t*, uint32_t mode)`, RDI/RSI/EDX).
- Engine side (binary, section 4): 0x23d590 → 0x23d600 (with-length wide:
  parses `_NN` suffix into the Number field, collapses UTF16→ANSI for pure
  ASCII) → 0x242470 (interning core: lazy pool init, then **mode==1
  (FNAME_Add) → Store 0x23c0b0; mode==0 (FNAME_Find) → shard-Find
  0x23c340**).

### (a) FindFirstOf("ItemDatabaseSubsystem") — the path that WORKS

1. Lua binding: `port/ue4ss/src/Mod/LuaMod.cpp:1632-1657`, string overload at
   :1651 `FindFirstOf(ensure_str(lua.get_string()))` — argument is at stack
   index 1, read correctly.
2. `port/unreal/src/UObjectGlobals.cpp:501-504` (std::string overload) →
   **`FName(ensure_str(ClassName))` with FindType defaulting to `FNAME_Add`**
   (`port/unreal/include/Unreal/NameTypes.hpp:303-310`).
3. Search: `ShouldUseHashTables()` is false on this build
   (`port/unreal/src/UnrealInitializer.cpp:108-112`;
   `fuobject_hash_tables_get` is "[Seeds] Unbound by design", l25 initializer
   :1243) → **linear GUObjectArray scan** comparing
   `Class->GetNamePrivate().Equals(ClassName)` (UObjectGlobals.cpp:457,
   super-struct compare :468).
4. `FName::Equals` compares **ComparisonIndex only** — Number ignored
   (NameTypes.hpp:342-348).

### (b) db:GetItemIDFromDeprecatedEnum("Bandage") — the path that FAILS

1. `__index` metamethod: `port/ue4ss/include/LuaType/LuaUObject.hpp:502-505`
   → `prepare_to_handle(Operation::Get, …)` (:791-840).
2. **`LuaUObject.hpp:821`: `FName property_name = FName(member_name,
   FNAME_Find);`** — **Find mode**, not Add.
3. Custom-property list miss (:822), property lookup miss (:836, on the
   CLASS — correct receiver).
4. `port/ue4ss/src/LuaType/LuaUObject.cpp:2303-2335`
   `handle_unreal_property_value`:
   - **:2314 guard: `if (!field && property_name != Unreal::FName(0u, 0u))`
     — a Find that returns NAME_None {0,0} silently SKIPS the lookup.**
   - :2316 `func = base->GetFunctionByNameInChain(property_name)`.
5. `port/unreal/src/UObject.cpp:660-672`
   `GetFunctionByNameInChain`: `ReceiverAsStruct(this)` (:596-604 — for the
   instance `db` correctly returns its UClass; verified semantics: the
   UClass-object's own `ClassCastFlags` member (UClass+216) contains
   CASTCLASS_UStruct only when the receiver itself is a class object) →
   `TFieldRange<UFunction>(Struct, IncludeSuper|IncludeInterfaces)` walks
   `UStruct::Children` (offset 72) via `UField::Next` (offset 40), filters by
   per-field `ClassCastFlags` (UnrealType.hpp:3174-3187), comparing
   **`Function->GetNamePrivate() == FunctionName` (:665) — `operator==` =
   full `ToUnstableInt()` = index AND Number** (NameTypes.hpp:322-325,
   401-410: 8-byte memcpy).
6. nullptr → invalid Lua object pushed (LuaUObject.cpp:2328-2333) → calling
   it throws the observed `"Tried calling a member function but the U[Object
   instance is] nullptr"` (`LuaUObject.hpp:512-519`). The reassembled zzdiag
   chunks (E0/E1/E2/E3…, `evidence/zzdiag-v2-session.log`) match this string,
   not a "UFunction nullptr" literal — that phrasing was the finding doc's
   interpretation; the failing call site is the __call-on-invalid-object
   path. Either way func==nullptr is confirmed.

### (c) Lua string-key marshaling (checked, NOT the bug)

The `LuaMadeSimple` __index wrapper (`port/first/src/LuaMadeSimple/
LuaMadeSimple.cpp:542-587`) calls the user callback with stack
[1]=userdata [2]=key [3]=ud-copy [4]=metamethod-container.
`get_userdata(force_index=1, preserve_stack=false)`
(`port/first/include/LuaMadeSimple/LuaMadeSimple.hpp:752-781`) **removes
index 1**, shifting the key to index 1, so the subsequent
`get_string()` (default index 1, LuaMadeSimple.cpp:755-760) reads the
**correct key**. `ensure_str_const` (String.hpp:615-647) uses the locale-free
`RC::Unicode` converter (the staged codecvt fix, String.hpp:11-19) — same
family as FindFirstOf's proven `ensure_str`. The `"Bandage"` argument is
marshaled much later in `call_ufunction_from_lua` (LuaUObject.cpp:196+,
param pushers :2303-2352) — never reached in the failing case.

### Why (a) succeeds on console while (b) fails

Not a different conversion function, not different pool access — the same
bound ctor is used. Three structural differences:

1. **FindType: (a) FNAME_Add vs (b) FNAME_Find.** Add is miss-forgiving
   (Store probes, inserts on miss, always returns a usable id); Find is
   miss-fatal (returns NAME_None {0,0} **silently** — binary 0x242505 and
   0x23c3ec both return zero without any flag), and the :2314 guard converts
   that miss into "skip the lookup entirely".
2. **Comparator: (a) `Equals` (index-only) vs (b) `operator==`
   (index+Number, full 64-bit).**
3. **Reflected-data dependency: (b) additionally walks the class Children
   linked list and per-field cast flags — two silent failure links (a)
   doesn't have.**

## 2. Seeds / PoolAdmission gating — what exactly is gated

- The switch: `read_pool_admission("/data/plugins/UE4SS-settings.ini")`,
  modes Oracle/Auto/Shadow (`units/pool_admission.h:36`), read at l25
  initializer :1074-1077. It configures **init-time validation of the pool
  label only**: `rc::fnamechecks::evaluate` inside `admission::retry`
  (image-segments + VirtualQuery + copyin-owned-read sources, optional
  shadow fallback, one shared `kAdmissionBudgetMs` deadline), i.e. how the
  runtime proves the pool words readable before trusting seeds. It gates
  **neither FNAME_Add nor FNAME_Find at runtime** — once armed, the thunk
  (:958-967) contains no admission, oracle, or check; it is a bare indirect
  call into the engine.
- Refused at INIT: `seed_refused` **throws** `"[Seeds] REFUTED: … Terminal
  for this launch…"` (:734-740); partial seed table throws the named
  SHAPE-A refusal at :1214-1223. Nothing binds in either case.
- What runtime refusals RETURN:
  - `invoke` !ok (ctor unbound / mode>1 / bad UTF16): **throw** via
    seed_refused — would surface as a "[Seeds] REFUTED" Lua error. **Not
    observed** in the failing sessions → the ctor call itself succeeded.
  - Engine FNAME_Find miss: **silent invalid FName** {0,0} — no error, no
    log, and `NativeFNameAdapter.hpp:48` sets `ok=true` unconditionally
    after the call, so the miss is invisible to the thunk.
  - `construct_with_string` with ctor unready (pre-bind only): **silent
    default FName** (NameTypes.hpp:241).
  - Then LuaUObject.cpp:2314 **silently skips** GetFunctionByNameInChain →
    invalid object → the observed __call error. The whole miss chain is
    silent-null.
- **GATE-1 (copyin short-writes / 0x7ffdfffe sentinel) is NOT implicated**:
  it lives in the init admission oracle
  (`bridge_kernel_write_copyin_probe_until`, `resolve_listing_gap` in
  `SeedValidation.hpp:503-539`). The failing session (variant A + cache-on)
  passed admission and logged
  `[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified
  (ByteProperty probe id == pool id 0x3)` (harvest log, 18:36:26).

## 3. Comparison-index duality

- This target is **not case-preserving**: `units/FNameAbiProfile.hpp:5-7`
  (`kOutputBytes=8, kCasePreserving=false`). There is no DisplayIndex split
  (`GetDisplayIndexFast` returns ComparisonIndex, NameTypes.hpp:371-378).
  The real duality on this build is **index-vs-Number inside the 8-byte
  form**: `operator==` compares the full `ToUnstableInt()` (index |
  Number<<32), `Equals` compares only the index.
- **Yes, a correctly-found name can fail the (b) equality while passing the
  (a) equality** — if the constructed FName's Number were nonzero. The
  binary settles that this does NOT happen with the current binding: the
  bound outer ctor 0x23d600 re-normalizes the result
  (`0x23d83f-0x23d845: shl r14,0x20 / mov eax,eax / or rax,r14` — r14 = 0
  for suffix-less names), so the constructed FName is {id, 0}, the same
  internal form as stored engine names. **Trap for future work:** the
  interning core 0x242470's own 64-bit return is `id | (id<<32)` (0x242555-0x242566:
  `mov ecx,eax / shl rcx,0x20 / mov eax,eax / or rax,rcx`) — high half
  garbage, normalized only by the outer overloads. Binding the core instead
  of 0x23d590 would make **every** `operator==` fail while `Equals` still
  passes — the exact (a)-works/(b)-fails signature. Note the startup probe
  never actually verified Number ("number=0" is a hardcoded literal in the
  log format at l25 initializer :1132), so this failure mode is ruled out by
  disassembly only.
- Id space (binary + rc_seed agreement): shard hash entries pack
  hash-frag(3 bits) | id(29 bits); id = block<<16 | offset with **stride 2**
  (0x23c3a0 `bextr edx,eax,0xd10` = (id>>16)&0x1FFF block; `movzx eax,ax` =
  id&0xFFFF; entry = Blocks[block]+offset*2). Matches
  `SeedValidation.hpp:61-76` kPoolLayout (CurrentBlock +0x8,
  CurrentByteOffset +0xC, Blocks +0x10, 16/8192). ByteProperty id=0x3 =
  block 0, offset 3 (immediately after "None") — console-verified.
- Comparators actually used: (a) `FName::Equals` (NameTypes.hpp:342-348);
  (b) `FName::operator==` (NameTypes.hpp:322-325). The UFunction path is
  strictly stricter.

## 4. Binary ground truth (decrypted eboot, capstone)

Full ctor chain decoded (code vaddr +0x4000 = file offset):

- **0x23d590** `FName(const WIDECHAR*, EFindName)`: RDI=sret out, RSI=name,
  **RDX=mode forwarded untouched**; NUL-walk length; wide flag is
  content-based (any char > 0x7F); `call 0x23d600`; `mov [rbx],rax` full
  8-byte store. (Verified byte-level: `e8 10000000` at 0x23d5eb → 0x23d600.)
- **0x23d600** with-length wide: `_NN` suffix parse (r14=Number), SIMD
  UTF16→ANSI collapse for ASCII, calls core, re-merges {id, Number}.
- **0x23d8a0 / 0x23d9b0**: ANSI overloads, same core.
- **0x242470** core: lazy pool init (flag @0x9365ea0 → `FNamePool::Init`
  0x23a5d0, which interns "None","ByteProperty","IntProperty"… and stores
  their ids at pool+0x14040/0x14044); `len>=0x400` + Find → **{0,0}
  silently**; `edx==1` (FNAME_Add) → **0x23c0b0 Store** (write-lock, probe
  table, insert on miss, returns (block<<16|offset) handle or existing id);
  `edx==0/2` (Find/Replace) → hash-prep 0x239b00 (ANSI) / 0x23a2c0 (WIDE)
  → **0x23c340 shard-Find** (read-lock; compares hash-fragment 0xe0000000,
  expected u16 header `len<<6 | probehash<<1`, then full string via
  0x8cdb0/0x8ce20; hit → id (0x1fffffff mask), **miss → 0 silently**).
- Pool singleton linked 0x9365EC0 / runtime 0x9765EC0; **256 lock-shards of
  64 B at pool+0x10040** (shard = hash-derived; shard {lock@0, mask@0xC,
  hashes@0x10, pool-backptr@0x18}); classic allocator head behind it. The
  engine's own id-validity getter at 0x23d570 is `(id>>16) <= CurrentBlock+1`
  — the R1 predicate SeedValidation documented.
- **Correction to FNAME-VERDICT.md section 4 step 3**: it swaps the two
  callees ("FNAME_Find: call 0x23c0b0 … add path: call 0x23c340"). Actual
  dispatch: 0x2424ee-0x2424fe (call 0x23c0b0) executes when `edx==1` =
  **Add**; 0x24250b-0x242550 (call 0x23c340) when `edx!=1` = **Find** (0x23c0b0
  writes entries and takes the write lock; 0x23c340 is read-only). No
  console conclusion depends on the swap (the init probe used Find through
  the real ctor; both call sites exist either way), but future RE should use
  the corrected labels.
- Does the shipping eboot's FName::Find get called by our path? Yes, by
  construction: the port calls the engine function directly at the bound
  address (no xrefs needed at runtime). Proven-ness per mode:
  - FNAME_Find of an **Init-registered** name: proven at init (ByteProperty
    id 0x3 == pool id).
  - FNAME_Add's probe of an **asset/BP** name: proven at runtime
    (FindFirstOf matched the class's stored index — Store's probe found the
    existing entry, so the engine's hash lookup demonstrably finds
    asset-registered names from our thread).
  - **FNAME_Find of an asset/BP name at runtime: never proven on console —
    this is exactly link (b) step 2.**
- The names exist in this build: ASCII literals
  "GetItemIDFromDeprecatedEnum" @ file 0x7bce269 (inside an FName-literal
  blob next to b'DisablePickupInteraction', b'IsInventoryItemIDEquippable',
  …) and "GetInteractablePickupIDActorPath" @ 0x7c14f47;
  "ItemDatabaseSubsystem" as UTF-16 @ 0x77a101e / 0x77f3f56 (BP-class
  references). Build drift (function renamed/removed) is unlikely.

## 5. VERDICT

The (a)-vs-(b) asymmetry is not one bug, it is two silent failure links that
only path (b) has, either of which produces the exact observed console error
with zero log output between LuaUObject.hpp:821 and the invalid-object push:

1. **Most likely: the runtime `FNAME_Find("GetItemIDFromDeprecatedEnum")`
   returned NAME_None and the LuaUObject.cpp:2314 guard silently skipped
   `GetFunctionByNameInChain`.** Rationale: every other link of path (b) is
   either proven on console (ctor bound+verified; same-thread Add probe
   finds BP names; string marshaling verified correct) or calibrated
   (NamePrivate 24 / Next 40 / Children 72 / ClassCastFlags 216 —
   CALIBRATION.json MATCH); `invoke` cannot report a miss (ok=true
   unconditionally), so a miss is invisible — matching an error signature
   with no diagnostics. The unproven primitive is precisely "runtime Find of
   a non-init name". A residual sub-cause for a genuine miss (name not yet
   interned at call time) cannot be excluded statically.
2. **Second: the Children-chain traversal found no match** — offsets are
   calibrated but the walk + cast-flag filter has never been exercised on
   live console data; zzdiag v2's class-children probe crash is weak
   corroboration of traversal fragility.
3. Ruled out as primary: Number/index-form mismatch (the bound ctor
   normalizes {id,0}; section 3 documents the core-return trap that would
   cause it if anyone re-binds the core); GATE-1/admission (init-only,
   passed on the failing session); Lua key marshaling (stack-shift is
   correct by design).

**What the fix must change** (ordered):

1. **Observability first** — one launch, no behavior change: in
   `handle_unreal_property_value`'s failure branch log (i)
   `property_name` index+Number, (ii) whether the :2314 guard skipped, (iii)
   the visited-field count from `GetFunctionByNameInChain`. One console run
   then discriminates miss-vs-traversal definitively.
2. **Make resolution use the proven primitives**: construct the member name
   with **FNAME_Add** (the mode FindFirstOf already proves at runtime;
   Add's probe is exactly the engine's hash lookup and inserts on miss
   instead of failing), and compare with **`Equals` (index-only)** in
   `GetFunctionByName(InChain)` — removing both the miss-fatality and the
   number-form failure class in one change. (Upstream parity note: the
   mod-facing behavior is unchanged; Equals is what `IsEqual(IgnoreCase)`
   and FindFirstOf already use.)
3. **String-based scan fallback** if a lookup still misses: iterate the
   Children chain comparing `GetNamePrivate().ToString()` case-insensitively
   — the same fallback shape the codebase already uses in
   `FUObjectHashTables::StaticFindObject` (port/unreal/src/UObjectHashTables.cpp:129-145)
   — immune to every index-form issue at bounded cost.
4. **Do NOT touch** the Seeds admission machinery, the slot-2 ctor binding,
   or GATE-1 — they are init-time only and passed on the failing session.

— agent-CB, 2026-10-08. Binary decode done against the sha-pinned decrypted
eboot; all file:line citations from the current worktree.

# RUNG3-RECIPE — harmless reflected access: definition, read path, probe, verification

Worker: worker-rung3-reflection-prep, 2026-10-04. Proposal-only. Companion to
`worker-acceptance-evidence/EVIDENCE-SPEC.md` section 3 (RUNG 3); this recipe
refines the reflected-access subset for the composition-5 runtime state and
adds exact failure signatures. Every claim is traced to source with file:line
under `PORT = .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port`.

---

## 1. Definition — what counts as rung 3 for THIS runtime

Runtime identity (validated): Townfall on UE **5.6.1-0+++UE5**, build
`04bb4330a7d71dcefe1cc2c4d19769d5`, **SHIPPING** (NonShipping=0), STATS off —
promoted pair identity header
(`worker-emit-final-pair/emission-final/MemberVariableLayout.ini` `[NativeLayout]`),
agreed by the resolved scan (`PublishStagedNativeLayoutMaps` refuses any
disagreement, `PORT/ue4ss/src/NativeLayoutBinding.cpp:175-182`).

**Rung 3 = harmless reflected access**, defined as ALL of:

1. **Resolve**: a Lua-originated query resolves a real engine object through
   live reflection data — `FindFirstOf` (GUA iteration + class-FName compare)
   and/or `StaticFindObject` — and returns a non-invalid handle.
2. **Read**: zero or more **read-only** property accesses via
   `GetPropertyValue` (a guarded `base + Offset_Internal` read through the
   registered pusher). No `SetPropertyValue`, no `CallFunction`
   (except the implicit FName find-or-add, section 4), no spawn, no UI, no
   console command.
3. **Harmless termination**: the query ends in a *recorded verdict* — value,
   invalid handle, or a caught/typed refusal — with the game process alive
   (C3 still able to reach a terminal state later;
   EVIDENCE-SPEC 3.2 item 2).

Explicitly OUT of rung 3 (EVIDENCE-SPEC :325): any reflected write, spawn, or
property set performed "to prove access works". Also out: `GetFullName` /
`FName::ToString` in the FIRST probe — those route through the engine-owned
`Conv_NameToString` fallback (`PORT/unreal/src/NameTypes.cpp:68-99`) which
executes `ProcessEvent` on the UKismetStringLibrary CDO (an engine function
call, heavier than a read). Name-to-string is a later, separately-evidenced
step, not the minimal probe.

## 2. The reflection READ path (traced, file:line)

### 2.1 Find by class name (Lua global)

| Step | Where |
|---|---|
| Lua global `FindFirstOf` registered | `PORT/ue4ss/src/Mod/LuaMod.cpp:1632` (body 1637-1659); `StaticFindObject` at `:1529`; `FindAllOf` at `:1666` |
| String overload → `FName(ClassName)` | `PORT/unreal/src/UObjectGlobals.cpp:481-484`; ctor default **FNAME_Add** `PORT/unreal/include/Unreal/NameTypes.hpp:307`; dispatch through resolved `ConstructorInternal` function pointer `:228`, `construct_with_string` `:239-255` |
| Iteration (non-hash path) | `UObjectGlobals.cpp:454-476`: `ForEachUObject` over `GUObjectArray->GetObjObjects()` (`:750-765`) — the same accessor family rung 2 already exercised live (`GetNumElements`, `PORT/unreal/src/UObjectArray.cpp:394-396`) |
| Class match | `Object->GetClassPrivate()` (`UObjectGlobals.cpp:455`) then `Class->GetNamePrivate().Equals(ClassName)` (`:457`) — **index-only compare, no name-pool string read** (`NameTypes.hpp:341-348`) |
| Super-struct fallback | `TSuperStructRange` walk `:466-473` (needs UStruct::SuperStruct staged — NOT in the promoted pair; skipped members stay refused, `worker-staged-pair-live/RUNG3-REQUIRED-READS.md:73-75`) |
| Hash path guard | `ShouldUseHashTables()` = `!bForceGUObjectArrayForIteration && FUObjectHashTables::IsAvailable()` (`PORT/unreal/src/UnrealInitializer.cpp:108-111`); staged settings set `bUseUObjectArrayCache=false` (`worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini:10`) |
| C++ helper (mod-free) | `PropertyMacros.hpp` Get/Set use `GetPropertyByNameInChain` + cached `GetOffset_Internal` (`PORT/unreal/include/Unreal/PropertyMacros.hpp:97-124`) — same accessors, Lua path shown above is what our mod uses |

### 2.2 Property read (Lua `GetPropertyValue`)

| Step | Where |
|---|---|
| Registration | `PORT/ue4ss/include/LuaType/LuaUObject.hpp:620-623` → `prepare_to_handle(Operation::Get)` `:791-840` |
| nullptr-object guard | `:800-819` — invalid/empty object constructs an empty handle, never dereferences |
| Property lookup | custom list `:822`, else `object->GetClassPrivate()->FindProperty(FName(name, FNAME_Find))` `:821,:834-836` |
| Chain walk | `UStruct::FindProperty` `PORT/unreal/src/UStruct.cpp:155-168`: `TFieldRange<FProperty>` (ChildProperties/Children chains, IncludeSuper\|IncludeDeprecated), `Property->GetFName() == PropertyName` (index compare); result cached host-side by (ClassPtr, PropertyName) |
| Function-vs-field split | `PORT/ue4ss/src/LuaType/LuaUObject.cpp:2303-2336` — a null field falls through to `GetFunctionByNameInChain` and, if that is also null, constructs an **invalid handle** (`:2328-2335`) — the designed "property not found" refusal |
| Offset resolution | `data = base + property->GetOffset_Internal()` `LuaUObject.cpp:2348`; `Offset_Internal` is read from the LIVE FProperty object at the offset given by the staged `FProperty::Offset_Internal` map (generated accessor, width 4 per profile `worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json:423`) |
| Pusher dispatch | property-class FName → registered pusher `LuaUObject.cpp:2343-2351`; `IntProperty` → `push_intproperty` (`UE4SSProgram.cpp:1708` registration; `PORT/ue4ss/src/LuaType/LuaUObject.cpp:598-602`) → `push_integer<int32_t>` (`LuaUObject.hpp:975-1001`): null-data refusal `:978-981`, integer push `:987` |

### 2.3 Who parses the layout INIs, and when

`UE4SSProgram::setup_unreal()` → `load_unreal_offsets_from_file()`
(`PORT/ue4ss/src/UE4SSProgram.cpp:936 → 896-928`). On `__PROSPERO__`:
`mod_source->read("ue4ss/MemberVariableLayout.ini")` +
`"ue4ss/VTableLayout.ini"`, **both required** — either missing throws
`"Native activation requires both member and vtable layout files"` and startup
hard-stops BEFORE the engine scan (`:899-904`); bytes go to
`StageNativeLayoutTexts` (`:905`). File variant (non-Prospero / tests):
`StageNativeLayoutFiles` reads both as regular files ≤ 8 MiB
(`NativeLayoutBinding.cpp:44-59,131-135`). Parse → validate → stage:
`StageNativeLayoutBundle` (`:139-164`). Publish into the live accessor maps
only after engine axes resolve (`SetResolvedNativeLayoutAxes` →
`PublishStagedNativeLayoutMaps`, `:165-199`, one-shot swap `:193-198`).
Startup-order receipt: `worker-activation-bundle/LAYOUT-GATE.md:12-37`.

## 3. The guard that makes a bad offset REFUSE, not crash

Four layers, in order:

1. **Staging width gate (the width-table gate).** Every non-bitfield ini
   member (except `UEP_TotalSize`) requires a compiled accessor width from
   `GetAccessorWidth`; missing/zero width throws
   `"Native accessor width unavailable"` and the ENTIRE bundle is refused
   before engine init (`NativeLayoutBinding.cpp:87-92`). Per-class bounds:
   positive `UEP_TotalSize`, every offset in `[0, total)`, width within the
   declared size (`:105-116`). Identity: `format==1`,
   `target=="prospero-x86_64-sysv"`, `pointer_size==8`, engine_major ≥ 4,
   non-empty revision/build (`:143-146`). One staging per loaded lifetime
   (`:161`). Width-table provenance: `NativeLayoutWidths.hpp` is supplied by
   the composition build (not in this worktree); its content is the 419-entry
   compiled-port contract (304 available) — profile
   `:405-500`.
2. **Publication gate.** `PublishStagedNativeLayoutMaps` refuses unresolved
   axes and any disagreement between staged identity and resolved engine
   facts (`NativeLayoutBinding.cpp:175-182`); a wrong-build pair can never
   reach the accessor maps.
3. **Access-time missing-key gate.** Generated member accessors throw
   `std::runtime_error{"Tried getting member variable '<Class>::<Key>' that
   doesn't exist in this engine version."}` when the staged map has no entry —
   e.g. `UObjectBase::GetClassPrivate`
   (`generated_include/MemberVariableLayout_SrcWrapper_UObjectBase.hpp:42-56`),
   `FProperty::GetArrayDim`
   (`SrcWrapper_FProperty.hpp:3-12`). With the promoted 17-key pair
   (FUObjectArray 10 + TUObjectArray 7 only —
   `worker-emit-final-pair/emission-final/MemberVariableLayout.ini`), any
   rung-3 accessor call refuses loudly instead of reading offset 0.
4. **Pusher null gate.** `push_integer` refuses a null data pointer
   (`LuaUObject.hpp:978-981`); a property the chain cannot find returns an
   invalid handle, not an error (`LuaUObject.cpp:2328-2335`).

Consequence: under the CURRENT promoted pair the probe's engine-touching
calls fail closed. A rung-3 attempt before the extended member pair lands is
SAFE but can only produce refusal signatures (section 7) — it cannot crash via
a bad offset, because a bad offset cannot be staged (layer 1) nor published
(layer 2), and a missing offset refuses at access (layer 3).

## 4. The minimal probe (one expression) and its class-name evidence

**Probe (find step, primary):**

```lua
local g = FindFirstOf("GameEngine")
```

* `FindFirstOf` never returns Lua `nil` — `auto_construct_object` is
  nullptr-safe (`LuaMod.cpp:1650-1654`), so check validity explicitly.
* Needs exactly: the FC-1-resolved FName string ctor (find-or-add of an
  already-present name = pure find), the rung-2 GUA maps (already staged +
  published), and `UObjectBase::ClassPrivate` + `NamePrivate` offsets from the
  EXTENDED pair. `FName::Equals` is index-only (`NameTypes.hpp:345-348`) — no
  FName-pool iteration, no string materialization.
* The find-or-add hits the exact FC-1 candidate form (ctor default
  `FNAME_Add`, `NameTypes.hpp:307`; FC1-B's `mov edx,1`,
  EVIDENCE-SPEC :266-269). For "GameEngine" — a name that already exists —
  add degrades to find: no new name-table entry is created.

**Read-only verification (same line family, still minimal):**

```lua
local ok  = g:IsValid()                                  -- LuaUObject.hpp:761-774
local twb = (g == FindFirstOf("GameEngine"))             -- userdata equality, LuaUObject.cpp:2280-2300
```

* `IsValid` is read-only: host-side listener map + `IsUnreachable()` (an
  `ObjectFlags` read; width 4 available, profile `:412`). Caveat: the
  listener-map membership test can be false for objects found through the
  non-caching path — that is why `twb` (two finds agree on the same remote
  pointer) is the path-independent determinism signal.

**Why "GameEngine" and only "GameEngine":** searched the checkpoint + worker
dirs per instruction — `ingame-v1/v2/v3` facts records contain NO object class
names (module-list names are empty strings; experiments g1-g7/v1-v5 cover
vmmap/module-list/layout predicates only); `anchor-validation`,
`c2-alternative`, `igf6` results validate array-header words, not objects
(`worker-anchor-validation/REPORT.md:75`: "gated 96 B at 0x97ba110"). The only
evidenced class is `UGameEngine`: serialized-ELF symbol survey
`ps_ugameengine_tick_file_image` vaddr `0x2fac8c0` (profile
`GameEngineTick.gameengine_tick`, validated-static) + rung-1 acceptance (the
genuine game-thread Tick route fires inside `UGameEngine::Tick`;
`worker-query-harness/REPORT.md:190`). The UClass short name of `UGameEngine`
is `GameEngine`; the engine instance exists whenever the engine ticks (rung 1
proves it ticks). Any other class name would be unevidenced guessing.

**Property read (step 2 — sequenced, NOT the first probe):**

```lua
local v = g:GetPropertyValue("<R4-validated-int-property-name>")
```

Blocked until (a) the extended pair stages UStruct/FField/FProperty (R4/R5
land Validated), and (b) `FField::Next` / `FField::NamePrivate` have compiled
widths — currently width=null in the 419-entry contract (profile `:447-455`,
reason "getters unavailable/private"), and widthless members stay skipped
(`RUNG3-REQUIRED-READS.md:73-75`). No property name for this build is
validated yet; supplying one today would be a guess. The mechanism is fully
traced (section 2.2) and its refusal behavior is designed (missing property →
invalid handle; missing layout key → typed throw).

## 5. Carrier — least invasive

New additive Lua mod, staged via the existing FTP ritual next to the pinned
mod (which must NOT change — rung 4 pins `main.lua`
sha256 `f586043a…658`, `worker-settings-bundle-merge/STAGING-COMMANDS.md:22`):

```
ue4ss/Mods/Rung3Probe/enabled.txt        (0-byte marker)
ue4ss/Mods/Rung3Probe/Scripts/main.lua   (probe below)
```

Mod `print()` lines route into the C1 runtime log like the existing mod's
channel (EVIDENCE-SPEC 0.1 fifth channel, 4.1). No binary changes, no settings
changes, no changes to the TownfallItemSpawner tree.

```lua
-- Rung3Probe/Scripts/main.lua  (runs after on_engine_initialized -> mods start)
local MOD_TAG = "[Rung3Probe]"

local function probe()
    local g = FindFirstOf("GameEngine")          -- reflected resolve (read-only)
    local valid = g:IsValid()                    -- read-only membership + ObjectFlags
    local twin  = FindFirstOf("GameEngine")
    local stable = (g == twin)                   -- determinism of the reflected query
    print(MOD_TAG .. " find GameEngine valid=" .. tostring(valid)
        .. " stable=" .. tostring(stable) .. "\n")
    -- NO SetPropertyValue, NO CallFunction, NO GetFullName in this probe.
end

-- defer to the first safe point: ExecuteInGameThread if published, else pcall
-- at mod start is acceptable because every call is read-only and the runtime
-- gates dispatch by game-thread identity (refusals are recorded, not fatal).
local ok, err = pcall(probe)
if not ok then print(MOD_TAG .. " probe error: " .. tostring(err) .. "\n") end
```

## 6. Expected observables, pass criteria

Channel C1 = `/data/townfall_runtime_aligned_entry.log` (current lineage;
path-agnostic oracle per EVIDENCE-SPEC 4.1). C2 =
`/data/modbridge-ingame-facts.jsonl`. C3 = `runtime-status.txt`.

**PASS (rung 3, find step) — all from ONE launch:**

1. C1 contains exactly one `find GameEngine valid=true stable=true` line
   (mod tag `[Rung3Probe]`), written after `mods-started`.
2. FC-1 disambiguator in the SAME launch: FC1-A byte equality +
   FC1-B all 9 windows + FC1-C fail-closed agreement, as C2 records under the
   probe build_id (EVIDENCE-SPEC 3.1-3.2) — this is what separates a genuine
   resolve from the silent-FName no-op (section 7, F1).
3. Harmlessness: no crash/hang — C3 reaches `stopped` later in the bounded
   window; game continues; the Item Spawner mod's own rung-4 lines still
   appear if staged.
4. Pre-flight: C1 shows the `Using engine version: 5.6` scan line and the
   staged-pair load line (`MemberVariableLayout.ini loaded`,
   `UE4SSProgram.cpp:940`), C3 shows `activated` → `engine-ready` →
   `mods-started`.

**PASS (read step, only when sequenced after R4/R5):** additionally one
`prop <name> = <int>` line whose value round-trips stable across two reads on
distinct ticks, with the property name taken from the R4-validated property
list for this build — never guessed.

**Evidence discipline:** fresh mtimes, one launch, records born in this
run's C1/C2/C3; no re-rating unchanged evidence; host tests never advance the
rung (EVIDENCE-SPEC 0.2, appendix).

## 7. Failure signatures (each discriminates a specific root cause)

| Sig | Observable (C1 unless noted) | Root cause | Layer |
|---|---|---|---|
| F1 | `find GameEngine valid=false stable=true` AND FC-1 records absent/refuted | FName ctor unresolved → `construct_with_string` silently no-ops, search compared against FName "None" (`NameTypes.hpp:241`) — harmless by construction, NOT evidence of absence of GameEngine | reflection resolve |
| F2 | `find GameEngine valid=false` with FC-1 records PASSING | genuine negative: no GameEngine-named class object found via GUA iteration — would contradict rung-1 evidence; treat as mapping/identity question, do NOT re-roll | reflection resolve |
| F3 | C1 Lua error `...Cannot call Lua function: GetPropertyValue` / `nil value (global 'FindFirstOf')` | composed runtime lacks the query API registration (`LuaMod.cpp:1632`) — B2-unresolved discriminator per EVIDENCE-SPEC 3.3 | API surface |
| F4 | C1 runtime exception text `Tried getting member variable 'UObjectBase::ClassPrivate' ...` | extended pair not staged/published — the designed missing-key refusal (layer 3). Expected if attempted with today's 17-key pair; sequencer bug, not a crash | layout gate |
| F5 | Startup hard stop, C3 never reaches `activated`; C1/ploader shows `Native activation requires both member and vtable layout files` | pair missing from mod root (`UE4SSProgram.cpp:899-904`) — pre-rung-2 state, staging error | staging |
| F6 | Startup throw `Invalid native layout identity` / `Native accessor width unavailable` / `Native member is outside declared class size` | malformed or widthless entry in a NEW extended pair (`NativeLayoutBinding.cpp:143-146,87-92,110-114`) — the width-table gate refusing a bad pair BEFORE engine init | staging width gate |
| F7 | Startup/scan throw `Native layout declarations disagree with resolved engine facts` | staged identity ≠ resolved scan (wrong build/axes pair) (`NativeLayoutBinding.cpp:175-182`) | publication gate |
| F8 | `refused_layout_gate_closed` climbing in C2 counter snapshots, no record | publication never armed (SEAM S2 GAP state, EVIDENCE-SPEC 2.3) | publication gate |
| F9 | C1 Lua error `Tried accessing unreal property without a registered handler. Property type '<X>' not supported.` | property found but its type pusher unregistered (`LuaUObject.cpp:2353-2360`) — use an IntProperty-typed target for the read step | pusher table |
| F10 | `find GameEngine valid=false stable=false` (two finds differ) | GUA mutated between finds or identity/iteration defect — refuses the determinism criterion; investigate, never re-roll | reflection resolve |
| F11 | Process death / C3 never terminal after the probe line | ANY crash here is a rung-3 FAIL and a console-safety event — under the staged gates this signature should be UNREACHABLE (layers 1-3); its occurrence means a guard was bypassed, stop and reconcile | integrity |

## 8. Sequencing (binding)

1. Rungs 1-2 hold (done, composition-5 launch).
2. batch2 live reads R1-R5 land Validated (`worker-staged-pair-live/RUNG3-REQUIRED-READS.md`)
   → extended member pair emitted (UObjectBase minimum for the find step;
   UStruct/UClass/FField/FProperty for the read step), ParsePair-verified.
3. FC-1 resolution wired into `FName::ConstructorInternal` with the
   `ig_run_fc1_probe` add-on staged for same-launch C2 records.
4. Stage `Rung3Probe` mod + extended pair; ONE launch per the ritual;
   collect per section 6; only then rate.

## 9. Non-evidence

- Host-side everything: fixture loads, ParsePair self-tests, width-table unit
  tests, Lua VM host compositions — delivery evidence only.
- A `valid=true` line without same-launch FC-1 records (F1 ambiguity).
- GetFullName/ToString output (engine call, not in the minimal probe).
- Any write/spawn/UI performed "to prove access" (EVIDENCE-SPEC 3.5).
- Frozen artifacts re-presented as a live run.

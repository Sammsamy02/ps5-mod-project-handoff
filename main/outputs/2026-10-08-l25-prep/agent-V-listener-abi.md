# Agent V — FUObjectDeleteListener Itanium-ABI ground truth (UE 5.6.1 vs our runtime)

Read-only research. No source files were modified. Purpose: settle the suspected
vtable/ABI mismatch behind the init stall and save-load crash in the PS5 UE4SS
runtime port.

## 0. Engine version confirmed

- **UE 5.6.1** — vendored official snapshot
  `outputs/2026-10-08-l23-layout-generic/official-source/Engine/Build/Build.version`:
  `MajorVersion 5, MinorVersion 6, PatchVersion 1, CompatibleChangelist 43139311, BranchName "UE5"`.
- The snapshot is pinned at Epic commit `6978b63c8951e57d97048d8424a0bebd637dde1d`
  (`outputs/2026-10-08-l23-layout-generic/REPORT.md:18`), and its layouts already
  matched the game image on the 93-fact calibration (84 matches), corroborating the
  game runs a 5.6.1-lineage engine.

All paths below are relative to `/WORKSPACE/.codex/worktrees/translation-core/`.

## 1. Engine-side declaration (ground truth)

File: `outputs/2026-10-08-l23-layout-generic/official-source/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h`

```cpp
929: class FUObjectDeleteListener
930: {
931: public:
932:     virtual ~FUObjectDeleteListener() {}                                              // dtor, non-pure, inline
940:     virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index)=0; // pure
945:     virtual void OnUObjectArrayShutdown() = 0;                                        // pure
950:     virtual SIZE_T GetAllocatedSize() const                                           // non-pure, inline { return 0; }
951:     {
952:         return 0;
953:     }
954: };
```

No base classes. Four virtual functions declared in this order.

### 1a. Engine vtable, Itanium C++ ABI (PS5 clang, x86_64-sie-ps5)

Itanium layout: `[offset-to-top][RTTI ptr][address point -> slots]`, virtuals in
declaration order, destructor occupying two consecutive slots (complete-object D1
first, deleting D0 second).

| Slot | Function | Signature (this + params -> ret) | Engine mangled name |
|---|---|---|---|
| 0 | `~FUObjectDeleteListener` (D1, complete) | `(FUObjectDeleteListener*) -> void` | `_ZN22FUObjectDeleteListenerD1Ev` |
| 1 | `~FUObjectDeleteListener` (D0, deleting) | `(FUObjectDeleteListener*) -> void` | `_ZN22FUObjectDeleteListenerD0Ev` |
| 2 | `NotifyUObjectDeleted` | `(const UObjectBase*, int32) -> void`, non-const | `_ZN22FUObjectDeleteListener20NotifyUObjectDeletedEPK11UObjectBasei` |
| 3 | `OnUObjectArrayShutdown` | `() -> void`, non-const | `_ZN22FUObjectDeleteListener22OnUObjectArrayShutdownEv` |
| 4 | `GetAllocatedSize` | `() -> SIZE_T` (size_t, 8 bytes), **const** | `_ZNK22FUObjectDeleteListener16GetAllocatedSizeEv` |

`SIZE_T` == `size_t` == unsigned 64-bit on the PS5 target (l23 PLATFORM-AUDIT:
pointers 8 bytes). `int32` == `int` (32-bit signed).

## 2. Our runtime's declarations

### 2.0 Which trees were checked

- `outputs/2026-10-08-l25-runtime/typescan-r4/` (include/, candidate/src/) contains
  **no listener code at all** — only the typescan units (NativeTypeScanPolicy,
  Property scans, UnrealInitializer scans). The listener lives in the lineage port
  root that the l24/l25 builds compile against, pinned by
  `outputs/2026-10-08-l24-build-r2/SOURCE-SELECTION.json` →
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`.
- The l22 diagnostics edit (`outputs/2026-10-08-l22-runtime/diagnostics/`) carries an
  identical listener and is cited below as the in-lineage copy.

### 2.1 Our base class — `RC::Unreal::FUObjectDeleteListener`

File: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectArray.hpp`

```cpp
29: class RC_UE_API FUObjectDeleteListener
30: {
31: public:
32:     virtual ~FUObjectDeleteListener() = default;
33:     virtual void NotifyUObjectDeleted(const class UObjectBase* Object, int32 Index) = 0;
34:     virtual void OnUObjectArrayShutdown() = 0;
35: };
```

(The header's own comment at lines 18-20 claims the struct "hasn't changed from
4.12" — that claim is false for 5.6: the engine added `GetAllocatedSize`.)
Identical text exists in every other port copy (ue4ss-unreal-source-access-1,
ue4ss-unreal-native-build-1, ue4ss-native-unreal-properties-1, composition-1, ...).

### 2.2 Our derived listeners (three)

a) `RC::Unreal::FClassDeleteListener` — `port/unreal/include/Unreal/ClassListener.hpp:30-36`
   (overrides `NotifyUObjectDeleted(const UObjectBase*, int32)`, `OnUObjectArrayShutdown()`).
b) `RC::Unreal::FShutdownDeleteListener` — `ClassListener.hpp:38-44` (same two overrides).
c) `RC::LuaType::FLuaObjectDeleteListener` — l22 in-lineage copy
   `outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp:32-42`
   (and port root `port/ue4ss/include/LuaType/LuaUObject.hpp:31-41`):

```cpp
struct FLuaObjectDeleteListener : public Unreal::FUObjectDeleteListener
{
    static FLuaObjectDeleteListener s_lua_object_delete_listener;   // data member, no layout effect

    void NotifyUObjectDeleted(const Unreal::UObjectBase* object, [[maybe_unused]] int32_t index) override;
    void OnUObjectArrayShutdown() override
    {
        Unreal::UObjectArray::RemoveUObjectDeleteListener(this);
    }
};
```

Implementation: `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp:92-96`
(`remove_from_global_unreal_objects_map(static_cast<const Unreal::UObject*>(object));`).

None of the three overrides `GetAllocatedSize` — and none can: **the base does not
declare it.** Every derived class introduces no new virtuals, so each derived
vtable is exactly the base layout plus nothing.

### 2.3 Our vtable, Itanium ABI (confirmed against real emitted symbols)

Emitted symbols from `outputs/2026-10-08-r5-thread-final-integration-v2/pass2/final.defined.nm`
and `outputs/2026-10-08-native-frame-audit/target-census.stdout`:

- `_ZTVN2RC7LuaType24FLuaObjectDeleteListenerE`, `_ZTVN2RC6Unreal20FClassDeleteListenerE`,
  `_ZTVN2RC6Unreal23FShutdownDeleteListenerE` — the three vtables;
- `...FLuaObjectDeleteListenerD0Ev`, `...20NotifyUObjectDeletedEPKNS_6Unreal11UObjectBaseEi`,
  `...22OnUObjectArrayShutdownEv` (+ same pair for FClassDeleteListener/FShutdownDeleteListener);
- base `_ZN2RC6Unreal22FUObjectDeleteListenerD2Ev` (D2/D1 alias) and `_ZTIN2RC6Unreal22FUObjectDeleteListenerE`;
- **no `GetAllocatedSize` symbol anywhere in our runtime** (rg over all nm dumps: 0 hits).

| Slot | Function | Our mangled name (derived FLuaObjectDeleteListener) |
|---|---|---|
| 0 | D1 complete dtor | `_ZN2RC7LuaType24FLuaObjectDeleteListenerD1Ev` |
| 1 | D0 deleting dtor | `_ZN2RC7LuaType24FLuaObjectDeleteListenerD0Ev` |
| 2 | `NotifyUObjectDeleted` | `_ZN2RC7LuaType24FLuaObjectDeleteListener20NotifyUObjectDeletedEPKNS_6Unreal11UObjectBaseEi` |
| 3 | `OnUObjectArrayShutdown` | `_ZN2RC7LuaType24FLuaObjectDeleteListener22OnUObjectArrayShutdownEv` |
| 4 | — | **ABSENT — vtable ends here** |

## 3. Slot-by-slot comparison and verdict

| Slot | Engine (UE 5.6.1) | Ours | Match |
|---|---|---|---|
| 0 | D1 `~FUObjectDeleteListener()` complete | D1 (implicit, non-pure base dtor) | YES — same this/ret, non-virtual thunk needed |
| 1 | D0 deleting dtor | D0 deleting dtor | YES |
| 2 | `void NotifyUObjectDeleted(const UObjectBase*, int32)` | `void NotifyUObjectDeleted(const Unreal::UObjectBase*, int32_t)` | YES — `int32`==`int32_t`==`int`; const pointer identical |
| 3 | `void OnUObjectArrayShutdown()` | `void OnUObjectArrayShutdown()` | YES |
| 4 | `SIZE_T GetAllocatedSize() const` | **does not exist** | **MISMATCH — truncated vtable** |

**Verdict: MISMATCH. Slots 0-3 are exact matches (order, signatures, constness,
return types). Slot 4 `GetAllocatedSize() const` exists only in the engine.**

### What the engine actually executes at slot 4

Engine call chain (all citations local):

- `FUObjectArray::GetDeleteListenersAllocatedSize()` — engine
  `UObjectArray.h:1465-1478` — iterates `UObjectDeleteListeners` and calls
  `Listener->GetAllocatedSize()` at **line 1471** → virtual dispatch `vptr[4]`.
- Caller: `LogHashMemoryOverheadStatistics(FOutputDevice&, EObjectMemoryOverheadOptions)`
  — engine `CoreUObject/Private/UObject/UObjectHash.cpp:2318`, call at **line 2416**
  (`GUObjectArray.GetDeleteListenersAllocatedSize(&NumListeners)`).
- `FUObjectArray::GetAllocatedSize()` (`UObjectArray.h:1449-1463`) also touches the
  same listener array under `ObjObjectsCritical` + `UObjectDeleteListenersCritical`.

With one of our listeners registered, `vptr[4]` is an **out-of-bounds read one word
past the end of our vtable object** in `.data.rel.ro`. The word that lands there is
whatever the linker placed after the vtable (typically the next vtable's
offset-to-top/RTTI pointer or another ro-data constant). The engine then performs an
indirect `call` on that value — a jump to a typeinfo pointer or a small integer
(often 0). Result: not a call to any of our functions; an uncontrolled branch —
near-certain crash/misbehavior in any engine path that accounts listener memory
(memory-stats dumps, memreport-style flows reachable during save-load bookkeeping).
Slots 0-3 dispatch correctly, which is why plain delete-notification works and the
failure is confined to slot-4 paths.

Caveat noted for completeness: RTTI names also differ (`_ZTSN2RC6Unreal22FUObjectDeleteListenerE`
vs engine `_ZTS22FUObjectDeleteListener`) but the engine only makes virtual calls
through the registered pointer — no dynamic_cast/catch across this boundary — so the
RTTI difference is harmless here.

### Fix shape (for the l26 fix, not applied here)

Add to the port base `RC::Unreal::FUObjectDeleteListener` (port UObjectArray.hpp:29-35):

```cpp
virtual size_t GetAllocatedSize() const { return 0; }
```

declared **after** `OnUObjectArrayShutdown` (never before — it must occupy slot 4),
then rebuild. Optionally override in derived listeners to report real heap usage.

## 4. GUObjectArray registration path (task item 5)

### Engine side

- Member: engine `UObjectArray.h:1432` — `TArray<FUObjectDeleteListener* > UObjectDeleteListeners;`
  (plain heap-allocated TArray of raw pointers; sibling create array at line 1429).
- Lock: engine `UObjectArray.h:1433-1435` —
  `#if THREADSAFE_UOBJECTS` → `mutable FTransactionallySafeCriticalSection UObjectDeleteListenersCritical;`
  `THREADSAFE_UOBJECTS` is hard-defined to 1 at
  `CoreUObject/Public/UObject/ObjectMacros.h:33` in this snapshot, so **the critical
  section exists in shipping PS5 builds**.
- `AddUObjectDeleteListener` / `RemoveUObjectDeleteListener` declarations:
  engine `UObjectArray.h:1133` / `1140` — `COREUOBJECT_API` (compiled into the engine
  binary; the implementing `UObjectArray.cpp` is NOT vendored locally — only
  `UObjectHash.cpp` is). Known upstream 5.6 shape, consistent with every inline user
  in the vendored header:
  `void FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener* Listener) { FScopeLock ListenersLock(&UObjectDeleteListenersCritical); UObjectDeleteListeners.Add(Listener); }`
  — i.e. **yes, it takes `UObjectDeleteListenersCritical`** (the same lock pattern is
  demonstrated inline at `UObjectArray.h:1456-1462` and `1467-1469`).
- Engine dispatch of `NotifyUObjectDeleted` lives in the non-vendored
  `UObjectArray.cpp` (`FUObjectArray::FreeUObjectIndex`-family) and iterates the array
  under the same critical section; shutdown dispatch iterates a *copy* of the array,
  so a listener removing itself inside `OnUObjectArrayShutdown` (what ours does) is safe.

### Our side (and why it matters for the stall)

Our registration does **not** call the engine's `AddUObjectDeleteListener` at all —
it writes the engine's live member array directly:

`port/unreal/src/UObjectArray.cpp:379-387`:

```cpp
void UObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener* Listener)
{
    auto& DeleteListeners = GUObjectArray->GetUObjectDeleteListeners();   // engine member via generated member-offset
    if (DeleteListeners.Contains(Listener)) { throw std::runtime_error{...}; }
    DeleteListeners.Append(TArray{Listener});                             // our TArray code, OUR allocator
}
```

`RemoveUObjectDeleteListener` (lines 389-392) is `RemoveSingleSwap` on the same
member, also unlocked.

Two engine-vs-us hazards on this path (beyond the slot-4 vtable bug):

1. **No lock**: engine writers/dispatchers hold `UObjectDeleteListenersCritical`
   (THREADSAFE_UOBJECTS=1); our unlocked `Contains`/`Append`/`RemoveSingleSwap` can
   race a concurrent engine dispatch (e.g. async-loading thread freeing a UObject)
   — a plausible contributor to the init-stall class of failure.
2. **Allocator mismatch**: growth of the engine-owned `TArray` runs through our
   runtime's allocator (`TSizedDefaultAllocator<32>` machinery,
   `ResizeTo`/`ResizeForCopy` symbols emitted in our module); the engine later
   mutating/freeing that array uses `FMemory` — mismatched heap → corruption/crash
   on save-load-style churn. The emitted
   `_ZN2RC6Unreal6TArrayIPNS0_22FUObjectDeleteListenerENS0_22TSizedDefaultAllocatorILi32EEEE...`
   symbols in our module confirm our container code is what reshapes the engine array.

Registration call sites in our runtime:

- `port/unreal/src/UnrealInitializer.cpp:425` `AddUObjectDeleteListener(&FClassDeleteListener::ClassDeleteListener)`
- `port/unreal/src/UnrealInitializer.cpp:429` `AddUObjectDeleteListener(&FShutdownDeleteListener::ShutdownDeleteListener)`
  (in-lineage copies: `outputs/2026-10-08-l22-runtime/profile/units/UnrealInitializer.cpp:522-527`)
- `port/ue4ss/src/Mod/LuaMod.cpp:6896` `AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener)`
  (in-lineage copy: `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp:7927`)

## 5. Summary

- Engine: UE **5.6.1** confirmed (Build.version + Epic commit pin + image calibration).
- Vtable verdict: slots 0-3 exact; **slot 4 `SIZE_T GetAllocatedSize() const`
  missing from our base** → engine virtual dispatch at `UObjectArray.h:1471` reads
  past our vtable and calls garbage. Fix = add that one virtual (in last position)
  to `RC::Unreal::FUObjectDeleteListener` and rebuild.
- Registration: engine member is `TArray<FUObjectDeleteListener*>`; the engine's
  `AddUObjectDeleteListener` locks `UObjectDeleteListenersCritical`
  (`FTransactionallySafeCriticalSection`, THREADSAFE_UOBJECTS=1). Our port bypasses
  both the engine function and the lock and grows the array with our own allocator —
  separate concurrency/allocator hazards to fix alongside the vtable repair.

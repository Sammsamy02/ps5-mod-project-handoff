# Agent Z8 — FUObjectDeleteListener blast-radius map + vtable layout proof

Read-only on all trees. Harness lives in `/tmp/l25-listener-layout/` (this
report is the only file written elsewhere). Companion to
`agent-V-listener-abi.md` (ground-truth declaration analysis); this document
adds the usage-site blast radius and a compiled, compiler-witnessed layout
proof of the one-line fix.

## 0. Scopes examined (pins verified against SOURCE-SELECTION.json)

Pinned port root (per `outputs/2026-10-08-l24-build-r2/SOURCE-SELECTION.json`,
sha256 verified MATCH for every file cited):

- `what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
  - `unreal/include/Unreal/UObjectArray.hpp` — sha `b01bf4ef…5544ce1`
  - `unreal/include/Unreal/ClassListener.hpp` — sha `94b4274a…d5347ff`
  - `unreal/src/UObjectArray.cpp`, `unreal/src/ClassListener.cpp`,
    `unreal/src/UnrealInitializer.cpp`
  - `ue4ss/include/LuaType/LuaUObject.hpp` — sha `89cb7390…8b401b82`
  - `ue4ss/src/LuaType/LuaUObject.cpp`, `ue4ss/src/Mod/LuaMod.cpp`

In-lineage l22 diagnostics copy
(`outputs/2026-10-08-l22-runtime/diagnostics/`, sha-verified in the same pin
file — note it is a *divergent newer revision* of LuaUObject/LuaMod, not
byte-identical to the port root):

- `include/LuaType/LuaUObject.hpp` — sha `1b6e2ab7…01783c35`
- `src/LuaUObject.cpp` — sha `d31f4e52…1c70c9a6`
- `src/LuaMod.cpp` (registration site)

Engine ground truth (vendored UE 5.6.1 official source; both l23 copies
byte-identical, sha `24d5bead…7fb29`):

- `outputs/2026-10-08-l23-layout-{generator,generic}/official-source/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h`

l24-build-r2 output tree was also swept for listener references: **none** (no
listener code is duplicated into that build's outputs).

## 1. TASK A — usage-site map

### 1.1 FUObjectDeleteListener (the base being fixed)

| # | file:line | kind | detail |
|---|---|---|---|
| 1 | port `unreal/include/Unreal/UObjectArray.hpp:29-35` | declaration | `class RC_UE_API FUObjectDeleteListener`; slots today: D1, D0, Notify[pure], Shutdown[pure] — 4 entries |
| 2 | port `unreal/include/Unreal/UObjectArray.hpp:115` | alias | `using FUObjectDeleteListener = Unreal::FUObjectDeleteListener;` inside `FUObjectArray` |
| 3 | port `unreal/include/Unreal/UObjectArray.hpp:127` | registration API decl | `static void AddUObjectDeleteListener(FUObjectDeleteListener*)` |
| 4 | port `unreal/include/Unreal/UObjectArray.hpp:128` | removal API decl | `static void RemoveUObjectDeleteListener(FUObjectDeleteListener*)` |
| 5 | port `unreal/src/UObjectArray.cpp:379-387` | registration impl | appends pointer into engine-owned `GUObjectArray->GetUObjectDeleteListeners()` TArray (duplicate ⇒ throw) |
| 6 | port `unreal/src/UObjectArray.cpp:389-393` | removal impl | `RemoveSingleSwap` on the engine TArray |
| 7 | port `unreal/include/Unreal/UObject.hpp:49` | friend decl | `friend class FUObjectDeleteListener;` in `UObjectBase` (access only, no layout coupling) |
| 8 | port `unreal/generated_include/MemberVariableLayout_HeaderWrapper_FUObjectArray.hpp:31-32` + `MemberVariableLayout_SrcWrapper_FUObjectArray.hpp:137-153` | member accessor | `GetUObjectDeleteListeners()` — generated **member-variable offset** accessor for the engine `FUObjectArray::UObjectDeleteListeners` TArray; pointer-typed elements, listener-class-layout-independent |

Registrations / removals through that API (all pass `&Concrete::instance`,
upcast to the base — no casts in user code):

| # | file:line | call |
|---|---|---|
| 9 | port `unreal/src/UnrealInitializer.cpp:425` | `AddUObjectDeleteListener(&FClassDeleteListener::ClassDeleteListener)` |
| 10 | port `unreal/src/UnrealInitializer.cpp:429` | `AddUObjectDeleteListener(&FShutdownDeleteListener::ShutdownDeleteListener)` |
| 11 | port `unreal/src/UnrealInitializer.cpp:125` | `RemoveUObjectDeleteListener(&FClassDeleteListener::ClassDeleteListener)` (cache retire, game thread only; comment: *"The engine notifies listeners from the game thread"*) |
| 12 | port `ue4ss/src/Mod/LuaMod.cpp:6896` (`on_program_start`) | `AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener)` |
| 13 | l22 `diagnostics/src/LuaMod.cpp:7927` (`on_program_start`) | same Lua registration (in-lineage copy, PROSPERO build) |

**Dispatch sites:** there are **zero** port-side invocations of
`NotifyUObjectDeleted`/`OnUObjectArrayShutdown` (grep over both trees finds
only declarations, definitions, and `override`s). All dispatch is
engine-side: the engine iterates its `UObjectDeleteListeners` array and calls
through the *engine-compiled* vtable layout. That is exactly why the port's
class layout must match the engine's declaration — including
`GetAllocatedSize`, which the engine calls on every registered listener from
`FUObjectArray::GetDeleteListenersAllocatedSize()` (vendored header lines
1465-1475: `for (FUObjectDeleteListener* Listener : UObjectDeleteListeners)
AllocatedSize += Listener->GetAllocatedSize();`). With the pre-fix port class,
that call indexes **slot 4 of a 4-entry vtable** — out-of-bounds.

### 1.2 FClassDeleteListener

| # | file:line | kind | detail |
|---|---|---|---|
| 14 | port `unreal/include/Unreal/ClassListener.hpp:30-36` | declaration | `struct FClassDeleteListener : FUObjectDeleteListener`; overrides Notify + Shutdown only; static instance `ClassDeleteListener`; no new virtuals, no data |
| 15 | port `unreal/src/ClassListener.cpp:13` | construction | static instance definition `FClassDeleteListener FClassDeleteListener::ClassDeleteListener{}` |
| 16 | port `unreal/src/ClassListener.cpp:85-119` | override def | invalidates global-object cache + prunes 4 ObjectSearcher pools |
| 17 | port `unreal/src/ClassListener.cpp:122-125` | override def | `OnUObjectArrayShutdown` → `RemoveUObjectDeleteListener(this)` |
| 18 | registrations/removals | see #9, #11 above | |

### 1.3 FShutdownDeleteListener

| # | file:line | kind | detail |
|---|---|---|---|
| 19 | port `unreal/include/Unreal/ClassListener.hpp:38-44` | declaration | overrides Notify + Shutdown only; static instance; no new virtuals, no data |
| 20 | port `unreal/src/ClassListener.cpp:14` | construction | static instance definition |
| 21 | port `unreal/src/ClassListener.cpp:127-129` | override def | empty Notify body |
| 22 | port `unreal/src/ClassListener.cpp:131-136` | override def | clears init flags then `RemoveUObjectDeleteListener(this)` |
| 23 | registration | see #10 above | always-on shutdown tripwire |

### 1.4 FLuaObjectDeleteListener

| # | file:line | kind | detail |
|---|---|---|---|
| 24 | port `ue4ss/include/LuaType/LuaUObject.hpp:31-41` | declaration | overrides Notify (decl only) + Shutdown (inline, removes itself); static instance; no new virtuals, no data |
| 25 | port `ue4ss/src/LuaType/LuaUObject.cpp:91` | construction | `FLuaObjectDeleteListener FLuaObjectDeleteListener::s_lua_object_delete_listener{}` |
| 26 | port `ue4ss/src/LuaType/LuaUObject.cpp:92-105` | override def | Notify: fires Lua ` onDelete` callbacks |
| 27 | port `ue4ss/include/LuaType/LuaUObject.hpp:39` (inline body) | removal | `Unreal::UObjectArray::RemoveUObjectDeleteListener(this)` inside `OnUObjectArrayShutdown` |
| 28 | registration | see #12 above | |
| 29 | l22 `diagnostics/include/LuaType/LuaUObject.hpp:32-42` | declaration | **struct body byte-identical to the port root copy** — harness sha `5a34e510…15bbbc7` for both extracted regions |
| 30 | l22 `diagnostics/src/LuaUObject.cpp:92-93` | construction + override def | same shape (includes extra L22Diagnostics logging elsewhere in file) |
| 31 | l22 registration | see #13 above | |

### 1.5 Things the fix could break — searched for, none found

- **reinterpret_cast / pointer casts on listeners:** none in either tree (the
  only upcasts are implicit `&Concrete::instance` → base at #9-#13).
- **static_assert / sizeof / offsetof on any listener:** none (the generated
  `MemberVariableLayout_*FUObjectArray*` offsets are for the *engine*
  FUObjectArray's data members, not the listener class).
- **vtable enumeration / manual vptr indexing:** none in the port or l22
  trees (`grep -i 'vtable|vptr'` filtered by listener/delete ⇒ 0 hits). The
  port's vtable generator (comment at UObjectArray.hpp:18-20) explicitly
  excludes these small listener classes — confirmed: no
  `*VTableOffsets*Listener*` generated files exist (generated VTableOffsets
  files cover UEngine, IConsoleManager, AActor, FField, UObjectBaseUtility,
  etc., never a listener).
- **enhanced-prologue / hook-prologue vptr[4] assumptions:** the only
  "prologue" mentions in the lineage (l22 `LuaMod.cpp:4784,7952`) concern
  `process_event_hook`'s instruction prologue and the marshal-calibration
  detour ordering — unrelated to listener vtables. No code assumes a
  listener slot count.
- **serialization of vtable indices:** none (listeners are never serialized).
- **Slot-completeness reliance by the derived classes:** all three concrete
  listeners declare only overrides of existing base virtuals (verified by the
  harness compile: their vtables are exactly the base slots + inherited
  `GetAllocatedSize` post-fix; no derived-local virtual exists that could
  collide with engine slot 4 — see §2, the sentinel experiment shows what
  such a collision *would* look like).
- **Create-listener side (untouched by the fix, verified):** engine
  `FUObjectCreateListener` (header lines 908-924) also has only 3 virtuals —
  dtor, `NotifyUObjectCreated`, `OnUObjectArrayShutdown` — matching the port
  4-entry-for-4-entry. The harness asserts this parity as a negative control
  (first-free-slot 4 on both sides). **Do not** add GetAllocatedSize to the
  create listener — the engine does not have it there.

**Blast radius verdict (Task A):** the fix is additive-only at every observed
site. The only behavioral change is that engine slot-4 calls
(`GetDeleteListenersAllocatedSize`, e.g. during memory tracking) stop
reading past the end of the port objects' vtables and instead hit the inline
`return 0`. Registrations, removals, and dispatch at slots 2/3 are untouched
(slot order preserved — proven next). Risk found: none beyond the fix's
intended effect; the three listeners and both trees need no source changes.

## 2. TASK B — compiled layout proof (`/tmp/l25-listener-layout/`)

Harness: `layout_proof.cpp` includes **verbatim extracted regions** (sed
slices, sha256-recorded in `verbatim/SHASUMS.regions`; full-file shas match
the l24-build-r2 pins):

- `verbatim/engine_UObjectArray_h_929_954.orig` — engine delete listener (sha `8b2b27c6…14bea65`)
- `verbatim/engine_UObjectArray_h_908_924.orig` — engine create listener (sha `dd655a5f…90ceb0`)
- `verbatim/port_UObjectArray_hpp_21_35.orig` — port base listeners, PRE-fix (sha `da7efd52…89eacdd1`)
- `verbatim/port_UObjectArray_hpp_21_35.fixed` — same region + the one-liner
  `virtual size_t GetAllocatedSize() const { return 0; }` inserted after
  `OnUObjectArrayShutdown()` as the LAST virtual (sha `6e672500…bcc884`;
  exact diff in `verbatim/port_base_fix.diff`)
- `verbatim/port_ClassListener_hpp_20_44.orig` (sha `529f85d7…3f0697`)
- `verbatim/port_LuaUObject_hpp_31_41.orig` and
  `verbatim/l22_LuaUObject_hpp_32_42.orig` (both sha `5a34e510…15bbbc7` —
  byte-identical, proving port/l22 listener parity)

Compiler: `Apple clang 17.0.0 (clang-1700.6.4.2)`, arm64-apple-darwin24.6.0,
`-std=c++20 -Wall -Wextra`. Same Itanium vtable ABI family as the PS5
target (`x86_64-sie-ps5` per agent V); the PTMF word0 encoding differs
between the two compilers (Itanium `byteoff+1` vs arm64-apple raw `byteoff` —
determined empirically in `ptmf_probe.cpp`), so the harness slot probe decodes
self-descriptively (odd ⇒ Itanium) and is cross-checked against clang's own
`-Xclang -fdump-vtable-layouts` dump, which worked and is captured in full
(`out/vtable-layouts.txt`, key sections distilled in
`out/vtable-layouts-key-sections.txt`).

### 2.1 Pre-fix: port 4 entries vs engine 5 — clang's own dump

```
Vtable for 'rc_pre::Unreal::FUObjectDeleteListener' (6 entries).        <- 2 header words + 4 slots
VTable indices for 'rc_pre::Unreal::FUObjectDeleteListener' (4 entries).
   0 | ~FUObjectDeleteListener() [complete]      (D1)
   1 | ~FUObjectDeleteListener() [deleting]      (D0)
   2 | NotifyUObjectDeleted(const class UObjectBase *, int32) [pure]
   3 | OnUObjectArrayShutdown() [pure]

Vtable for 'eng::FUObjectDeleteListener' (7 entries).                   <- 2 header words + 5 slots
VTable indices for 'eng::FUObjectDeleteListener' (5 entries).
   0 | ~FUObjectDeleteListener() [complete]
   1 | ~FUObjectDeleteListener() [deleting]
   2 | NotifyUObjectDeleted(const class UObjectBase *, int32) [pure]
   3 | OnUObjectArrayShutdown() [pure]
   4 | SIZE_T GetAllocatedSize() const
```

Port has **no `GetAllocatedSize` member at all** (also compile-time-proven:
`static_assert(!has_get_allocated_size<rc_pre…FUObjectDeleteListener>)`,
while engine and post-fix both have it, and neither create-listener does).
A harness sentinel class (first *new* virtual after the base) landed on
**slot 4 pre-fix / slot 5 post-fix** — i.e. the pre-fix base occupies exactly
slots 0-3, the post-fix base exactly 0-4. Entry counts were additionally
witnessed at the binary level: the linked vtable word dump
(`out/vtable-entries.txt`) shows 4 function words for rc_pre/rc_l22 Lua
listener vtables vs 5 for rc_post (macOS chained fixups garble per-word
symbolization, so counts-not-names is what that dump reliably adds; the
clang textual dump above is the primary evidence).

### 2.2 Post-fix: identical slot order AND signatures

```
Vtable for 'rc_post::Unreal::FUObjectDeleteListener' (7 entries).
VTable indices for 'rc_post::Unreal::FUObjectDeleteListener' (5 entries).
   0 | ~FUObjectDeleteListener() [complete]
   1 | ~FUObjectDeleteListener() [deleting]
   2 | NotifyUObjectDeleted(const class UObjectBase *, int32) [pure]
   3 | OnUObjectArrayShutdown() [pure]
   4 | size_t GetAllocatedSize() const
```

Slot-for-slot identical to the engine dump in §2.1 (D1, D0, Notify,
Shutdown, GetAllocatedSize). Signature equality is asserted at compile time
by exact member-pointer-type `static_assert`s for every virtual in every
variant, e.g. `size_t (C::*)() const` for both GetAllocatedSize and
`void (C::*)(const UObjectBase*, int32)` for both Notify declarations
(engine uses `SIZE_T`, port uses `size_t` — same type on the 64-bit target;
`int32`/`int32_t` likewise).

### 2.3 Concrete listeners: no reorder, still concrete, still 8 bytes

Clang dump excerpts (full text in the artifacts):

```
Vtable for 'rc_pre::Unreal::FClassDeleteListener' (6 entries).   indices: 0 D1, 1 D0, 2 Notify, 3 Shutdown
Vtable for 'rc_post::Unreal::FClassDeleteListener' (7 entries).  indices: 0 D1, 1 D0, 2 Notify, 3 Shutdown,
                                                                                     4 size_t FUObjectDeleteListener::GetAllocatedSize() const  [inherited]
```

Same pattern for `FShutdownDeleteListener` and for `FLuaObjectDeleteListener`
in **both** the port-root and l22 variants (pre: 4 slots; post: 5 slots with
inherited base `GetAllocatedSize` appended at slot 4). Derived slots 0-3 are
byte-for-byte the same positions pre and post — **no existing slot moves**.
`-Xclang -fdump-record-layouts` (captured in `out/record-layouts.txt`)
shows every listener class is exactly `[sizeof=8, dsize=8, align=8]` — one
vptr, no data — in all variants, so the fix adds a virtual function without
changing any object size. All verbatim `override` declarations compiled
against both the pre- and post-fix bases without a single diagnostic
(`override` catches signature drift; `!is_abstract` asserts they remain
concrete).

### 2.4 Runtime end-to-end dispatch (engine-style raw slot calls)

`out/run-output.txt` (ALL PROOFS PASSED, exit 0):

- Engine-style calls through literal vtable slots 2/3/4 on post-fix objects:
  slot 2/3 hit the correct overrides, slot 4 returns the base `0`.
- Pre-fix objects: slots 2/3 dispatch correctly (the ABI gap is *only* slot
  4; consistent with the port working at runtime until the engine walks
  `GetDeleteListenersAllocatedSize`).
- Hazard demo (safe stand-in object): on a pre-fix derived class carrying one
  extra virtual, the engine "GetAllocatedSize" slot-4 call dispatched into
  that unrelated virtual — a live illustration of the collision/OOB the fix
  removes. The real listeners declare no extra virtuals, so their pre-fix
  slot-4 reads run past the 4-entry vtable into adjacent read-only data.

### 2.5 Verdict

**Pre-fix:** port base vtable = 4 entries (D1, D0, Notify, Shutdown) vs
engine = 5 (…+ GetAllocatedSize at slot 4) — mismatch confirmed by clang's
own vtable dump, member-pointer slot probes, sentinel free-slot probes, and
binary vtable word counts. **Post-fix:** identical slot order and signatures
across all five slots; all three concrete listeners (port + l22 copies)
compile unchanged, keep slots 0-3 fixed, inherit `GetAllocatedSize(){return 0;}`
at slot 4, and stay 8-byte single-vptr objects. Adding the line as the LAST
virtual of the port's `FUObjectDeleteListener` is layout-safe and
ABI-completing; nothing in the scanned trees relies on the 4-entry shape.

## 3. Artifacts

- `/tmp/l25-listener-layout/layout_proof.cpp` — harness TU (verbatim-region includes)
- `/tmp/l25-listener-layout/build.sh` — 6-step build (record-layouts, vtable-layouts, strict compile, vtable symbols, link+run, binary vtable dump)
- `/tmp/l25-listener-layout/verbatim/` — extracted regions + `SHASUMS.regions` + `port_base_fix.diff`
- `/tmp/l25-listener-layout/out/` — `vtable-layouts.txt`, `vtable-layouts-key-sections.txt`, `record-layouts.txt`, `run-output.txt`, `vtable-entries.txt`, `vtable-symbols.txt`, `build.log`
- `/tmp/l25-listener-layout/ptmf_probe.cpp`, `vtable_dump.py` — encoding-calibration probe and binary dumper

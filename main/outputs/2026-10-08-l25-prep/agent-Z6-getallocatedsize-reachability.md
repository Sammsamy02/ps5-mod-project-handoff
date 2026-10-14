# Agent Z6 — Is `FUObjectDeleteListener::GetAllocatedSize` (slot 4) reachable at save load / GC in UE 5.6.1?

Date: 2026-10-08. READ-ONLY analysis. No source files modified; this report is
the only file written. Evidence tags used throughout:

- **[V]** verified in the vendored official source or other local artifacts
- **[B]** verified in the shipping game binary (decrypted eboot)
- **[U]** upstream engine knowledge — NOT locally verifiable (file not in the
  478-file vendored subset); confidence stated

## 0. Sources actually used (paths verified)

- Vendored UE 5.6.1 snapshot (478 files, the tree agent V cited):
  `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-layout-generic/official-source/Engine/`
  - `Build/Build.version`: `MajorVersion 5, MinorVersion 6, PatchVersion 1,
    CompatibleChangelist 43139311, BranchName "UE5"` **[V]**
  - Only `CoreUObject/Private/UObject/UObjectHash.cpp` is vendored from
    CoreUObject's Private tree; **`UObjectArray.cpp` and `UObjectGlobals.cpp`
    implementations are NOT vendored** (confirmed by directory listing) **[V]**.
  - A second pruned copy exists at
    `outputs/2026-10-08-l23-layout-generator/official-source/` (465 files,
    content-equivalent for all files cited below) **[V]**.
- Shipping game binary (the image the whole effort calibrated against):
  `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`
  — 194,241,424 bytes, byte count identical to the code_image authenticated in
  the crash session (agent F §timeline, `fd 103, read 194,241,424 bytes`);
  sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`.
  ELF64 x86-64; single executable PT_LOAD of 0x7479b1c bytes (~122 MB) at
  vaddr 0 — the UE engine is statically linked into eboot (the console dump
  shows only system/audio/font .prx modules, no UE sprx) **[B][V]**.
- Runtime evidence: `/tmp/l24-ue4ss*.log` (5 sessions),
  `/tmp/l24-ab2-*.log`, agent F/G/Q/V/Z1/Z7 reports in this directory **[V]**.

## Headline answer

**No. In stock UE 5.6.1 the slot-4 dispatch lives exclusively behind explicit
memory-statistics/diagnostic entry points — never save load, never GC, never
normal gameplay. In THIS shipping Townfall binary the entire calling function
is dead-stripped, so the slot-4 `vptr[4]` dispatch cannot execute at all, at
any time, even via console commands. Mechanism A is ruled out for this title;
the crash attribution inside the listener family collapses onto mechanism B
(the unlocked/allocator-mismatched registration write), with the caveat that
neither mechanism directly produces the kernel `copyin: has nonsleeping lock`
assertion on its own.**

---

## 1. Full caller chain of `GetDeleteListenersAllocatedSize` [V]

Bottom-up, every step cited from the vendored tree:

1. **Dispatch site (slot 4).** `FUObjectArray::GetDeleteListenersAllocatedSize`
   — `Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h:1465-1480`.
   Loop over `UObjectDeleteListeners` at **:1471**; the virtual call
   `AllocatedSize += Listener->GetAllocatedSize();` at **:1473** (agent V
   cited the loop line 1471; the call itself is 1473). Under
   `UE::TScopeLock ListenersLock(UObjectDeleteListenersCritical)` at **:1467-1469**.
2. **Virtual declaration.** `virtual SIZE_T GetAllocatedSize() const { return 0; }`
   — `UObjectArray.h:950-953`, the 4th virtual of `FUObjectDeleteListener`
   (`class` at **:929**) → Itanium slot 4 (agent V §1/§1a) **[V]**.
3. **Sole caller of step 1.** `LogHashMemoryOverheadStatistics(FOutputDevice&, EObjectMemoryOverheadOptions)`
   — declared `UObjectHash.h:322` (COREUOBJECT_API; deprecated bool overload
   `UObjectHash.h:330-333`); defined `UObjectHash.cpp:2320`; calls
   `GUObjectArray.GetDeleteListenersAllocatedSize(&NumListeners)` at
   **`UObjectHash.cpp:2416`**, inside the delete-listeners stats block
   `:2414-2422`. Grep across the ENTIRE vendored tree returns exactly two hits
   for `GetDeleteListenersAllocatedSize`: the declaration (`UObjectArray.h:1465`)
   and this one call (`UObjectHash.cpp:2416`) **[V]**.
   - Note: the task brief's anchor "UObjectHash.cpp:2318" is the closing brace
     of the *preceding* function `LogHashOuterStatistics` (`:2302-2318`); it is
     not a caller **[V]**.
4. **Non-hazard lookalike.** `FUObjectArray::GetAllocatedSize()`
   (`UObjectArray.h:1456-1463`) calls only `UObjectDeleteListeners.GetAllocatedSize()`
   — the **TArray's** capacity size, not a virtual dispatch on listeners. It is
   NOT a slot-4 path and is reached from `LogHashMemoryOverheadStatistics`
   itself at `UObjectHash.cpp:2425` **[V]**.
5. **Callers of `LogHashMemoryOverheadStatistics` (walk up to entry points).**
   Zero callers exist anywhere in the vendored 478-file subset (grep: only
   `UObjectHash.h` declaration/inline + `UObjectHash.cpp` definition) **[V]**.
   Upstream UE 5.6-lineage callers are the Engine-module diagnostic machinery:
   the `memreport` console-command / `-memreport` command-line flow
   (Engine/Private/MemReport.cpp `FMemReport`) and editor/automation memory
   diagnostics — all explicit, operator-initiated diagnostics **[U, confidence:
   medium-high on the memreport family; the vendored subset cannot enumerate
   them]**. No GC, save-load, or gameplay code path calls it **[U, confidence:
   high — the function is a statistics logger; `CollectGarbage`
   (`UObjectGlobals.h:912` [V]) and the async-loading pipeline have no
   reference to it]**.

## 2. Shipping reachability per entry-point class

| Entry point | Exists in stock UE 5.6.1? | Exists in THIS shipping binary? | Reachable at save load / GC / gameplay? |
|---|---|---|---|
| `memreport` console cmd / `-memreport` CLI → `FMemReport` → `LogHashMemoryOverheadStatistics` [U] | yes (non-shipping/Test configs; `UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING` affects exec routing, not this function's callers) | **NO — proven absent [B]** | n/a here; and never automatic anyway |
| Stats/`stat memory`-style | yes (STATS builds) | **NO — `stat unit`, `stat memory`, `STAT MEMORY` strings: 0 hits [B]**; consistent with l23 `MACRO-AUDIT.json` stats=0 **[V]** | no |
| Editor-only (`WITH_EDITOR`) paths | yes | no editor on PS5 | no |
| Save load (`UEngine::LoadMap` → world teardown → `CollectGarbage`, `UObjectGlobals.h:912` [V]) | **no call path** [U-high] | function absent [B] | **NO** |
| Garbage collection | **no call path** [U-high] | function absent [B] | **NO** |
| Normal gameplay | no call path [U-high] | function absent [B] | **NO** |

### Binary proof (the decisive new evidence) [B]

Whole-file search of the decrypted eboot (all segments; both ASCII and
UTF-16LE — the game's wide strings are UTF-16LE, established by positive hits
such as `LoadMap` ×8):

- `UObject Hash Tables and Maps memory overhead` — the UNCONDITIONAL first
  `Ar.Logf` of `LogHashMemoryOverheadStatistics` (`UObjectHash.cpp:2322` [V]):
  **0 hits**.
- Every other literal of that function and its siblings: `Hash Tables and Maps`
  0; `Memory used by UObject Delete Listeners (including annotations)` and
  substring `Delete Listeners` 0; `including annotations` 0; `Memory used by
  UObject` 0; `Total memory allocated by Object hash` 0; `Hash efficiency
  statistics` 0; `Object Outer Map` 0 — i.e. `LogHashMemoryOverheadStatistics`,
  `LogHashOuterStatistics`, `LogHashStatisticsInternal` are ALL absent **[B]**.
- General MemReport machinery: the single `MemReport` UTF-16 hit is the
  **audiomemreport** cvar help text; `Memory report`, `MemReport-`,
  `GENERATING`, `DumpHash`, `obj hash`, `FMemReport` — 0 hits **[B]**.
- Positive controls that the search method works on this binary: `LoadMap`
  (UTF-16, 8), `audiomemreport` text, the cvar help `If not 0 dumps UObject
  counts to log when maximum object count limit has been reached`, GC-adjacent
  strings like `GarbageCollection` (class-name/reflection text) all present.
  The two `memory overhead` hits are `r.RayTracing` cvar help and a MassEntity
  page-lookup comment — unrelated **[B]**.

Reasoning: the function's first statement unconditionally logs its header
string. All its unique literals are absent file-wide ⇒ the function body is
not linked into the shipping game ⇒ the sole caller of
`GetDeleteListenersAllocatedSize` does not exist ⇒ **the `vptr[4]` virtual
dispatch never executes in this game under any input**. The shipping link
dead-stripped the unreferenced diagnostics (same reason the GC log strings
`Purging garbage` / `Collecting garbage` are 0-hit while GC itself obviously
runs). Residual caveat: a hypothetical direct call
`GUObjectArray.GetDeleteListenersAllocatedSize()` from Konami game code would
leave no string trace; there is no plausible motive and no evidence of one —
noted for completeness, assessed as negligible.

## 3. Does save load / GC dispatch `NotifyUObjectDeleted` (slots 0-3)? YES — heavily [U-high + V corroboration]

- Dispatch chain (impl in the non-vendored `UObjectArray.cpp`): UObject
  destruction → `FUObjectArray::FreeUObjectIndex` (declared
  `UObjectArray.h:1016` [V]) → iterates `UObjectDeleteListeners`
  (`UObjectArray.h:1432` [V]) calling `NotifyUObjectDeleted(Object, Index)`
  (virtual slot 2, declared `UObjectArray.h:940` [V]) per listener; the create
  sibling is `AllocateUObjectIndex` (`:1009`) → create listeners (`:1428`)
  [U for the loop bodies, V for every declaration touched].
- Save load = mass destruction: `UEngine::LoadMap` tears down the previous
  world and collects garbage (`CollectGarbage`, `UObjectGlobals.h:912` [V]);
  every destroyed UObject fires the delete-listener dispatch. A save load
  destroys thousands of objects, so the path is exercised at maximum intensity
  exactly at the observed crash point. Z1's klog baseline corroborates: **every
  crash follows a SaveGame mount/unmount** [V].
- `FUObjectHashTables` itself does not dispatch `FUObjectDeleteListener`
  (vendored-tree grep: the only other `NotifyUObjectDeleted` text is the
  `SparseDelegate.h:87` override declaration; `DissociateExternalReferences`
  has 0 hits in the vendored tree — it is not a delete-listener dispatcher) [V].
- Our slots 0-3 match the engine exactly (agent V §3), so these dispatches are
  ABI-safe. Behavioral corroboration: all five l24 sessions ran through menu
  idle (GC cycles at ~60 s cadence) after init without an immediate crash;
  failure clusters at save load, not at first dispatch [V: `/tmp/l24-ue4ss*.log`,
  agent F].

## 4. (Bonus) Is the engine's dispatch LOCKED — do our unlocked writes race it?

- `GetDeleteListenersAllocatedSize` holds `UObjectDeleteListenersCritical`
  (`UObjectArray.h:1467-1469`) and `FUObjectArray::GetAllocatedSize` holds both
  `ObjObjectsCritical` and `UObjectDeleteListenersCritical` (`:1458-1461`) [V].
- `THREADSAFE_UOBJECTS` is hard-defined to **1** at
  `CoreUObject/Public/UObject/ObjectMacros.h:33` [V] → the
  `FTransactionallySafeCriticalSection` member exists in shipping PS5 builds
  (`UObjectArray.h:1433-1435` [V]).
- The `NotifyUObjectDeleted` dispatch loop in `FreeUObjectIndex`, and
  `AddUObjectDeleteListener`/`RemoveUObjectDeleteListener` (declared
  `UObjectArray.h:1133`/`:1140` [V]) take the same critical section in the
  upstream implementation — every listener-array access the vendored header
  shows is lock-guarded, and agent V §4 reached the same conclusion [U-high,
  consistent with V].
- Our port bypasses the engine functions and writes the live member array
  directly, UNLOCKED, growing it with our own container code:
  `port/unreal/src/UObjectArray.cpp:379-392` (`Contains` → `Append` /
  `RemoveSingleSwap`) [V: agent V §4 quotes the code]. Registration sites run
  at UE4SS init while the game thread is live (menu): `UnrealInitializer.cpp:425/429`,
  `LuaMod.cpp:6896` [V: agent V §4]. **So yes — the classic corruption window
  exists: an unlocked read-modify-write of an array the engine iterates under
  lock, plus a cross-allocator growth of an engine-owned `TArray`.**
- Additional local fact that sharpens B: agent G code-traced that in EVERY
  l24 arm the UE4SS log ends at `UnrealInitializer.cpp:527` ("Adding shutdown
  listener", `:608` in all five logs [V]) and the ONLY statement before the
  next printable line (`:542`) is `AddUObjectDeleteListener` (`:528` →
  `UObjectArray.cpp:379-389`) — the first live-state write / GMalloc-virtual
  call in startup; the post-init GMalloc repoint is logged one line earlier
  (`Post-initialization: GMalloc: 0x970add8 -> 0x97089b8` [V]). A fault, stall,
  or partial write in that registration leaves init unreturned (no
  `on_program_start`, no Lua mod) AND can leave the engine's live listener
  array / heap damaged — detonating later under save-load mass destruction.
  Z1's salvaged abort backtrace (plugin worker → eboot frames → libc abort)
  is compatible with an allocator-path abort from our side writing into
  engine-owned state [V: Z1 summary in CLAUDE-REPORT §4b].

## 5. Verdict for CE-108255-1-at-save-load — ranked

1. **Mechanism A (slot-4 OOB dispatch): RULED OUT for this game.** Not
   "unlikely" — impossible: the only function containing the `vptr[4]` call
   site is not present in the shipping binary [B], and even in stock UE 5.6.1
   it is reachable only through explicit memory-statistics entry points, never
   save load or GC (§1-2). The agent V fix (add the slot-4 virtual, declared
   last) remains correct long-horizon ABI hygiene — for non-shipping targets,
   other games, or any future engine config where the diagnostics survive
   linking — but it cannot change THIS game's crash behavior.
2. **Mechanism B (unlocked, wrong-allocator write of the live listener array
   racing / corrupting the engine's locked dispatch state): the only live
   member of the listener family.** It has a concrete, evidence-aligned chain:
   registration at init (menu, game thread live) → unlocked
   `Contains`/`Append` + our allocator + GMalloc-virtual on an engine-owned
   array → either init stall (agent G; all five logs stop at the registration
   statement [V]) or latent corruption of the array/heap → engine mass
   delete-listener dispatch + allocation storm at save load → abort
   (Z1: crash always after SaveGame mount/unmount; plugin-worker backtrace) [V].
3. **Honest confounder:** neither A nor B by itself obviously produces the
   kernel-side `copyin: has nonsleeping lock` assertion (libkernel RIP) — that
   signature wants a thread performing copyin while holding a spinlock. Agent Q
   already weakened the resident copyin-probe theory (retired at init, no
   post-admission caller), so B (and registration-adjacent live-state writes
   generally) is the leading in-family candidate — but attribution outside the
   listener family cannot be excluded yet; Z11's symbolization of the 15
   plugin frames is the pending decisive input on that axis.

### Discriminating experiment (one outcome)

**Run the l25 twins in the order A then B (Z7's LAUNCH-PLAN arms) and compare
save-load behavior — the single most informative outcome is:**

> **A (slot-4 fixed, normal registration) still crashes at save load AND B
> (registration skipped) survives it.**

That outcome isolates the crasher to the registration write family (B
mechanism) and simultaneously confirms the static prediction that slot 4 never
fired (fixing a dispatch that cannot execute changes nothing). It is exactly
the "B-skip-registration eliminates the crash entirely but A-normal still
crashes late" pattern the coordinator hypothesized — this report predicts it,
with the refinement that A crashing is not "A-normal crashes late ⇒ slot 4
implicated" but "A crashes because A still performs the broken registration".

Branches on the same experiment:
- **A survives, B survives** ⇒ the l24 crash needed something the twins both
  changed (e.g. build skew) — re-baseline with the original l24 PRX before
  drawing conclusions.
- **B also crashes** ⇒ NEITHER A nor B: attribution moves outside the listener
  family entirely (residual modbridge machinery / copyin lineage); stop
  bisecting the listener family and let Z11's backtrace symbolization lead.
- Independent of save load: watch the `[l25] event loop started` /
  `[l25] on_program_start entered` phase tags — if A stalls before them (all
  l24 arms did [V]), the registration write is implicated even before any
  save load, reinforcing B.

## 6. Provenance

- Vendored-tree citations verified by Read/grep at the paths in §0 on 2026-10-08.
- Binary analysis: manual ELF64 program-header parse + whole-file byte search
  (ASCII + UTF-16LE) of `console-decrypted-eboot.bin` (sha256 above); scratch
  scripts were run from /tmp only; no other file written.
- Upstream-labeled items are explicitly marked [U] with confidence; none of
  the verdicts depend on an [U] item except where corroborated by [V]/[B].

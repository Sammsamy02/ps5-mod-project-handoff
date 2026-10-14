# ANALYSIS — worker-l11-loadmap-guard (r2)

2026-10-05 · LOCAL ONLY (host compile gates + host detour-layer test; no console
contact, no composition run, no packaging; canonical receipts untouched).

## 0. The question, restated

Launch-11's post-arm LoadMap blocker (RUNBOOK §5 item 2): `LuaMod::on_program_start`'s
first eager detour arm — `RegisterLoadMapPreCallback` at LuaMod.cpp:7324 in the
marshal-consumer staging — was believed to "walk exec-only text (igf6 crash class)".
Decide: does that site route through the FOLDED guard (launch-10 shipped
ASMHelper.guarded.cpp fold-0, unit object `81dd832b`, so an R3 world would SURVIVE
with a loud `[WalkRxGuard]` refusal) or an unguarded copy? If unguarded: stage the
fix routing it through the guard / deferring it. Host-verify: exec-only simulation →
loud refusal + survival; readable → proceeds. Document what the unchanged mod loses
without LoadMap hooks.

## 1. The measured answer: neither horn of the dilemma

The arm's real path is:

```
LuaMod.cpp:7324 RegisterLoadMapPreCallback            (marshal-consumer staging)
  → Hooks.cpp:211-213  GetDetourInstance<LoadMap>()->AddCallback(Pre)
    → DetourInstance.hpp:151-172 InstallHook()        (bShouldHook + is_ready flag
                                                       checks — data only, NO exec access)
    → DetourInstance.hpp:176-188 ActivateHook() → PLH::x64Detour::hook()
        x64Detour.cpp:351   m_disasm.disassemble(m_fnAddress, …, +100, *this)
          ZydisDisassembler.cpp:57-69  reads via accessor.safe_mem_read
            → OwnedMemory::copy (read-lease d824528d) :76-83
            → non-owned extent ⇒ FALSE with ZERO dereference ("No pointer is
              dereferenced until both explicitly owned extents pass")
        x64Detour.cpp:354-356  empty insts ⇒ SEV log, return false   ← the arm ends here
    → DetourInstance.hpp:203-206  EnsureHooked false ⇒ ONE Output Error line
      ("[UE4SS.LoadMap.LuaModImpl] Failed to add hook…") ⇒ ERROR_ID
  → LuaMod.cpp:7357  the returned GlobalCallbackId is IGNORED; on_program_start
    continues; start_lua_mods (UE4SSProgram.cpp:1506) runs; Lua loads.
```

- **Not the guarded site**: the WalkRxGuard wraps
  `ASMHelper::get_first_instruction_at_address` (coverage note, ASMHelper.guarded.cpp:29-35),
  which only the INITIALIZER's earlier resolve walks call (LoadMap walk at
  UnrealInitializer.cpp:1755). By the time `on_program_start` runs, the LoadMap
  address is a plain assigned value; `is_ready()` reads a flag, never text.
- **Not an unguarded raw copy either**: the composed launch-10/11 units mediate
  every PLH touch of memory through the owned registry (x64Detour `5bb9cea4`
  owned path, ZydisDisassembler `facfea22` accessor reads, read-lease OwnedMemory
  `d824528d`). Live game text is NEVER registered (the AllocatorBridge records
  only allocator-owned mappings; the owned-mapped scan sources are file-backed
  images). So in EVERY world the arm soft-refuses at its first step — crash-free,
  throw-free — and per arm costs one PLH SEV line + one Error line.
- **The DIVERGENCE that explains the mission premise**: DRAIN-TRACE (and RUNBOOK
  §5.2's "throws under BOTH_FAIL") pinned `ue4ss-native-polyhook-sources-1/port/
  polyhook/sources/x64Detour.cpp` (`0619d2b4`, hook() at :322, raw Zydis branch,
  no isGood gate). That is a DIFFERENT-LINEAGE file. Every launch-10/11 composition
  receipt pins `5bb9cea4` (owned-default-detour port): owned reads, `isGood()`
  gates (:491-492), owned patch/rollback (:447-522). The igf6-class naked read
  does not exist on this path as composed. Raw DRAIN-TRACE stays unchanged per
  contract; E4 in CLAIM.json carries the supersession.

## 2. Where the real [WalkRxGuard] refusal lives

At fold-0 the guard refuses UNCONDITIONALLY (loud Error + `std::runtime_error`,
zero syscalls — ASMHelper.guarded.cpp:231-239). The refusal fires in the
INITIALIZER's walks, BEFORE `on_program_start`: the OPTIONAL macro does not catch
(UnrealVirtualBaseVC.hpp:118-136), the initializer's `catch(...)` disarms the
query-harness gate and RETHROWS (:2061-2071), and the throw surfaces through the
module entry's loud failed-status ack chain. So the mission's "R3 world survives
with a loud [WalkRxGuard] refusal" is TRUE — one level earlier than the LuaMod
arm, and it ABORTS UE4SS init (no Lua, no mods) rather than proceeding. The r2
gate below is therefore reachable only in worlds where the walks were admitted
(fold-1) or bypassed (seeded addresses); this is stated in the staged comment.

## 3. The staged fix (r2) — why defer when there is no crash

No crash-safety fix is needed. What remains is real but smaller:

1. **R3D §5.3 item 3 obligation** — in the R3 world the initializer deferred its
   identical-class registrations (pump_pending=true, UnrealInitializer.cpp:1861);
   the LuaMod-side arms are futile by the same law. Leaving them to fail one by
   one every boot leaves the obligation looking unowned.
2. **Log hygiene** — ~19 eager arms (LoadMap pre/post, InitGameState pre/post,
   BeginPlay pre/post, EndPlay pre/post, StaticConstructObject post,
   ULocalPlayerExec pre/post, CallFunctionByNameWithArguments pre/post,
   ProcessConsoleExec ×5, ProcessLocalScriptFunction/custom events) each emit a
   PLH SEV line + an Error line per boot in the R3 world.
3. **Explicit loss declaration** — there is no runtime re-arm path (DIFF-2
   header law); the Lua-side loss is silent unless declared (§4).

The r2 staging gates the whole eager-arm block on `Unreal::R3MarshalPumpPending()`
(declared in this TU at line 93 by the marshal consumer — zero new imports;
defined by the initializer bridge at :2190-2192). W-works world (pump_pending
=false): the block runs unchanged. R3 world: ONE loud Warning names exactly what
stays unregistered for the process lifetime. The first-attempt staging (r1) is
preserved in place; its COMMENT wrongly claimed a naked-read crash class and its
Warning literal repeated it — r2 corrects the rationale and the literal; the
gate code is otherwise identical. Distinct IDs: `l11-loadmap-defer` (r1,
preserved) vs `l11-loadmap-defer-r2` (deliverable).

## 4. What the unchanged mod loses without LoadMap hooks (honest)

TownfallItemSpawner main.lua:731-747 (per the API inventory,
worker-itemspawner-lua-audit/LUA-API-MATRIX.md):

```lua
pcall(RegisterLoadMapPreHook, function()
    if menuOpen then showMenu(false) end
    ui.root, ui.tree, … = nil,…        -- clears all cached UI refs
    items = {}; ownedActors = {}       -- clears item + actor caches
    page = 1; selectedIndex = 1; busy = false
    status = "Load complete. Open with F7."
end)
```

In the R3 world the Lua-side registration still SUCCEEDS (it appends to
`m_load_map_pre_callbacks`; the mod loads cleanly), but the engine-side detour
never arms, so the callback is **never invoked**: stale cross-map UI references
and item/ownedActor caches persist across map loads, the menu stays open across
loads, and page/selection/busy state carries over until manually rebuilt. With
r2, this exact loss is stated in the boot log. The same law covers the sibling
hooks in the gated block (BeginPlay/EndPlay/InitGameState/ProcessConsoleExec/
custom events), which the unchanged mod does not use but other mods would.

## 5. Host verification (receipt: host-test/receipt.json)

The real composed detour layer (every polyhook unit compiled from the exact
launch-10-walkguard receipt sha; asmjit/asmtk archives source-identical via the
host-deps pin) under `PLH_OWNED_MEMORY_HOST_TEST`, 5/5 phases, ASan clean:

| phase | world simulated | measured outcome |
|---|---|---|
| exec-only-denied | PROT_NONE page (kernel-denied, unregistered) | hook() false, zero dereference, process survived |
| readable-unregistered | raw mmap R\|X page — the live game-text analog (readable, never registered) | hook() false at :354-356, zero bytes mutated, source still executes — refusal is registry-based, not readability-based |
| owned readable+writeable | registry-backed JIT page | arm PROCEEDS end-to-end: hook() true, real patch (source()==84), owned trampoline runs ORIGINAL (42), unHook restores |
| write-refused (R3) | RWX transition denied | hook() false at the isGood gate (:491-492), zero writes, source keeps running |
| copy-refused rollback | patch copy refused after genuine 7-byte prefix | internal rollback (:500-522), source restored byte-identical, trampoline released |

Fixture fact: the pinned host-deps build routes asmjit allocation through the
owned-allocator port (`AllocatorBridge::record_created`, virtmem.cpp:910), so JIT
pages are registry-backed — the only genuine host analog of owned live text; the
unregistered analog is the raw RX page of phase 0b.

## 6. Compile gate (frozen argv)

Dry-run's 172-token argv, only source/-o/-MF redirected: control reproduces the
marshal-integration consumer object byte-identically (`854a476b…`, 4th
independent reproduction); r2 object is NEW (`3eeba02f…`); both exit 0, 0 errors,
23 baseline warnings each; undefined-symbol sets IDENTICAL — zero new imports.

## 7. Bounds

No console contact, no composition run, no packaging, no canonical-receipt
writes; one heavy compile at a time; the in-game oracle lines remain the only
acceptance evidence for launch-11 and are NOT claimed here. The finding that
detour-class hooks are structurally refused by the owned layer on live game text
in every world is recorded for the launch-11 brief / DIFF-1 carrier owner — it is
not resolved in this worker's scope.

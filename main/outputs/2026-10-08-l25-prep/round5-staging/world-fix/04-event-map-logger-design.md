# 04 — Event-driven [FL-MAP] logger (Claude round-10 item 2)

Agent IO, 2026-10-10/11. Deliverable: `04-event-map-logger.patch` (+ syntax receipt)
in this directory. Design + patch prep only — staged files, no builds, no console.

Base: the frozen candidate tree
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` WITH 03-map-logger already
folded in and the marshal watch disabled in place (verified: `marshal_watch()` body
starts with `return;` + the DISABLED launch-2-regression comment at LuaMod.cpp:5069
area; `patch -p1 --dry-run` on a pristine candidate copy is clean).

---

## 1. The event-source decision

Claude round-10 item 2: "Replace the 1 Hz polling map-watch with event-driven logging
(NotifyOnNewObject on World and/or a LoadMap hook registered at framework start).
Don't just throttle: polling caused the stutter AND missed the early maps."

Ground truth gathered before deciding:

- **Why the poll stuttered/missed**: the watch ran a `FindFirstOf("World")`
  GUObjectArray walk (40-150K objects) INSIDE `funcptr_marshal_hook` — the same
  game-thread call stack that dispatches keybind callbacks. Launch-2 evidence: F7
  keybind entries with zero Lua output / zero ExecuteInGameThread queues; round-4
  (no watch) both queued and ran. And it only STARTED after the marshal world was
  proven — the early maps (Level_Splash) were already gone.
- **NotifyOnNewObject's seam is dead here** (LuaMod.cpp:2917 stores into
  `m_static_construct_object_lua_callbacks`; delivered by
  `RegisterStaticConstructObjectPostCallback` at LuaMod.cpp:8833 — inside the `else`
  branch the R3 gate skips; never registered in this console world). The three
  stub registrations at LuaMod.cpp:1250/1276/1302 throw "not allowed in the game
  thread" for the other Lua states. It cannot be revived without the detour layer.
- **The LoadMap DETOUR is dead here for two stacked reasons**: (1) the deferred arm
  (`try_deferred_loadmap_arm`, LuaMod.cpp:5234) installs via PLH, which the
  owned-memory layer soft-refuses on live game text; (2) `bHookLoadMap` has no INI
  mapping (default false) so the eager path refuses with the hooking-disabled pair.
  The [FL-MAP] markers inside `lua_load_map_pre/post_callback` stay useful only for
  W-works worlds.
- **The engine create-listener is out of risk scope today**: the port's
  `UObjectArray::AddUObjectCreateListener` (UObjectArray.cpp:760) is the unlocked
  direct-TArray-append family; the ENGINE's own Add op was never pinned or
  admission-verified (only the DELETE pair 0x4841A0/0x484240 has Variant-C
  admission). It would also fire on EVERY UObject creation (including the save-load
  window where l24 hit CE-108255-1 with the delete listener registered), and the
  create-listener vtable ABI is unverified in this port — the delete listener's
  "unchanged since 4.12" claim was exactly the l25 slot-4 bug.
- **The VmtTickPump exists and is accepted**: UnrealInitializer.cpp:2152+ — the
  copied-vtable genuine `UEngine::Tick` hook (Claude l26 fix 4, host-fixture-tested
  core in VmtTickPumpCore.inc, three accepted-risk declarations). It arms at
  framework start (from the `bHookEngineTick` R3 block, on the startup-looked-up
  GameEngine instance) — BEFORE the game's first map load.
- **The LoadMap vtable slot is console-proven**: round-4 decoded log line 616:
  `GameEngine::LoadMap address 0x3c4d460` — resolved by the initializer from the
  LIVE GameEngine vtable (`OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, LoadMap,
  GameEngine)`). The address was never the problem; only the detour install was.

### Options

| Option | Mechanism | Verdict |
|---|---|---|
| A. engine `AddUObjectCreateListener` | true event, fires per object creation | **REJECT today** — unlocked TArray append, engine op unpinned/unadmitted, per-object callback frequency, unverified create-listener ABI. Future work: pin + admit the engine Add op like the delete pair, then a World-filtering create listener becomes the gold source. |
| B. LoadMap PLH detour (existing) | UFunction/native detour | **DEAD in this world** (two stacked refusals above). Kept verbatim for W-works worlds — the `src=detour` [FL-MAP] markers stay. |
| C. per-frame cached-World-pointer compare on the Tick entry | cheap per-tick check | **REJECT** — there is NO offset-proven cheap World read in this port (UEngine.hpp has zero member layout: no WorldContexts, no GameInstance; the only proven World resolver is the FindFirstOf walk = the banned scan). Any per-frame check is also "just throttling", the thing Claude explicitly rejected. |
| **B′ (CHOSEN). LoadMap vtable-slot hook on the VmtTickPump's copied table** | the SAME single vptr swap that arms the Tick pump also pokes the LoadMap slot | **RECOMMENDED + implemented.** True event (fires exactly on engine LoadMap calls), arms at framework start (before the first game map → catches Level_Splash), zero per-frame cost, zero dispatch-path work, uses the already-accepted copied-vtable primitive, needs zero new offsets (slot from `UEngine::VTableLayoutMap[L"LoadMap"]` — the same map the initializer already reads). |

### Why the vtable slot intercepts real map loads

`UEngine::LoadMap` is virtual; `UGameEngine::Browse`, world-travel handling and
seamless travel reach it through the GameEngine instance's vtable (C++ virtual
dispatch — even intra-class calls). Swapping the slot on the live instance
intercepts exactly the calls the upstream UE4SS PLH detour targets. Honest delta vs
the (dead) detour: an explicitly QUALIFIED engine-internal call
(`UEngine::LoadMap(...)`) would bypass the slot hook — stock UE source has none on
this path; noted as a residual miss class, same family as the detour's own
qualified-call caveat.

---

## 2. What the patch does

`04-event-map-logger.patch` — `patch -p1` at the candidate root; touches exactly two
TUs, all additions `__PROSPERO__`-guarded (LuaMod bridge) /
`__PROSPERO__ && UE4SS_R3_MARSHAL_MODE`-guarded (UnrealInitializer VmtPump block,
matching the existing fold law).

### src/UnrealInitializer.cpp (the event source)

- **VmtPump additions** (after `in_text`): `vmt_engine_loadmap_entry` — plain SysV
  function with the exact `UEngine::LoadMapSignature` shape
  (`bool(void*, void*, FURL, void*, void*)`; the by-value FURL is the caller's
  temporary under Itanium, so forwarding re-passes the identical bits). Body: log
  `phase=pre` (isolated) → call the ORIGINAL slot word exactly once → log
  `phase=loaded` (isolated) → return the original's result. Counters are
  `std::atomic<uint64_t>`; no PumpCore state is touched from the entry (no race
  with retire).
- **`vmt_pump_poke_loadmap_slot()`**: resolves the LoadMap slot from
  `UEngine::VTableLayoutMap` (refuses if absent/zero/colliding-with-Tick/outside
  the copied extent/non-text original word), pokes the entry into the pump's
  PRIVATE copy (never the original table), verifies by byte-wise readback, restores
  bit-exact on a failed poke, returns Armed / RefusedClean / DirtyCopy.
- **`vmt_pump_arm` integration**: the poke runs between `pump.arm(...)` and
  `pump.publish_swap()` — ONE atomic vptr store then publishes BOTH hooks; a racing
  dispatcher sees the original table or the fully-poked copy, never a half state.
  RefusedClean → pump arms Tick-only (loud `[FL-MAP-EV]` line, non-fatal). DirtyCopy
  → the whole publish is refused (the pump retires; carriers remain the only pump).
- **Extern "C" declarations** next to `vmt_engine_tick_drain` (header-free `void*`
  law) + `#include <atomic>`.
- Marker line at arm: `[FL-MAP-EV] LoadMap slot armed (src=vmt-loadmap): slot=... original=... resolved=...`.

### src/LuaMod.cpp (the marker contract, reused verbatim)

- **`FlMap::note_pre_event` / `note_loaded_event`**: the `src=vmt-loadmap` pair —
  same contiguous `[FL-MAP] order=<n> map='<name>' phase=pre|loaded` shape, SAME
  order counter (`s_order`), SAME pending-pair logic (`s_pending`), SAME
  `s_last_world` publication and `world_name`/`sanitize`/`url_name` helpers as IA's
  detour pair, so no load is double-counted across sources. Two documented deltas:
  the loaded line carries `ok=<0|1>` (the engine LoadMap RETURN — false = the load
  failed, the exact save-load failure signature the mission is chasing), and the
  `GetThisCurrentWorld()` read is individually exception-isolated so a missing
  FWorldContext offset can only blank the name, never drop the marker (the URL
  still names the map).
- **`RC::Unreal::VmtFlmapNoteLoadMapPre/Post` + extern "C" trampolines** at the
  file bottom, mirroring the `vmt_engine_tick_drain` trampoline law (Prospero body
  + empty non-Prospero body; every fold links).
- **Comment fix** in `funcptr_marshal_hook`: the stale "NotifyOnNewObject at
  framework start" pointer now names the real mechanism ([FL-MAP-EV] vtable-slot
  hook; detour markers retained).

Scope held to Claude's ask: **LOGGING ONLY**. No Lua delivery — queued
RegisterLoadMapPreHook/PostHook callbacks stay undelivered exactly as the [R3]
refusal line states. No new offsets, no engine writes beyond the already-accepted
vptr swap, no header changes (tree-wide-recompile law respected).

---

## 3. Safety analysis — why this cannot repeat the launch-2 regression

The launch-2 kill mechanism was: a 40K-object engine scan executing INSIDE
`funcptr_marshal_hook` on the same game-thread call stack that dispatches keybind
Lua callbacks (before `process_simple_actions`/`process_delayed_actions`), at
carrier cadence, every second, for the whole session.

1. **The dispatch path is untouched.** `funcptr_marshal_hook`'s body (identity
   publication → raw gate → dispatch-scope → deferred detour arm → action queues)
   is byte-identical under this patch; so is `VmtEngineTickDrain`. The dead
   `marshal_watch()` stub is not even called. Zero new work runs between the
   keybind dispatch and its actions.
2. **The event source is not in any pump or drain.** `vmt_engine_loadmap_entry`
   executes ONLY when the engine itself calls LoadMap through the swapped vtable
   slot — map transitions, seconds-to-minutes apart, taking microseconds (two
   bounded, sanitized Output::send lines + one atomic increment each). Per-frame
   cost: zero (the Tick entry/drain is unchanged).
3. **The original call is sacred.** Pre-marker isolated → original forwarded
   exactly once, same receiver/args (bit-identical by ABI construction) → post
   marker isolated → original return value returned verbatim. A marker failure can
   neither omit nor duplicate the real map load (the drive_tick law, applied to
   LoadMap).
4. **No shared mutable engine state.** The entry touches no GUObjectArray walk, no
   Lua state, no dispatch mutexes — only its own atomics and the FlMap contract
   (two atomics + one pointer, all game-thread-confined: every LoadMap is an
   engine game-thread call, serialized by the engine itself; the detour source, if
   a future world ever arms it, serializes on the same engine call).
5. **Arming is fail-closed and atomic.** Both pokes land in the PRIVATE copy
   BEFORE the single aligned vptr publish; validation refusals are loud and leave
   the pump exactly as it is today (Tick-only or not published at all on
   DirtyCopy). The original vtable is never written; teardown is the existing
   `VmtTickPumpRetire` wholesale vptr restore (the poked copy is abandoned with
   the allocation).
6. **Fold isolation.** Everything compiles out identically in the non-Prospero and
   disarmed-R3 folds; the LuaMod trampolines keep every fold linking (empty bodies
   elsewhere).

Timing/coverage (the "missed the early maps" fix): the poke+publish happen at
framework start in the `bHookEngineTick` R3 block — strictly before the game's
first map load, so Level_Splash and every later map produce markers. Honest edge:
if the engine's default ENTRY map load completes before the initializer runs, that
one non-game map predates the hook (it is not a game map); the round-4 log shows
the initializer resolving the engine instance at boot, before game maps.

---

## 4. Honest limits

- `world_name` for `phase=loaded` depends on `FWorldContext::GetThisCurrentWorld()`
  (generated member accessor); if its offset is ever missing the name blanks but
  the marker survives with `url=` (isolated read — see patch).
- Both-sources worlds (a future successful PLH arm + this slot hook) log one
  pre+post pair PER SOURCE per load, each `src=`-tagged with sequential order
  numbers — noisy but honest; the harvest reads `src=` per source.
- Qualified engine-internal `UEngine::LoadMap(...)` calls (if the game's build ever
  emits one) bypass the slot hook — residual miss class, no stock-UE instance
  known on this path.
- The [FL-MAP-EV] hook does NOT resurrect Lua RegisterLoadMapPreHook/PostHook
  delivery (logging scope only, per Claude's ask).

## 5. Verification

- `patch -p1 --dry-run` on a pristine candidate copy: both files apply clean.
- One valid `-fsyntax-only` run (prospero-clang++, IA's standard recipe + 4
  documented recipe deltas for the UnrealInitializer TU): **EXIT=0, 0 errors**;
  the 6 file-level warnings are the known pre-existing vendor classes
  (-Wswitch, deprecated-enum-enum-conversion, undefined-inline, asmjit pragma,
  unused linker input); zero diagnostics inside any patched region. The two real
  bugs the check caught (PumpCore namespace qualification; vtable-entry slot store
  typed as void** instead of void*) are fixed in the deployed patch. Full
  invocation history in `syntax-receipt-UnrealInitializer-04.log`.
- The LuaMod.cpp additions were not separately syntax-checked (the one-check
  budget went to the novel-code TU); they mirror in-file proven patterns (FlMap
  neighbors, the drain trampoline) and compile risk is limited to those hunks.

## 6. Apply order

04 is independent of 02/02b/03's regions (02/02b: LuaUObject.hpp; 03: already
folded into this tree's LuaMod.cpp). Apply 04 to the CURRENT candidate tree state
(watch already disabled): `patch -p1 < 04-event-map-logger.patch` at the candidate
root. Next build then carries the event source; expected first-boot lines:
`[EU-VMT] copy ready ...` → `[FL-MAP-EV] LoadMap slot armed ...` →
`[EU-VMT] armed with the [FL-MAP-EV] LoadMap logger ...`, then per map:
`[FL-MAP] order=1 map='...' phase=pre src=vmt-loadmap` /
`[FL-MAP] order=1 map='...' phase=loaded src=vmt-loadmap url='...' ok=1`.

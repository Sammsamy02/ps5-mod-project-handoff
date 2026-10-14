# VTABLE-SWAP-DESIGN — the dispatch-data hook backend (hedge for W-fails/R-works)

Worker: worker-vtable-swap-design, 2026-10-04. PROPOSALS ONLY: nothing built, staged, or
executed; no console contact; no nested subagents. This is the pre-built hedge for the
`{W_FAILS_R_WORKS}` branch of the exec-writability decision tree
(`TEAM:worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md:198-205`, full analysis
`EXEC-WRITABILITY-PROBE-DESIGN.md:145-171`), so that a W-fails probe verdict does not start
design from zero. Read-order contract honored: worker-exec-writability's report + decision
tree were read first.

Path abbreviations (reused from the cited workers):
- `TEAM=` `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/`
- `CON=` `.../outputs/2026-10-04-zcode-controller/console/`
- `SRC=` `.../.superpowers/native-dependency-spike/native-coherent-runtime-1/source-reconstruction-final-4/` (canonical reconstruction per `TEAM:worker-all-signatures-inventory/SIGNATURES-INVENTORY.md:8-9`)
- `SINI=` `TEAM:worker-query-s2-arm/staging/UnrealInitializer.cpp` (the staged initializer per `SIGNATURES-INVENTORY.md:10-11`)
- `DET=` `.../.superpowers/native-dependency-spike/ue4ss-native-owned-default-detour-1/port/x64Detour.cpp` (the composed runtime's consumed PLH copy, `TEAM:worker-tick-address-validation/REPORT.md:82`)

---

## 0. Executive summary

1. **The swap surface is real and mostly already resolved data-side.** EngineTick (C4),
   ProcessEvent (C8), and the C3/C5-C7/C9 vtable family all resolve today by live vtable
   walks that perform exactly the two data reads a swap needs (vptr load, slot load) —
   `SRC:port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp:104-116,118-134`.
2. **One premise correction (load-bearing):** vtables on this target are NOT in proven-RW
   spans. The RELRO-covered segment L3 maps READ-ONLY (prot=1) at runtime — §1. A vtable
   swap therefore needs one new in-game fact: `mprotect R->RW` on a 16-KiB DATA page
   (syscall 74; mprotect is import-unavailable, `worker-exec-writability/CLAIM.json` EW-4).
   Exec pages are never written or read by this architecture.
3. **The thunk needs no allocation, no new imports, no code writes:** replacements are
   ordinary static functions in the PRX's own executable image; per-target saved originals
   live in module .bss; the "hook" is one aligned 8-byte store — §4.
4. **Minimal composition change:** a `DispatchDataDetour` behind the exact
   `PLH::x64Detour` interface (ctor / `hook()` / `unHook()` / `isHooked()`, writing the
   saved original into the `Trampoline` out-word), selected by a PROSPERO typedef at the
   single construction site — §5 R1. The entire callback machinery (Invoke, pre/post
   vectors, GC, PROSPERO invocation scopes) is preserved unchanged — §4.3.
5. **Zero-syscall tier:** `UFunction::SetFuncPtr` hooking already exists in-tree
   (`SRC:port/unreal/src/UFunction.cpp:78-90`) and can carry Lua `RegisterHook` without any
   mprotect — §5 R3 — but it cannot carry EngineTick (not a UFunction).
6. **Honest limits:** FName ctor verification (C1) is not dispatch-table reachable
   (direct `E8` call sites) — FC-1 replaces the verifier, not a hook; global
   ProcessInternal/ProcessLocalScriptFunction interception is lost (per-function FuncPtr
   substitutes); subclass vtable copies make ProcessEvent a family of swaps, not one — §6.

---

## 1. Memory-permission ground truth (where the dispatch tables actually live)

### 1.1 The retained live map of the game image

The igf3 build recorded the first decoded page of the live KERN_PROC_VMMAP census
(`CON:igf6-results.jsonl`, build `igf3-de0c0cb644d3`, seq 4, exp `v1_vmmap_page`, part 0 of
1024). The leading entries, verbatim prot values:

| live span | prot | = PT_LOAD (SEGMENT-CORRESPONDENCE §2, `TEAM:worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md:21-28`) |
|---|---|---|
| `[0x400000,0x787c000)` | 4 (X) | L0 `--X` [vaddr 0x0,0x7479b1c), rounded 0x747c000 — the trusted exec span (`worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md:26-31`) |
| `[0x787c000,0x82bc000)` | 1 (R) | L1 `R--` [0x747c000,0x7eba918), rounded 0xa40000 |
| `[0x82bc000,0x953c000)` | **1 (R)** | L3 `R\|W` [0x7ebc000,0x913c000), rounded 0x1280000 — **PT_GNU_RELRO (ph4) covers this span** (`SEGMENT-CORRESPONDENCE.md:25`) |
| `[0x953c000,0x9568000)` | 3 (RW) | L7 `R\|W` [0x913c000,0x9633418), rounded 0x4f8000 (head) |
| `[0x9568000,0x96dc000)` | 1 (R) | inside L7 — unexplained sub-region (see §1.3) |
| `[0x96dc000,0x9a34000)` | 3 (RW) | L7 (tail, contains BSS) |
| `[0x970c000,0x9a34000)` | 3 (RW) | (overlaps row above; the igf4 GUA span) |

Every one of these matches the per-segment translation `runtime = 0x400000 + vaddr`
(`SEGMENT-CORRESPONDENCE.md:72-95`): L0's match is proven
(`SEGMENT-CORRESPONDENCE.md:108-110`), and **L7's bias is validated by igf4**: the GUA read
record gives `rt_addr 0x97ba110 = 0x400000 + 0x93ba110`, inside the observed prot=3 span —
the exact value `SEGMENT-CORRESPONDENCE.md:92` had to leave "NOT derivable today …
UNVALIDATED" (`CON:igf6-results.jsonl` igf4 seq 6 `gua_read`). The uniform-bias model should
be treated as confirmed for L0/L1/L3/L7 as of the igf3/igf4 records, with per-target
mapscan verification retained as the operational gate anyway.

### 1.2 What this means for the swap premise

- UE C++ vtables (`.data.rel.ro`, containing relocated pointers) land by toolchain
  convention in the RELRO-covered segment → **L3 = runtime [0x82bc000,0x953c000), observed
  prot=1: READABLE but NOT writable.** The section headers of the retained capture are
  unreadable (`e_shoff` beyond EOF, `SEGMENT-CORRESPONDENCE.md:13-14`), so the definitive
  confirmation of a given vtable's span comes from (a) the LIVE vptr read at install time
  (the runtime itself reads `*(void**)instance`, `UnrealVirtualBaseVC.hpp:114` — the actual
  table address is a runtime fact, never an offline guess), then (b) a fresh mapscan of the
  covering span before any write (the proven discipline,
  `EXEC-WRITABILITY-PROBE-DESIGN.md` C-2). The offline witness method (relocation-backed
  vftable roots, `TEAM:worker-tick-witness-design/TICK-WITNESS-DESIGN.md:154-158`) can
  pre-compute predicted vtable vaddrs for log cross-checks only.
- **Consequence:** the W-fails branch statement "vtables live in serviced WRITABLE memory"
  (`EXEC-WRITABILITY-EVIDENCE.md:200-202`) is WRONG in this one respect. A vtable swap needs
  `mprotect(page, 0x4000, PROT_READ|PROT_WRITE)` on a prot=1 DATA page first — call this
  open fact **D-1**. D-1 is a *different page class* from IGW-1's H2 (exec pages, prot=4):
  exec is never involved; the loader itself re-protects RELRO ranges in-process during
  startup, which is a plausibility argument, not evidence (marked speculation, same status
  as `EXEC-WRITABILITY-EVIDENCE.md:154-160`). If D-1 fails, vtable swaps die with detours
  and only the heap tier (§5 R3) + polling survive (`EXEC-WRITABILITY-PROBE-DESIGN.md:172-188`).
- **What is proven writable today:** data read+write+restore inside a prot=3 span — igf4's
  96 B GUA read and same-value writeback at 0x97ba110
  (`CON:igf6-results.jsonl` igf4 seq 6 `gua_read`, seq 7 `gua_writeback`:
  `"restored":1,"method":"same_value_writeback_96B_8byte_stores"`). Heap objects
  (UFunction instances) live in engine-allocated memory; their spans are expected prot=3
  but must be mapscan-verified per target before any write (the [0x9568000,0x96dc000)
  R-only hole inside L7, §1.3, shows why per-target gating is mandatory).

### 1.3 The unexplained L7 hole (honesty marker)

`[0x9568000,0x96dc000)` sits inside L7's runtime extent but maps prot=1 (igf3 seq 4). No
retained analysis explains it (second RELRO region? overlaid mapping?). Nothing currently
bound lands there (GUObjectArray 0x97ba110 is in the prot=3 tail), but GNatives' address has
never been bound (slot 8, `SIGNATURES-INVENTORY.md:81`), so **any future GNatives work must
derive its vaddr offline first and check it against this census before assuming writability.**

---

## 2. The swap surface, per hook (task 1)

Legend: **slot** = the exact writable word; **swap** = what replaces it; **restore** =
reverse operation; **safety** = pre-write validation; **coverage** = which dispatches are
intercepted. All addresses/offsets come from runtime data (VTableLayoutMap / live vptr),
never from hardcoded constants.

| # | Target (inventory id) | Dispatch mechanism | Slot location + evidence | Swap / restore | Safety | Coverage verdict |
|---|---|---|---|---|---|---|
| 1 | **UEngine::Tick (C4)** — init-armed, `SINI:1312-1400`; registration `SINI:1383-1397` (`RegisterEngineTickPreCallback(HookedEngineTick,…)`, `QueryHarnessEngineTickDispatch`) | C++ virtual: engine calls `GEngine->Tick(...)` through the instance vtable (the port's own hooking premise, route B, `TEAM:worker-tick-address-validation/TICK-VALIDATION-PATH.md:92-106`) | ONE word: `vtable + VTableLayoutMap["Tick"]` where `vtable = *(void**)GameEngine`. The walk already computes both (`UnrealVirtualBaseVC.hpp:132-133`); the initializer cross-checks the slot's value against the scan address (`SINI:1315-1378`) | Store thunk address; save original into module .bss; `Trampoline = original` (§4.3). Restore = store original back + re-protect R (write-window restore; no unload-restore — §4.5) | Identity pre-check: slot value must equal the resolved Tick address before the store (refuses wrong-offset corruption loudly); span mapscan prot gate; D-1 mprotect for the write window | **COMPLETE for UGameEngine::Tick.** One class, one live instance; every virtual call site routes through the one slot. The query harness (`SINI:1394-1397`) is the in-game falsifier: observed ticks = swap works. First real swap target, loud-throw path intact (`SINI:1384-1387`) |
| 2 | **UObject::ProcessEvent (C8)** — `SINI:1448-1456`, default true (`SettingsManager.hpp:128`); carries ALL Lua `RegisterHook` traffic (`SRC:port/ue4ss/src/Mod/LuaMod.cpp:4542-4556` → `RegisterProcessEventPreCallback`, `SRC:port/unreal/src/Hooks/Hooks.cpp:83-106`) | C++ virtual on every UObject-derived instance | **A FAMILY of words:** every compiled-in UClass has its own vtable copy with a ProcessEvent slot. The walk uses the `Object` CDO (`SINI:1449-1456`) → one vtable only | Enumerate UClass CDOs via GUObjectArray (slot 0 is seed-bound data-side, `SINI:893-895`; iteration works-data-side, `SIGNATURES-INVENTORY.md:123,132`); dedupe vptrs; for each DISTINCT vtable whose slot value **identity-matches** the resolved ProcessEvent address: swap + save. Restore = same set, store-back | Per-slot identity pre-check (a slot that differs — override or wrong layout — is REFUSED and logged, never written); per-span mapscan; D-1 per 16-KiB page (many vtables share pages; batch per page: protect → swap all slots on page → verify → re-protect) | Full family swap = ProcessEvent parity with the detour. **Partial swap = silent per-class holes**; close them at registration time (R4, §5): verify the hooked function's class CDO vptr is in the swapped set, else loud error at `RegisterHook` |
| 3 | UEngine::LoadMap (C3), AGameModeBase::InitGameState (C5), AActor::BeginPlay/EndPlay/Tick (C6), UGameViewportClient::Tick (C7) — `SINI:1303-1311,1403-1447` | C++ virtual | Same as row 1/2 mechanics | Same | Same identity pre-check | Same class-copy caveat as row 2 for AActor-family (huge subclass surface). Consumers today: begin/end-play + actor-tick callbacks. Lower priority than rows 1-2; begin as single-class swaps only where a live instance is found (`SINI:1295-1300` disables ViewportClient when absent) |
| 4 | UObject::ProcessConsoleExec, UStruct::Link (C9) — `SINI:1457-1474` | C++ virtual | Same | Same | Same | Works but **not-on-path** (console keys zeroed, `SIGNATURES-INVENTORY.md:130`) — skip |
| 5 | **GNatives table (slot 8)** — `GNatives_Internal = results.gnatives` (`SINI:897-899`), declared `extern COREUOBJECT_API FNativeFuncPtr* GNatives_Internal` (`SRC:port/unreal/include/Unreal/Script.hpp:136`) | Game's BP-VM opcode→native-func table; the port's OWN `FFrame::Step` reads it for its own stepping (`SRC:port/unreal/src/FFrame.cpp:204-214`); game-side ProcessInternal uses the game's copy | The table is a mutable global function-pointer array. **Address never bound** (slot 8 unbound today, graceful degradation `SIGNATURES-INVENTORY.md:81,189`); segment class unverified (§1.3) | Entry swap = same 8-byte store mechanics per index | Requires offline vaddr derivation + span-class check FIRST; none exists | **Designed but address-unproven.** Note: the port's own Step needs only the BINDING (read), not a swap — interception value is game-side VM opcodes only. Rank last |
| 6 | **UFunction FuncPtr (per-function)** — `UFunction::GetFunctionHookData()` swaps FuncPtr to `UnrealScriptFunctionHook` (`SRC:port/unreal/src/UFunction.cpp:78-90`); hook body `SRC:port/unreal/src/UFunctionStructs.cpp` (falls back to ProcessInternal via ExecuteUbergraph's FuncPtr when no original) | The engine's ProcessEvent consults `Func->Func` — a pointer field in a HEAP object, not a vtable | The UFunction object's FuncPtr field (heap, expected prot=3; mapscan-verify per target) | SetFuncPtr swap; original saved in `HookedUFunctionMap` (already in-tree) | None needed beyond heap-span mapscan (write class proven igf4, §1.2) | **Zero-syscall tier (R3).** Covers every reflected call of that function (all ProcessEvent traffic reaches FuncPtr). Cannot cover EngineTick (C++ virtual, not a UFunction) |
| 7 | **FName constructor (C1)** — verifier detour, `SINI:929-988`, throw at `:954-958` | **NOT dispatch-table reachable**: direct `E8 rel32` call sites (9 materialized, `TEAM:worker-fname-fc1-materialize/FC1-CHECKS.md` §2) | none | **No swap possible — and none needed.** The detour's only role is verification ("throw on failure"); replace it with the FC-1 byte-equality validator (exact expected 32/64 bytes + 9 call-site windows, FC1-CHECKS §1-2), which is read-only and launchable once RX works (H1) | FC-1's own gates | **Replaces C1 entirely** (detour dead in W-fails; validator carries the verification function) |
| 8 | ProcessInternal / ProcessLocalScriptFunction (E5, C10/FLAG-3) | Reached as UFunction FuncPtr values (script functions' FuncPtr = interpreter), NOT vtables — derivation currently reads code (`SINI:1497-1557`, FLAG-3, `SIGNATURES-INVENTORY.md:231-236`) | Per-UFunction FuncPtr words (row 6 mechanics) applied to script UFunctions enumerated via GUObjectArray | Per-function swap; **global** ProcessInternal/ProcessLocalScriptFunction interception is NOT achievable via swaps | Identity pre-check where a resolved interpreter address is available (needs RX per FLAG-3 remedy, or seed-style offline derivation) | **Degraded: per-function yes, global no.** Settings kill-switches remain the composed fallback (`HookProcessInternal=false`, `HookProcessLocalScriptFunction=false`, `SIGNATURES-INVENTORY.md:236`) |

### 2.1 What the slot identity pre-check buys

PLH patches the target prologue with no semantic validation at all — "nothing refuses a
wrong Tick address" (`TEAM:worker-tick-address-validation/REPORT.md:29-32,188-194`). The
swap backend is structurally safer: the ONLY safe write is into a word whose current value
equals the independently resolved original address (vtable-vs-scan cross-check for Tick,
`SINI:1315-1378`). Wrong offset, wrong layout, wrong class, or mid-engine vtable change ⇒
the pre-check refuses and the init thread throws LOUDLY (`SINI:1384-1387` shape), instead of
patching hot code and crashing at the next call.

---

## 3. The write window: mprotect discipline for swaps (D-1)

Per swap session (init thread, single-writer, after `module_start` returned):

1. Fresh trusted mapscan (sysctl 202 — proven import, `EXEC-WRITABILITY-PROBE-DESIGN.md:40-45`);
   locate the span covering the slot; **gate**: prot must be 1 (vtable page) or 3 (heap
   FuncPtr page). prot=4 ⇒ refuse (never touch exec).
2. If prot=1: raw syscall 74 `mprotect(page,0x4000,PROT_READ|PROT_WRITE)` (constants
   build-parsed from the pinned SDK, C-6 discipline). Record rc/errno write-through BEFORE
   proceeding. rc!=0 ⇒ D-1 refuses ⇒ this hook does not arm (loud), others may still try
   their own pages.
3. **Identity pre-check** (§2.1): read slot (readable: prot 1 or 3 both include R); require
   slot == expected original entry.
4. Store thunk address (single aligned 8-byte store). Readback-verify. Fresh mapscan optional
   here (prot still RW) but the store itself is verified by readback.
5. Re-protect `PROT_READ` + fresh mapscan + verify prot back to 1. Anomaly ⇒ fail-closed
   (stop the ladder; do not continue installing from an unrestorable page) — the igf
   restore discipline (`EXEC-WRITABILITY-PROBE-DESIGN.md` C-4, seq 8 shape).
6. Batch optimization for row 2 (ProcessEvent family): one protect/verify/re-protect cycle
   per 16-KiB page covering many slots, never two pages in flight (C-4).

D-1 as a probe: a standalone falsifiable design (igf4-style **same-value writeback** on one
far page of [0x82bc000,0x953c000): read 8 B, mprotect RW, write the SAME 8 B, readback,
re-protect R, mapscan — zero semantic delta even in the crash case, localizing any fault by
missing record, C-3). This is deliberately NOT designed further here (one bounded design per
worker; IGW-1's residual-risk statement
`EXEC-WRITABILITY-PROBE-DESIGN.md:198-208` applies unchanged to syscall 74 on data pages:
if 74 is ring-0-fatal in general, D-1 kills the console the same way). Sequencing: D-1 is
meaningful ONLY after IGW-1's outcome is reconciled; if IGW-1 is never run, D-1 inherits its
go/no-go posture (controller+user).

---

## 4. The thunk (task 2)

### 4.1 Where thunk code lives

Ordinary static functions in the modbridge PRX's own .text — the module's code demonstrably
executes in-game (entry/startup/mapscan records run module code,
`CON:igf6-results.jsonl` seq 1-3 shapes). **No mmap, no RWX anon pages, no allocation at
all** (the exec-writability sketch's `mmap(PROT_READ|PROT_WRITE|PROT_EXEC)` idea,
`EXEC-WRITABILITY-PROBE-DESIGN.md:165-167`, is unnecessary: our code is already executable
in ITS OWN pages). Per-target state lives in module .bss:

```
struct SwapTarget {                 // one per hooked slot (or slot-set)
    void**          SlotAddress;    // vtable+i or &UFunction::FuncPtr
    void*           OriginalEntry;  // saved before the swap
    std::atomic<void*> Armed;       // release-set after OriginalEntry is committed
    // backend bookkeeping (detour id, coverage set for row 2, ...)
};
```

### 4.2 Entry shape

The redirect entry is exactly the port's existing `StaticHookFn` (per-target template
instantiation, `SRC:port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp:621-630`):
SysV AMD64, signature = the original's (typedefs at
`SRC:port/unreal/include/Unreal/Hooks/Hooks.hpp:98-114`; e.g. EngineTick
`void(UEngine*, float, bool)` at `:107`, ProcessEvent `void(UObject*, UFunction*, void*)`
at `:98`). The game's virtual call loads the slot into a register and `call`s it — our
function receives `this`/args exactly as the original would. **No `ms_abi` anywhere: PS5 is
SysV end to end** (the ms_abi concern is a Windows-PLH artifact). PROSPERO invocation
accounting (`FNativeInvocationScope`, `DetourInstance.hpp:100-115,623-626`) and
`FNativeCallbackScope` self-unregister exclusion (`:65-95`) apply unchanged because entry
into `StaticHookFn` is the same boundary it always was.

### 4.3 Calling the original — the saved-entry discipline

`TDetourInstance::Invoke` calls the original through the PLH trampoline word:
`PLH::FnCast(Trampoline, TargetFunction->get_function_pointer())(Args...)`
(`DetourInstance.hpp:487` for void, `:499` for non-void). The swap backend keeps that
contract by writing the **saved original entry** into `Trampoline` (the ctor's
`uint64_t* userTrampVar` out-param, `DET:30`) instead of a re-encoded trampoline. Because
the original's instruction bytes are untouched, "the trampoline" is simply the original
function — prologue-replication, `calcNearestSz`, `makeTrampoline`, and the entire
disassembler path (`DET:348-545`) are never used. The full Invoke semantics (pre-callbacks,
OriginalFunctionCallPrevented, post-callbacks, return-value override, `DetourInstance.hpp:474-512`)
survive byte-for-byte.

### 4.4 Ordering and the swap moment (thread-safety)

- **Commit order (strict):** (1) `SwapTarget` fully initialized; (2) `OriginalEntry`
  stored; (3) `Armed.release(original)`; (4) slot store (single aligned 8-byte, x86-64
  atomic); (5) readback-verify; (6) re-protect R + mapscan.
- Readers (game threads doing virtual calls) load the slot unlocked; an aligned 8-byte load
  never tears — a call lands on the original or the thunk, never a mix. A thread that loaded
  the original just before the store completes its call into unmodified code — **the classic
  PLH prologue-patch race (a thread executing the prologue mid-patch) does not exist in this
  architecture because no instruction byte ever changes.**
- The thunk reads `Armed` (acquire) before its first use of `OriginalEntry`; if not armed
  (impossible under the commit order, but defensive), tail-call the original via the slot's
  pre-swap value cached in the thunk's own .bss cell.
- Re-entrancy: EngineTick/ProcessEvent callbacks re-entering the same slot recurse through
  StaticHookFn exactly as with PLH; existing PROSPERO recursion scopes apply
  (`DetourInstance.hpp:65-95,623-626`).
- Multi-thread init: all swaps happen on the one init thread behind
  `InstallMutex` (`DetourInstance.hpp:232-239`); no second writer exists (serialize editor/
  heavy-writer discipline).

### 4.5 Restore discipline — and its honest limit

- **Write-window restore** (re-protect R, mapscan verify) is mandatory and fail-closed (§3).
- **Teardown restore does not exist and must not be promised:** the port's PROSPERO
  `DeactivateHook` already refuses to unhook and keeps forwarding
  (`DetourInstance.hpp:191-205`), and `DeactivateHookAtExternalQuiescence` requires an
  authority no provider supplies (`:207-226`). Swap hooks are process-lifetime, matching
  that precedent; the backend exposes `unHook()` as "restore slot only under the same
  external-quiescence conditions PLH's `unHook` would need" and otherwise returns false.

---

## 5. Port integration sketch, ranked by blast radius (task 3)

**R1 — backend typedef (smallest, carries C4 end to end).**
Add `DispatchDataDetour` exposing the exact consumed-PLH surface: ctor
`(uint64_t fnAddress, uint64_t fnCallback, uint64_t* userTrampVar)` (`DET:30`), `hook()`,
`unHook()` (`DET:547`), `isHooked()`. At the single construction site
(`DetourInstance.hpp:164-166`) select it under PROSPERO:

```
#ifdef __PROSPERO__
  using DetourBackend = DispatchDataDetour;   // swap-based; fnAddress = registry key
#else
  using DetourBackend = PLH::x64Detour;
#endif
```

`hook()` resolves `fnAddress` in the slot registry (R2), performs §3, sets
`*userTrampVar = original`, returns false on any gate refusal → existing
`EnsureHooked`→`ERROR_ID`→loud-throw path fires unchanged
(`DetourInstance.hpp:282-288`, `SINI:1384-1387`). Zero changes to Invoke, callback vectors,
GC, scopes, RemoveCallback. Cost: one new file + one typedef + registry.

**R2 — slot capture at the walks.** The walks already compute the slot address and value
(`UnrealVirtualBaseVC.hpp:114-115,132-133`). Add a PROSPERO-only recorder (macro variant of
`GET_ADDRESS_OF_UNREAL_VIRTUAL`, or a registration call at the `SINI:1303-1474` sites) that
pushes `{slot_addr, entry_value, class_instance}` into the backend registry keyed by
`entry_value` (= `fnAddress` R1 receives). No behavior change without the R1 backend armed.

**R3 — zero-syscall tier for Lua hooks (independent of D-1).** Route the Lua
`RegisterHook` overloads that land in `RegisterProcessEventPreCallback`
(`LuaMod.cpp:4542-4556`) to
`UFunction::RegisterPreHook/RegisterPostHook` (`SRC:port/unreal/src/UFunction.cpp:92-113`)
instead of the ProcessEvent detour. The `UnrealScriptFunctionHook` machinery already handles
original-invocation and ProcessInternal fallback (`UFunctionStructs.cpp`). Composition-side
change confined to LuaMod + the registration wrappers; works on heap FuncPtr writes alone.
Limit vs ProcessEvent-global: only functions whose UFunction object we can find + swap
(identical set RegisterHook targets anyway); loses "see every event" global observers (the
asset-loaded default hook noted at `DetourInstance.hpp:51`).

**R4 — ProcessEvent full family (D-1-dependent, largest).** GUObjectArray enumeration +
vptr dedupe + per-page batched swaps (§2 row 2), plus registration-time coverage
verification so a `RegisterHook` on an unswapped class fails loudly instead of silently.

**C1 — FC-1 validator replaces the FName detour** (independent; prepared:
`TEAM:worker-fname-fc1-materialize/FC1-CHECKS.md`; needs only RX per IGW-1 H1 to run its
64-byte + 9-window reads — it is a verifier, never a hook). Kill the detour registration at
`SINI:945-958` under a settings gate, run FC-1's checks, throw on mismatch (same
verification contract, zero interception).

**Sequencing recommendation:** R1+R2 (Tick first, loud-throw) → C1 → R3 → R4. EngineTick
remains the first real hook under all branches (it is also the W-works branch's first
target, `EXEC-WRITABILITY-PROBE-DESIGN.md:139-141`).

### 5.1 Branch mapping (post-probe)

| IGW-1 verdict | Swap-backend role |
|---|---|
| W-works | Unneeded as fallback; optionally still preferred for Tick (identity pre-check beats PLH's unvalidated patch, §2.1) |
| W-fails, R-works | **Primary**: R1/R2 for all vtable targets (needs D-1), C1 for FName, R3 for Lua hooks, FLAG-3 reads unblocked by RX (`EXEC-WRITABILITY-EVIDENCE.md:203-205`) |
| Both-fail | R1/R2 die with D-1; R3 (heap tier, no mprotect) + polling replacement for closures survive (`EXEC-WRITABILITY-PROBE-DESIGN.md:178-186`) |
| Fatal syscall fault | Same as both-fail for hooks; no replay of any syscall variant (`zcode_igf5_diagnosis` action rows) |

---

## 6. Honest limits (task 4)

1. **Direct/static calls cannot be swap-intercepted.** The FName ctor (9 direct `E8` sites,
   FC1-CHECKS §2), StaticConstructObject (slot 4 constant, called directly — its
   `EDetourTarget` exists, `SRC:port/unreal/include/Unreal/Hooks/Internal/Metadata.hpp:141-148`,
   but no dispatch table carries it), CallFunctionByNameWithArguments/ULocalPlayerExec
   (`Metadata.hpp:245-260`, not-on-path) are unreachable by any data swap. Cost: none of
   them is an init-armed hook today except C1 — and C1 is verification-only, fully replaced
   by FC-1 (§5). No acceptance-ladder loss identified for these.
2. **Global ProcessInternal/ProcessLocalScriptFunction interception is lost.** Those
   functions are invoked via FuncPtr values baked into thousands of script UFunction
   objects, not a vtable (§2 row 8). Per-function FuncPtr swaps cover the same functions
   individually (and `UnrealScriptFunctionHook` already exists), but "hook every BP entry
   globally" (some UE4SS features/mod styles) degrades to per-function or dies. The
   ProcessInternal *address derivation* itself still needs RX (FLAG-3,
   `SIGNATURES-INVENTORY.md:231-236`) or offline constants — swaps do not remove that
   dependency.
3. **ProcessEvent parity requires the whole family.** One-vtable swap ≠ global ProcessEvent:
   every UClass vtable copy carries its own slot (§2 row 2). Partial swaps create SILENT
   per-class holes — mitigated by R4's registration-time coverage check (loud), which must
   be an acceptance requirement, not optional. Cost: E4 (Lua RegisterHook, E4 row
   `SIGNATURES-INVENTORY.md:167`) is only "works" under R4-complete or R3-routing; a lone
   UObject-CDO swap must never be counted as E4 coverage.
4. **AActor-family coverage is large.** BeginPlay/EndPlay/Tick subclasses override or copy
   slots per class; the same family logic applies (§2 row 3). Unbudgeted today.
5. **EngineTick bypass paths are unproven either way.** The design assumes engine tick
   dispatch is virtual through the instance vtable (the port's own route-B premise). If any
   engine site called `Tick` non-virtually (baked direct call), the swap would miss it — the
   query harness counter (`SINI:1394-1397`) is the in-game falsifier; until it counts ticks,
   no "tick hook works" claim is allowed.
6. **D-1 is a real unproven fact with real risk** (§1.2, §3): swaps are NOT a free lunch —
   they trade "write exec pages" for "make a data page temporarily writable", a different
   but still-unverified syscall-74 operation, and the both-fail branch keeps only the heap
   tier (R3) + polling.
7. **No telemetry-free claims:** every acceptance statement ("EngineTick hook live", "Lua
   RegisterHook fires") must rest on the existing evidence instruments (query harness
   two-consecutive-reads, `SINI:1388-1397`; PlayMode/capture regression runs), never on the
   design. This document contains zero acceptance claims.
8. **Nothing is staged.** All of §5 is proposal; the composed runtime still dies at A1/A2
   before any of this matters until the seed/settings remedies land
   (`SIGNATURES-INVENTORY.md:243-246`, FLAG-5).

---

## 7. Explicit non-claims

- No claim any swap, thunk, mprotect-on-data, or hook has run in-game.
- No claim about UE 5.6 vtable index values, GNatives' address/segment, or the exact ELF
  section hosting any given vtable (runtime vptr + mapscan are the authorities; offline
  derivations are log cross-checks).
- No claim that D-1 succeeds, or that syscall 74 is safe on data pages because it might
  succeed on exec pages, or vice versa.
- No claim of PLH parity beyond the listed interface (disassembly-based refusals, jmp
  following, prologue-size checks are PLH behaviors the swap backend does not need and does
  not reproduce).
- No modification of any frozen/wiring file; no console contact; nothing executed.

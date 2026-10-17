# Mac session brief — Townfall Item Spawner: menu + spawn

Written 2026-10-06 by a cloud session that has the GitHub copy of `main` but
NOT the Mac-only build inputs (initializer `782dce71`,
`.superpowers/native-coherent-runtime-1/source/port/`, the PS5 SDK, built
candidates) and no route to the console. Everything below is source/doc
analysis plus host-side tool fixes; nothing here was built or run on the
console. Read this, then `OVERNIGHT-STATUS.md`
(`outputs/2026-10-04-zcode-controller/incident-loading-regression/`).

Branch with the fixes: `claude/townfall-item-spawner-hyj9eg` (= `main` +
tool fixes + three staged source fixes). Merge it into the Mac worktree
before pulling evidence.

**Summary.** Three independent blockers stand between today's boot and a
visible menu; all three now have staged, host-tested fixes (§3):

| # | blocker | where it bites | staged fix |
|---|---|---|---|
| B1 | GATE-1: the vtable resolve follows a possible jmp thunk by READING exec-only text; the walkguard refuses and init unwinds | `Finding GameEngine::LoadMap` (every boot so far) | ASMHelper no-jmp-follow on PROSPERO (§3.1) |
| B2 | the game owns the pad: `scePadOpen` → `ALREADY_OPENED`, so input is deferred forever | OPTIONS never reaches `RegisterKeyBind(F7)` | borrow the game's handle via `scePadGetHandle` (§3.2) |
| B3 | facf993a's marshal only arms NATIVE carriers; both ranked carriers are BlueprintImplementableEvents (no `FUNC_Native`) → never arms | every `ExecuteInGameThread` (all menu/spawn work) queues forever | arm the Blueprint overrides of the per-frame events + lazy re-arm (§3.3) |

## 0. Why the 2026-10-06 launch showed eight OK lines and no menu

Expected for the staged build line. "OK" is the ploader injection line only
(USER-LAUNCH-GUIDE §1). The pool-validation candidate `12fcc41f`, and the
planned "candidate 2" too, keep GATE-1 armed: the initializer reaches
`Finding GameEngine::LoadMap` (settings `HookLoadMap=true`), the walkguard
refuses the executable-text read, and init unwinds fail-closed. No event loop,
no mods, no pad thread, so OPTIONS has nothing to deliver to
(POSTPOOL-CANDIDATE.md §0, §4 Step 3–4). No build in the current ladder can
show the menu until GATE-1 is avoided or passed (§3 below).

## 1. Tool fixes on this branch (use these, not the old copies)

| file | defect | effect before fix |
|---|---|---|
| `incident-loading-regression/pull_exact.sh` | `grep -oP` (macOS grep has no `-P`) | session always `unknown` → summary bound to the max pid in the cumulative sdiag, which after a console reboot (pids restart low) is an OLD boot |
| `incident-loading-regression/morning_stage.sh` | uploaded `/tmp/sink-seed.bin` before creating it | after a Mac reboot `curl -T` failed and `set -e` aborted before the candidate was staged |
| `incident-loading-regression/analyzer_v2_ctrl.py` | did not recognise `status.txt`/`ue4ss.log`/`sdiag.jsonl` (the names `pull_exact.sh` writes) | "no recognizable evidence files" on every pull folder |
| same | grep keys only populated when `UE4SS.log` present | `KeyError: 'resolution'` crash whenever the log is absent/empty (which the manifest calls normal) |
| same | default `--epoch-view` path resolved under the controller dir | per-epoch correlation table always skipped |
| same | (new) | warns and records `pid_disagreement` when status session pid ≠ chosen epoch pid; re-run with `--max-pid <session>` |

Checks run here: `bash -n` on both scripts; awk session parse on the
fixture status file; analyzer on a synthetic pull folder (old high-pid boot +
new low-pid boot) with and without `--max-pid`; differential run of old vs new
analyzer on a synthetic run with a log — identical except the new key and the
now-working epoch-view section. No archived raw console evidence is in git, so
no regression run against real pulls was possible here.

## 2. First step on the Mac: classify the 2026-10-06 launch (game may stay open)

Pulling is read-only FTP; it does not need the game closed.

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
git fetch origin claude/townfall-item-spawner-hyj9eg && git merge origin/claude/townfall-item-spawner-hyj9eg
bash outputs/2026-10-04-zcode-controller/incident-loading-regression/pull_exact.sh launch-20261006
# pull_exact writes to /tmp/pulls/<label>-<ts>; copy it under outputs/ before
# anything else (/tmp is cleared on reboot; host sinks belong in outputs/)
python3 outputs/2026-10-04-zcode-controller/incident-loading-regression/analyzer_v2_ctrl.py \
  --dir <that dir> --label launch-20261006 --build-id 12fcc41f
```

Questions this pull must answer (all from the fetched bytes, per the analyzer):
1. Which build ran? (session pid from `status.txt`; if the analyzer prints a
   pid WARNING, re-run with `--max-pid`.) If the module never started, the
   status file is a previous boot's.
2. C1 pool verdict: ADMITTED-VIA-COPYIN expected (capture-first initializer).
3. Did the log reach `Finding GameEngine::LoadMap` + the `[WalkRxGuard]
   REFUSED` pair (GATE-1, predicted) or stop earlier?

If C1 is not ADMITTED or the stop is earlier than GATE-1, fix that first; the
rest of this brief assumes the predicted GATE-1 stop.

## 3. Staged source fixes (all under `outputs/2026-10-06-claude-cloud/staging/`)

Each is a full replacement file + a `.diff` against the pinned base + a
`host-test/run.sh` (macOS: `brew install zydis fmt` for 3.1, `fmt` for 3.3).
None was compiled with the PS5 SDK or run on the console.

### 3.1 B1 — ASMHelper no-jmp-follow (`asmhelper-nojmp/`)
- Base `694af016` (EDITIONS "asmhelper fold-0 staging") → `ASMHelper.guarded.nojmp.cpp` sha `e8d5f98a64017779…`.
- On `__PROSPERO__`, `resolve_function_address_from_potential_jmp` returns the
  data-sourced entry (vtable slot / UFunction Func) unchanged, logs
  `[WalkRxGuard] jmp-follow skipped (exec-only text; no read): using entry 0x… as-is`,
  and reads nothing. `get_first_instruction_at_address` keeps its guard for
  every other caller (`resolve_jmp/resolve_call` still refuse).
- Why it is correct: the jmp-follow exists for MSVC incremental-link thunks;
  this ELF target has none, and an Itanium this-adjust thunk starts with
  add/sub so the old path would return it unchanged too. Calls through the
  entry are equivalent; only an inline-detour target could differ, and every
  detour is refused (R3 mode + owned-memory soft-refuse, ADMITTED-CHAIN-MAP step 19).
- Host test: the pinned helper reproduces the exact GATE-1 throw text; the
  new helper returns entries with no read; host behavior unchanged.
- Effect: the boot walks ADMITTED-CHAIN-MAP steps 14–24 with zero text reads
  (LoadMap/Tick/… addresses assigned from vtables, R3 deferral, PostInitialize
  noise, `engine-ready`, `mods-started`, event loop, the banner). The
  `HookProcessInternal=0` / `HookProcessLocalScriptFunction=0` keys MUST be in
  the staged settings (step 18's unguarded 164-byte decode otherwise crashes).
- It also gives `UObject::ProcessEventInternal` its address (the
  `HookUObjectProcessEvent` block), which every Lua UFunction call and
  `FText(...)` (built via `KismetTextLibrary:Conv_StringToText`) needs.

### 3.2 B2 — borrowed pad handle (`pad-borrow/`)
- Base PS5PadNative `501d77af` → `port/PS5PadNative.borrow.cpp` sha `c4d2c7d635e6c9cf…`.
  Only the backend wrappers change; `PS5PadSource.hpp`/ABI and all other pad units untouched.
- On `ALREADY_OPENED`, `scePadGetHandle(user, STANDARD, 0)` (declared at
  `SDL_ps5joystick.h:106`) returns the game's handle; we read it with the
  non-consuming `scePadReadState` and never close it.
- **Adds one import row (`scePadGetHandle`, libScePad): the per-build import
  audit moves 196 → 197.** The NID is name-derived; if the loader cannot
  resolve it the module fails to load loudly (FAIL line), it does not crash the game.
- sdiag: `pad_open_class=1` (borrowed) then `pad_first_state_read`; or
  `pad_borrow_failed=<raw rc>` and input stays deferred (old behavior).
- Host test (real pinned `PS5PadSource.cpp` d2f14de1, fake libScePad, ASan+UBSan): 17/17.
- Bindings stay in `ue4ss/PS5Pad.ini` (`OPTIONS = F7`, D-pad = arrows,
  `CROSS = RETURN`). If OPTIONS also opens the game's pause menu (likely),
  `TOUCH_PAD = F7` is a zero-rebuild alternative.
- Use it with the input-tag Handler `16fcfe49` + PS5PadSource `d2f14de1` and
  the coexist NativePadInput `03c9f6d0` (POSTPOOL §2.1 rows 2–3): the
  deferred-attach Handler is what lets the six `RegisterKeyBind`s register.

### 3.3 B3 — event-carrier marshal (`luamod-event-carrier/`)
- Base LuaMod `facf993a` (the merged r3-ladder + gate5 file, "ready for
  candidate 2") → `LuaMod.cpp` sha `8bd12c623bab4e35…`; 185 diff lines.
- `r3_marshal_arm_event_carriers()`: finds every UFunction whose SuperStruct
  chain reaches `/Script/Engine.Actor:ReceiveTick`,
  `/Script/Engine.AnimInstance:BlueprintUpdateAnimation` or
  `/Script/UMG.UserWidget:Tick` (i.e. the Blueprint OVERRIDES the engine
  actually invokes via ProcessEvent → Invoke → Func on the game thread), and
  arms `funcptr_marshal_hook` on each with the existing `RegisterPostHook`
  Func swap. Natives / null Func / the engine bases are skipped.
- Called from the calibration consumer at program start and lazily from
  `funcptr_marshal_usable()` when work is queued (≤1 scan/s; rescans when
  armed carriers stop firing, e.g. after a level unload).
- `funcptr_marshal_usable()` is now "pump_pending" (+ lazy arm), so in the
  marshal world `ExecuteInGameThread` and the WithDelay family queue onto the
  marshal even before a carrier arms (closures await it) instead of onto the
  refused EngineTick/ProcessEvent pumps; `ExecuteInGameThread` takes it first.
- Host test: the 151 changed lines extracted verbatim and compiled against
  stand-ins, 11/11 (ASan+UBSan). The full TU needs the Mac port headers.
- Comparator law: LuaMod changes → the luamod comparator regen is REQUIRED
  this time (COMPARATOR-REGEN-PROCEDURE); facf993a's R3Marshal cross-TU
  references resolve with initializer `782dce71` (worker-r3-import-fix).

## 4. Build the next candidate on the Mac (controller-owned cascade)

Start from the 12fcc41f working set (EDITIONS.json). Changes:

| unit | from | to |
|---|---|---|
| ASMHelper (walkguard fold-0 unit) | `694af016` | `e8d5f98a` (3.1) |
| LuaMod (`compose.luamod_merged`) | `c92c92ff` | `8bd12c62` (3.3) |
| native-pad-PS5PadNative | `501d77af` | `c4d2c7d6` (3.2) |
| input-handler / native-pad-PS5PadSource / native-pad-input | admitted pad rows | `16fcfe49` / `d2f14de1` / `03c9f6d0` (POSTPOOL §2.1, §3 S2) |
| initializer | `782dce71` | unchanged |
| settings ini | `b980cea7` | unchanged — verify it carries `HookProcessInternal=0`, `HookProcessLocalScriptFunction=0`, `[EngineVersionOverride] 5/6`, `HookUObjectProcessEvent=true`, `HookEngineTick=true` (the R3 pump_pending flag is set inside the Tick block) |
| optional | bridge `497348b8` → `41dd0c15` (gameid); in-session units with io slots NULL (POSTPOOL §2.4) | only if they compose on the first try — do not let them delay the menu launch |

Then: update EDITIONS.json shas → luamod comparator regen →
`launch11_one_pass.py verify` → `--full` (expect the import audit at 197:
+`scePadGetHandle`; any other UND drift is a stop) → determinism twin →
package → stage with `morning_stage.sh` (pointing at the new candidate dir)
plus `ue4ss/PS5Pad.ini` and the settings ini, byte-exact readback.

Offline pre-check worth doing before the launch (existing analysis tools, the
decrypted eboot + RELA reconstruction, KB §B2): predict the UEngine vtable
entry at the staged VTableLayout `Tick` offset. It must be linked `0x2fac8c0`
(runtime `0x33ac8c0`, KB §A1). Do the same for UObject `ProcessEvent`. If
either prediction disagrees with the layout, fix the layout first: a wrong
ProcessEvent slot would crash the first UFunction call (at menu open).

The in-session service (grammar v2: `noop`, `diag`, `log`, `io_probe`,
`pool_probe`) proves liveness and re-runs probes with the game open; it
cannot press keys, run Lua or open the menu, so it does not replace the launch.

## 5. The one launch, and how to read it

User: Townfall fully closed → arm ploader → launch → eight OK, no FAIL →
load a save → wait ~30 s → controller checks the log (below) → press OPTIONS
once → if the menu shows, D-pad down to a pickup row, CROSS once.

| evidence (UE4SS.log unless noted) | meaning |
|---|---|
| `[WalkRxGuard] jmp-follow skipped … using entry 0x… as-is` then `GameEngine::LoadMap address 0x…` | B1 fix active; GATE-1 passed with zero reads |
| `GameEngine::Tick address (vtable: 0x33ac8c0; scan: …)` (fmt prints pointers in lowercase hex) | vtable layout offsets confirmed against the offline-derived Tick (any other value: STOP before pressing OPTIONS) |
| `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` | pump_pending set (marshal world) |
| `ProcessEvent address 0x…` | compare with the offline prediction before pressing OPTIONS |
| PolyHook SEV + one Error after `Hooking StaticConstructObject for searcher pools` | expected soft-refuse noise (chain-map step 19) |
| runtime-status `engine-ready`, `mods-started` | first time ever past init |
| `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` | mods running |
| sdiag `pad_open_class=1`, `pad_first_state_read` | B2: controller shared with the game |
| `[R3] event carrier scan: N override(s) found, M newly armed…` / `[R3] FuncPtr-marshal event carrier armed: /Game/…` | B3 carriers armed (may first appear only after the save loads / OPTIONS) |
| `[R3] marshal is the game-thread pump …` | first carrier fire on the game thread |
| `[TownfallItemSpawner] Menu created.` | menu built; it should be on screen |
| `[TownfallItemSpawner] Spawned <item> x1 ID=…` | item spawn succeeded |

Distinct failure shapes: no `jmp-follow skipped` line → the old ASMHelper
object was linked; log ends at `Finding ProcessInternal` → settings keys
missing; `pad_borrow_failed` → GetHandle refused (input stays deferred, try
`scePadOpen` class sweep of PROPOSAL-ALTERNATE-COEXIST-ROUTE P1);
`event carrier scan: 0 override(s)` after a save is loaded → this title's
per-frame Blueprints are not reached via these three events (next candidates:
`/Script/Engine.HUD:ReceiveDrawHUD`, `PlayerController` BP ticks);
`UI creation failed: …` → read the Lua error text (UMG via reflection).

## 6. Not done here / open

- No SDK compile of any staged file (headers are Mac-only); host tests only.
- USB keyboard input is not implemented (no keyboard source in the port).
- Universal (non-Unreal) runtime and the app are out of this critical path.

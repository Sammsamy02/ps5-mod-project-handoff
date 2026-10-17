# l24 in-game evidence + l25 scope update — for Claude review

Date: 2026-10-08. Assembled by Zcode from the A/B v2 launch + 8 agent analyses.
Console state after last run: game CRASHED (no clean exit) → kstuff paused; reboot
required before next launch. No further launches performed or needed — both open
problems are code problems.

## 1. What happened today (chronology)

1. **Crash run (13:47, original settings)**: l24 PRX activated, full init,
   type scan PASSED (`scan-complete missing-keys=3 core-ready=true` — the l24
   spanpub `PropertyImageClassRange` fix works). Log ends at
   "Adding shutdown listener". Game crashes CE-108255-1 when user loads a save.
2. **A/B v1 (18:53 console time) — INVALID**: settings edited true→false on two
   lines → file grew 1947→1949 bytes → modforge MFA admission rejected it
   (`mfa_activate_failed code=-8`, size-exact check) → module_start returned 0,
   game ran fully unmodded. "No crash" observation was meaningless.
3. **A/B v2 (14:07 local) — VALID**: settings rebuilt at exactly 1947 bytes
   (2 bytes trimmed from an INI comment), uploaded byte-exact. Runtime
   ACTIVATED (`runtime-status: state 1 activated`), type scan passed again,
   same init endpoint, and the game STILL crashed CE-108255-1 at save load.

## 2. A/B verdict: HookLoadMap + HookInitGameState EXONERATED

Crash reproduces identically with both hooks disabled (v2) and enabled (13:47).
Settings-level bisect is exhausted; the decision tree's "v2 crashes" branch
applies: open a new bisect in code, not settings.

## 3. Why the Lua mod never starts (agent G — code-traced, supersedes the
   pump-gating hypothesis)

Chain: `module_start` → `PrepareActivatedUE4SSRuntime` (`start_native_mods`
before `init()`) → `UE4SSProgram::init()` → `UnrealInitializer::Initialize`
(waits only for ≥10000 objects + CDOs) → `PostInitialize` prints
"Adding shutdown listener" (`UnrealInitializer.cpp:527`) →
**`UObjectArray::AddUObjectDeleteListener` (`:528`, `UObjectArray.cpp:379-389`)**
→ … → event-loop jthread (`UE4SSProgram.cpp:715`) → `update()` →
`on_program_start()` → `install_lua_mods` → "Lua mod started" (`LuaMod.cpp:7557`).

- Nothing in this chain waits on the game thread, GEngine, world, first tick,
  or any hook. `bHookLoadMap`/`bHookInitGameState` only gate address resolution
  and Lua callback delivery (`UnrealInitializer.cpp:2120-2128`, `:2255-2263`);
  neither is consulted by mod start.
- The log's last line is exactly `:527`; the ONLY statement between it and the
  next print (`:542`) is `AddUObjectDeleteListener` — the first write /
  GMalloc-virtual-call into live game state in startup. A fault or deadlock
  there leaves `init()` unreturned → event loop, `on_program_start`, and all
  `[l22]` output unreachable — in every arm.
- Precedent: l21 runtime-status failure "Virtual FMalloc::QuantizeSize is
  unavailable" (same class of live-GMalloc-virtual-call failure).

## 4. Save-load crash attribution (agent F — forensics)

- The l24 diag vocabulary is INIT-ONLY: from "Adding shutdown listener" to the
  crash there are ZERO events in every file; no
  LoadMap/InitGameState/ProcessEvent/R3-marshal/ExecuteInGameThread/pump tag
  exists. The crash window is unobservable in l24.
- Best-supported attribution: the resident modbridge copyin machinery (async
  copyin initializer + kernel-write probe left running after init, per the
  GATE-1 lineage) racing the game thread's save-load syscalls. Kernel
  "copyin: has nonsleeping lock" requires a spinlock-holding copyin, which
  stock game code does not produce. Unlogged LoadMap detour remains a
  non-excludable secondary (v2 kept HookLoadMap=false, yet the crash persisted,
  weakening detour theories).
- Combined with §3: both failures cluster in the runtime's write-into-game-
  memory machinery (first live-state write kills init; resident probe kills
  save-load).

## 4b. Attribution update (agents Q + V + vanilla verdict, evening)

- The vanilla klog verdict (§8f) proves the CE-108255-1 copyin signature
  requires our runtime. Within the runtime, the attribution has SHIFTED:
- WEAKENED: resident copyin probe racing save-load (§4's primary) — agent Q
  proved the sink bank is retired before init ends (fds closed; UE4SS.log:538
  prints after complete()), the probe worker is one-shot and exits at init,
  and NO post-admission caller exists across all 374 composed units.
  Observation-only liveness instrumentation added (overlay + WIRING.md) for
  both variants.
- STRENGTHENED: the listener family (§8d) — the confirmed missing vtable
  slot 4 (GetAllocatedSize OOB dispatch) and/or the unlocked TArray
  registration write racing the engine's locked dispatch. Z6 is tracing the
  engine-side reachability of GetAllocatedSize during save load/GC to rank
  the two; Z7 has rewritten the A/B interpretation matrix accordingly.
- NEW (agent Z1, klog baseline): 8 fatal blocks, 7 eboot pids, ALL with the
  plugin loaded, every crash following SaveGame mount/unmount. Both l24
  crashes (13:47 pid 208, 14:07 pid 266) are in the same captured klog with
  the identical signature. KEY FORENSIC: the salvaged abort backtraces run
  libkernel thread-entry → 15 frames inside
  modbridge_supplier_composition.prx → eboot frames → libc abort — the
  aborting thread is a PLUGIN-context worker that called into eboot code.
  Z11 is symbolizing those frames against the matching build artifacts to
  name the exact function chain.

`mfa_activate` (`.superpowers/native-dependency-spike/claude-takeover-43a96969/
source/runtime/modforge_activation.c:311-495`) enforces byte-exact sizes from
`loadlist.txt` and NOTHING else for role=config (no hash/magic/parse).
Settings edits must preserve the declared 1947 bytes. INI parser verified:
semicolon comments skip blindly; `false` accepted exactly; the shortened
comment parses identically. Other 6 loadlist files unchanged since the
admitted 13:47 session → still matching.

## 6. l25 tree status (agents M + earlier-batch worker)

Claude's items: 1 (Checked reads in real scan) DONE code; 2 (per-object catch)
DONE; 3 (RAII ScanCollectionScope) DONE; 4 (bootstrap exact-name-first +
PublishVerifiedPoolAddress) DONE; 5 (container offsets 0x70/0x78/0x70 +
UEP_TotalSize) DONE host-PASS; 6 (host tests) PARTIAL (suite still pinned to
l24 headers; only item 5 tested real l25 units); 7 (CHANGES.json) DONE.

Gaps before an l25 build: re-point+re-run item-6 suite at l25 headers; full
typescan-r4 host suite (actual_scan.cpp still extracts the l24 scan body);
`target_compile.py` unexecuted; `build_runtime.py` unexecuted (no
2026-10-08-l25-build-r2); packaging + import-identity + fake-sign/decrypt-OFF
discipline not started.

## 7. l25 scope — ALL LANDED (status as of evening 2026-10-08)

1. Runtime-phase diag tags — DONE (agent P, verified Z4 34/34): [l25]
   event-loop/on_program_start/marshal-first-invoke, r3_marshal_pumped
   (Claude's exact tag), EIGT queue depth; plus O's [L25][DeleteListener]
   family and Q's [Seeds][L25] copyin state lines (overlay-wired).
2. Init-stall fix/instrumentation at AddUObjectDeleteListener — DONE
   (agent O): sanity-gated refuse-clean registration, pre/post markers +
   header dumps, duplicate no longer throws, layout-throw caught, post-write
   anomaly check, mutex-serialized writers, GMalloc-gated growth, per-event
   RISK logging of the residual realloc race.
3. Copyin-probe gating — observation landed (agent Q): post-admission state
   line + throttled fire events + shutdown stop-proof; bank retirement
   proven; GATING prepared as a reviewed l26 proposal (Z15) for Claude's
   decision — deliberately NOT wired into l25 variants to keep the A/B clean.
4. ABI repair — DONE (agent O per V's ground truth): slot-4
   GetAllocatedSize added as last virtual; all three listeners repaired via
   base-header alias; create-listener verified already-correct; ODR-safe
   tree-wide.
5. Twin variants A/B — driver built (agent R, final build pending
   verifications): B = MODBRIDGE_L25_SKIP_DELETE_LISTENER, Q's patch rebased
   onto O's efdef253 base; receipts must show 4 recompiled units.
6. Items 1-7 original scope — DONE (earlier batch + M); item-6 suite
   re-pointed + 2/2 (S); final re-run gated on build phase.

## 8. R3 marshal pump mechanics (agent H) — consistent with §3's single root cause

- Pump = UFunction **func-pointer swap post-hook** on a "carrier" (BP overrides
  of Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation /
  UserWidget:Tick — `ScriptHookThunks.cpp:142,155`). Game-thread invocation
  fires `funcptr_marshal_hook` (`LuaMod.cpp:4789-4867`), publishes game-thread
  identity once (`UnrealInitializer.cpp:2493`), drains `m_game_thread_actions`.
- **Arming happens at `LuaMod::on_program_start` (`LuaMod.cpp:7933→:5138`) —
  which is unreachable while init() stalls at `:528` (§3).** No session ever
  logged "[R3] FuncPtr-marshal armed" → pump never armed → no
  ExecuteInGameThread drain → L3+R3 dead. One root cause explains every
  downstream symptom.
- A/B settings (HookLoadMap/HookInitGameState=false) do NOT touch the pump;
  only HookEngineTick=false would (pump_pending sits under bHookEngineTick,
  `UnrealInitializer.cpp:2118/2215-2219`).
- L3+R3 path: PS5Pad L3+R3→F7 poll on update thread (`PS5PadSource.cpp:154-171`)
  → keydown dispatch (`Handler.cpp:149-173`) → `ExecuteInGameThread` queues
  marshal-only (`LuaMod.cpp:5411/5541`) → needs the pump. Pad itself never
  opened in crash sessions (`pad_input_deferred` 0x80920004 only; borrow needs
  ≥10 s uptime, l22/l23 boots lived 3-5 s).
- No timeout/fallback: closures queue forever. Cheapest liveness diagnostic:
  one throttled `diagnostic_event("r3_marshal_pumped", ms)` in
  `funcptr_marshal_hook` after `LuaMod.cpp:4827`.

## 8b. Pending inputs (will append)

- Agent K (independent review of the l25 typescan-r4 changes)

## 8c. Independent l25 code review (agent K) — ISSUES FOUND, no blockers

All six claimed l25 changes implemented as described; 8/8 recomputed sha256
hashes match CHANGES.json. Clean: per-object catch absorbs only
[PropertyPointer] runtime_errors; ScanCollectionScope RAII correctly placed
(fixes a real l24 dangling-scan_log hazard); PublishVerifiedPoolAddress
sequenced publish→Install→scan, single-shot CAS, fail-open only while
unpublished; exact-match path allocation/pool-mutation-free; no UAF/TOCTOU.

Findings to fix before l25 build:
1. **MEDIUM**: noexcept Checked wrappers (`hpp:86-88`) call `layout_offset`
   (`.inl:29-34`) which throws on missing layout member → std::terminate at
   scan time; l25 removed l24's `missing_keys.attempt` wrapping
   (`TypeChecker.cpp:285-294`), regressing collect-and-continue to terminate.
   Fix: catch-and-record inside the Checked wrappers, or assert the exact keys
   are in the required preflight set at Install.
2. **LOW**: `fname_index_within_verified_pool` margin (`.inl:159`) budgets
   ANSI suffix only (2+8, not 2+16 for wide L"Property") — doctrine-consistent
   laxness, document it.
3. **LOW**: trailer parser records fabricated SlotRead/field=0/class=0 for
   unparseable [PropertyPointer] messages; `substr` arithmetic could throw
   from inside the catch on out-of-order markers (never guard-generated).

Fix agent dispatched to apply 1 (+ cheap 2/3 hardening) in the l25 tree.

## 8d. ABI ground truth CONFIRMED (agent V) — Claude's hypothesis verified

Engine = UE 5.6.1 (vendored Build.version + 93-fact image calibration). Our
port's FUObjectDeleteListener vtable is MISSING exactly one slot: engine slot 4
= `virtual SIZE_T GetAllocatedSize() const` (`UObjectArray.h:950`), engine
dispatches `vptr[4]` from `GetDeleteListenersAllocatedSize` (`UObjectArray.h:1471`,
reached from `LogHashMemoryOverheadStatistics`, vendored `UObjectHash.cpp:2318/2416`)
→ out-of-bounds read + uncontrolled call past our vtable. Slots 0-3 match.
Fix: add `virtual size_t GetAllocatedSize() const { return 0; }` as the LAST
virtual in the port base. Affected listeners: FClassDeleteListener +
FShutdownDeleteListener (`port .../ClassListener.hpp:30-44`),
FLuaObjectDeleteListener (`LuaUObject.hpp:32-42`). Authoritative sources:
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
(per l24-build-r2 SOURCE-SELECTION.json) — NOT the typescan-r4 tree.

Registration path additionally broken (item 4 input): engine member
`TArray<FUObjectDeleteListener*> UObjectDeleteListeners` (`UObjectArray.h:1432`)
+ `mutable FTransactionallySafeCriticalSection UObjectDeleteListenersCritical`
(THREADSAFE_UOBJECTS=1, `ObjectMacros.h:33`); engine's locked
`AddUObjectDeleteListener` (`UObjectArray.h:1133`) impl not vendored; our port
writes the live array DIRECTLY, unlocked, growing it with our own allocator
(`port/unreal/src/UObjectArray.cpp:379-392`) — lock-bypass race + allocator
mismatch on growth.

## 8e. Phase tags landed (agent P)

Staged pin-identical (l23-thread + r5 sources per thread/CHANGES.json pins)
into typescan-r4 then edited; all 15 CHANGES.json rows re-verified against
build_runtime.py assertions. New markers: `[l25] event loop started`
(UE4SSProgram.cpp:1605), `[l25] on_program_start entered` (:1458),
`[l25] marshal hook first invocation (carrier fired)` (LuaMod.cpp:4859),
`r3_marshal_pumped` throttled event (:4909, Claude's exact tag),
`[l22] ExecuteInGameThread queued depth={}` (:5653, six queues summed,
throttled). Existing `[R3] FuncPtr-marshal armed` (LuaMod.cpp:5089) and the
per-closure queued/drained lines deliberately NOT duplicated;
ScriptHookThunks.cpp staged diff-empty.

## 8f. Round-2 additions

- Review fixes (N) applied + independently verified (X): 85 checks, 0 failures,
  plain + ASan/UBSan, negative controls prove non-vacuity; enum append-only.
- Item-6 suite re-pointed to l25 headers and passing 2/2 (S); PRE_RUN_HASHES
  snapshot; re-run command documented for the build phase.
- Vanilla klog test: ploader.ini verified all-=0 (backup
  /tmp/ploader-ini-backup-20261008.ini, sha 3eab9510...); klog tap running
  (TCP 3232, /tmp/l25-vanilla/); crash baseline extraction from the l23 klog
  in progress (Z1); clean-session precedent = zero fatal-signal hits.
- Launch plan + Claude-review receipt + packet template (W); harvest tooling
  with marker catalog (Y); provenance audit (Z3); ploader ini read-timing
  audit (Z2) — all in flight.

- VANILLA KLOG VERDICT (Claude's cheap test — DECISIVE): vanilla save load
  (no injection, verified 3 ways: ini all-=0, runtime-status/UE4SS.log
  untouched, klog free of plugin markers; 154 libSceSaveData lines) produced
  ZERO "fatal signal", ZERO "copyin", ZERO "nonsleeping" hits. The CE-108255-1
  copyin signature requires OUR runtime. Evidence:
  evidence/vanilla-saveload-klog-20261008.log + evidence/VANILLA-VERDICT.md.

## 8g. Reachability ruling (agent Z6) — expectation-setting for the A/B launch

- Mechanism A (slot-4 OOB dispatch): RULED OUT for THIS game. Source: the
  vptr[4] dispatch lives only in GetDeleteListenersAllocatedSize
  (UObjectArray.h:1473), sole caller LogHashMemoryOverheadStatistics
  (UObjectHash.cpp:2416) — pure statistics logging, no save-load/GC/gameplay
  path. Binary: the whole diagnostics family is DEAD-STRIPPED from the
  shipping eboot (194,241,424 B, sha 3b6cfad5...; 0 hits for its string
  literals incl. the unconditional first log line; positive controls hit).
  The slot-4 fix remains correct cross-game hygiene but cannot change this
  game's behavior.
- Mechanism B (unlocked live-array write racing the engine's LOCKED
  NotifyUObjectDeleted dispatch): the leading in-family suspect. Save
  load/GC dispatch slot 2 at maximum intensity — ABI-safe but exactly the
  iteration our unlocked write races (engine locks every vendored access
  path; port writes unlocked). Fits Z1 (crash after SaveGame mount/unmount,
  plugin-worker abort) + G (log ends at registration).
- Confounder: neither A nor B directly yields the kernel copyin assertion;
  with Q's retirement proof, out-of-family attribution is NOT excluded —
  Z11's stack symbolization pending.
- Discriminating outcome: A-crashes + B-survives isolates the registration
  write family; B-also-crashes moves the bisect outside the family. If A
  stalls before the new [l25] tags (as every l24 arm did), the registration
  write is implicated pre-save-load. A's hardening (mutex/gates) may itself
  suffice — A-survives does not distinguish hardening-vs-no-race without B.

## 8h. Layout proof + usage map (agent Z8) — compiler-verified

PRE-FIX port base = 4 vtable entries vs engine 5 (clang -fdump-vtable-layouts,
member-pointer probes, sentinel probes, binary word counts). POST-FIX: slot
order + signatures identical across all five slots (GetAllocatedSize size_t()
const at slot 4; engine SIZE_T == size_t); all three listeners compile
unchanged, slots 0-3 unmoved, single-vptr 8-byte; engine-style raw slot
dispatch verified end-to-end. Usage map: 31 sites; port code NEVER dispatches
Notify itself (all engine-side); no casts/offsetof/vtable-enumeration/
slot-completeness reliance anywhere — blast radius: none. Create-listener
already matched (4 slots both sides); fix correctly scoped to delete-listener
only.

## 8i. Adversarial verification + crash REFRAME (agent Z10) — decisive

- Q's retirement claim SURVIVED all five attack vectors (thread-exact gate,
  TOCTOU-safe, no re-entry primitives, retained artifacts read-only, zero
  probe events in crash-session diag).
- DECISIVE: /tmp/l24-harvest/klog-20261008-133536.log holds SIX
  byte-identical crash dumps (pids 144/178/193/208/251/266): rip
  0x80004920 copyin abort, backtrace = 5 game frames OVER 16 modbridge
  frames from a modbridge-spawned thread; every surviving UE4SS log ends at
  "Adding shutdown listener" with :542 never emitted. The CE-108255-1 is a
  DETERMINISTIC INIT-COMPLETION CRASH in UnrealInitializer.cpp:528-541 —
  not save-load-timed, not the probe, not the LoadMap detour
  (settings-independent across all A/B armings).
- Residual-producer ranking: (1) AddUObjectDeleteListener -> game-TArray
  Append -> game allocator on the init thread (UObjectArray.cpp:379-386) —
  PRIME SUSPECT with kernel-backtrace proof; (2) runtime FName thunk
  (seed_refused [[noreturn]] on pathological UTF16) — real save-load
  hazard, wrong crash signature; (3) searcher-pool/StaticConstructObject
  callbacks; (4) R3 marshal/LoadMap detour — unsupported; (5)
  bridge_io_controls — theoretical.
- Convergence: Z10's ranking, Z6's mechanism-B lead, Z1's plugin-thread
  aborts, O's first-live-state-write stall, and G's :528 stall line all
  point at the same code region. Variant A (hardened Append) and B
  (skipped) test it directly.
- Bookkeeping flag: Q cited bridge sha 89e4b643 which matches no file
  (actual ba095f56 per Z10; Z15 cites 89e4b643 from STAGED-SOURCE-PINS) —
  R's hard pin discipline will surface any real mismatch loudly at build.
- Z11's frame symbolization pending — expected to name the exact chain.

## 8j. THE CRASH, SOLVED (agent Z11 — full symbolized stack, byte-verified)

All 8 klog blocks = one image (the l24 build) + byte-identical backtrace.
Symbolized chain (both l24 crashes identical; every edge verified against
shipped PRX bytes; layout calibrated on module_start/module_stop dynsym):

  abort <- eboot fatal-error routine (appErrorf family + abort@PLT)
  <- eboot virtual dispatch `call [rax+0x10]` (vtable slot +0x10)
  <- game GMalloc Realloc path
  <- RC::Unreal::FMalloc::Realloc+0x12b (mov NewSize/Alignment; call rax)
  <- RC::Unreal::FMemory::Realloc+0xb0
  <- TSizedHeapAllocator<32,FMemory>::ResizeAllocation+0x90
  <- TArray<FUObjectDeleteListener*>::ResizeTo+0xc2
  <- TArray<FUObjectDeleteListener*>::Append+0xe4
  <- RC::Unreal::FUObjectArray::AddUObjectDeleteListener+0xcf
  <- RC::Unreal::UnrealInitializer::PostInitialize+0x3fa  (= the :528 line)
  <- Initialize+0x3c53 <- setup_unreal+0x70e <- UE4SSProgram::init+0x20d
  <- UE4SSBackend::initialize+0x47 <- RuntimeOwner::initialize+0x174
  <- std::thread entry (unnamed, tid 101547/102053 — the modbridge init
     thread; NOT game thread, pad poll, or copyin worker)

MECHANISM: the crash is INSIDE INIT — the first statement after the log's
last line. AddUObjectDeleteListener grows the listener TArray through
FMemory::Realloc -> FMalloc::Realloc -> `call rax` = the FIRST live call
into the game's (repointed) GMalloc. Inside the game allocator, a virtual
dispatch at slot +0x10 landed on the game's appErrorf-style fatal formatter
-> abort() -> CE-108255-1. SaveGame teardown completing seconds before each
crash = the race window context. NO copyin/probe frame exists anywhere in
the stack — the kernel's "copyin: has nonsleeping lock" line is a dump
artifact, not the mechanism. Agent G's :528 stall prediction confirmed
exactly; Z10's deterministic-init-crash reframe confirmed.

SHARPENED SUSPECT within the confirmed chain: the GMalloc repoint
("Post-initialization: GMalloc: 0x970add8 -> 0x97089b8") and/or our FMalloc
vtable layout — the in-allocator +0x10 dispatch landing on the fatal
formatter suggests the allocator object we dispatch through is not what we
assume (stale/wrong GMalloc pointer, or vtable slot mismatch of the same
family as the listener slot-4 bug).

l25 IMPLICATIONS: variant B (registration skipped) performs NO Append ->
no GMalloc call -> predicted to survive AND potentially reach mod start.
Variant A still grows via the game allocator when the array needs growth ->
may STILL crash with the same chain (hardening does not fix the dispatch).
Z11's recommended bisect direction: defer/re-route the registration out of
the init/save-load window (post-init or game-thread via the marshal) —
candidate l26 variant C for Claude's decision alongside Z15's gating
proposal.

## 9. Console state / safety

- UE4SS-settings.ini on console = A/B v2 variant (1947 B, both hooks false).
  Harmless either way; restore original before any future canonical run if
  desired (both are admission-legal).
- Game crashed → kstuff paused → REBOOT REQUIRED before any next launch.
- No executable uploads today; no fake-sign/decrypt-OFF events needed.
- Contaminated-then-restored l24 tree: guard .inl/.hpp + CHANGES.json restored
  byte-exact from review-zip diffs; contaminated copies quarantined at
  /tmp/l24-restore/ (provenance note only).

## Agent reports (full detail)

Round 1 (analysis): F crash-forensics, G mod-start-gating, H r3-marshal,
I mfa-audit, J log-digest, K l25-review, L experiment-matrix, M l25-inventory
Round 2 (implementation+verification): N review-fixes (X: 85/85 verification),
P phase-tags (Z4: 34/34 verification), S item6-repoint (2/2 PASS), T klog-capture,
V listener-abi, W launch-plan+receipt, Y harvest-tool, Q copyin-probe,
Z2 ploader-timing, Z3 provenance-audit (24/24), Z5 packet-assembler
In flight: O listener-instrumentation (critical path), R twin-build,
Z1 crash-klog-baseline, Z6 getallocatedsize-reachability, Z7 matrix-update,
Z8 listener-layout-proof, Z9 edit-path, Z10 copyin-adversarial-verification
Ops artifacts: LAUNCH-PLAN.md, INTEGRATION-CHECKLIST.md, harvest/, packet/,
evidence/ (vanilla klog + verdict), settings arms + experiment-matrix.md
- experiment-matrix.md — A/B arms (moot) + decision tree (consumed)

## 8k. ROOT CAUSE CONFIRMED AT THE OFFSET LEVEL (agent AA, engine-binary verified)

Claude review-2 item 1 suspicion PROVEN. Engine functions located in the
authenticated image (string-confirmed): AddUObjectDeleteListener 0x4841A0,
RemoveUObjectDeleteListener 0x484240, NotifyUObjectDeleted dispatch loop
0x483FF0, FUObjectArray::Shutdown 0x4843A0. GUObjectArray image vaddr
0x93BA110 (runtime 0x97BA110). Offsets read directly from their code:
- UObjectDeleteListeners: profile 0x78 WRONG -> real 0x68 (Data 0x68 /
  Num 0x70 / Max 0x74). At 0x78 sits the UObjectDeleteListenersCritical
  FAST-STATE; our Append read the lock state as "Data" and the lazy-mutex
  pointer halves (0x80/0x84) as Num/Max -> non-heap pointer to GMalloc
  Realloc -> allocator fatal -> CE-108255-1. Any write-back corrupts the
  crit the engine locks on EVERY object destruction (the save-load
  detonator).
- UObjectCreateListeners: profile 0x68 WRONG -> real 0x58 (0x68 is the
  delete array — the two shifted into each other's slots). Latent (port
  registers only delete listeners; config-off via bUseUObjectArrayCache).
- UObjectDeleteListenersCritical = 0x78, a 0x18-byte lazy-mutex
  FTransactionallySafeCriticalSection (fast state +0x78, mutex ptr +0x80,
  recursion +0x88). Engine Add/Remove/dispatch/Shutdown all take it.
- ROOT of the error: profile assumed 0x28-byte critical section; shipping
  Prospero build uses the 0x18-byte form -> every post-ObjObjects member
  shifted by 0x10/0x20. Bonus corrections (verify consumers before fixing):
  ObjObjectsCritical=0x30, ObjAvailableList=0x48, PrimarySerialNumber=0x90,
  sizeof(FUObjectArray)=0x98; profile's 0xB0/0xB4/0xB8 point into adjacent
  BSS globals.
IMPLICATION: with corrected offsets, variant A's registration reads the
REAL array and the crash mechanism is eliminated at the source. Variant C
(engine's own locked Add at 0x4841A0) now has its address proven. Profile
fix flows through AM's template once AL's consumer map is in.

## 8l. SECOND root-cause-grade defect (agent AB) — FMalloc vtable map off by one slot

- (a) Variable-vs-cached: CALL-TIME read confirmed (FMalloc** aimed once at
  the game variable FMemory.cpp:8 / UnrealInitializer.cpp:848; instance +
  vptr re-read per call; only the SLOT OFFSET is static-cached from the
  compiled-in 5_06 map).
- (b) The logged 0x97089b8 instance is the game's static BOOTSTRAP allocator
  (vtable GetDescriptiveName -> L"Unspecified allocator"; scrambled-NID
  import bindings); FMallocBinned3 is linked but installed only at runtime.
  Runtime capture: match vptr against 0x82c2e28 (bootstrap) / 0x82c26e0
  (Binned3).
- (c) MAJOR: the game's FMalloc vtable is exactly one slot (+8) ABOVE the
  port's 5_06 map — true Malloc@+0x30, Realloc@+0x40,
  GetDescriptiveName@+0xd0 (proven by wrapper forward-targets, signatures,
  slot semantics). The port's "Realloc"@0x38 dispatches the game's TryMalloc
  wrapper forwarding Malloc(Count=(SIZE_T)Original, Alignment=(uint32)NewSize)
  — arguments scrambled (old pointer as size, new size as alignment) ->
  deterministic huge-size fatal -> the :528 crash family. This SUBSUMES the
  race framing (TOCTOU remains real but secondary) and converges with AA's
  member-offset defect at the same crash site: EITHER defect alone fatals
  the Append. BOTH fixes required.
- Item-4 logging: AB's drop-in spec (var/instance/vptr + raw slot reads
  +0x28/+0x30/+0x38/+0x40/+0xd0) supersedes AD's PENDING-ALIGNMENT comment.

## 8m. Cross-agent reconciliation (root) — operative cause vs latent defect

AH's binary proof settles which defect fired in l24:
- CONFIRMED (Claude's interpretation): the eboot 'call [rax+0x10]' is
  FOutputDevice::Serialize on GError inside LowLevelFatalError (fatal
  logging before abort); the allocator rejected the call literally as
  FMallocBinned3 'Attempt to realloc an unrecognized pointer %p'
  (MallocBinned3.cpp:880, hash-walk miss at 0xb2160).
- The crash call dispatched [vptr+0x40] -> 0xb49b0 = the TRUE Realloc —
  AH triple-pins that the runtime used the STAGED native layout
  (Realloc=0x40), not the compiled-in per-version default (0x38).
- THEREFORE: the OPERATIVE l24 root cause is AA's member-offset defect
  (crit fast-state read as Data -> non-heap 'old pointer' -> the exact
  'unrecognized pointer' rejection AH proves). AB's vtable-map +8 defect
  (compiled-in 5_06 default: Malloc@0x28/Realloc@0x38 vs true +0x30/+0x40,
  GetDescriptiveName@+0xd0) is REAL but LATENT in l24 — the staged layout
  overrode it. Both get fixed (defense in depth): AA's via the profile
  correction (AS), AB's via correcting the compiled-in default map so a
  missing staged layout can never mis-dispatch to TryMalloc with scrambled
  args.
- Runtime discriminators for the A launch (item-4 logging): GMalloc variable
  value expect 0x97089b8; instance vptr expect 0xBEC26E8 (Binned3 runtime)
  vs 0x82c2e28 (bootstrap); Data pointer vs allocator arenas.

## 8n. GMalloc reconciliation (agent AB2) — Claude review-3 open question ANSWERED

- Our seed binding 0x970add8 (file-image 0x930add8, PH7 BSS) IS the primary
  platform allocator variable: written by exactly ONE store image-wide
  (0xb660 in the Prospero memory-init factory 0xb3f0) with the static
  FMallocBinned3 instance (0x93089b8; vptr ap 0x7ec26e0 — Binned3 from
  birth; NO repoint exists statically, in immediates, or in the 1.66M-entry
  RELA table; AB's earlier bootstrap/repoint reading corrected).
- The engine FMemory::Realloc (0xBEF20) loads a SECOND variable at
  0x9350950 file-image / 0x9750950 runtime (AE's "0x9340950" was a
  transcription ghost — corrected), whose lazy-init (0xbeba0) copies
  *(0x930add8) into it via the accessor 0xc080. Steady state: both
  variables hold the IDENTICAL Binned3 singleton; the ThreadSafeProxy
  branch is dead (Binned3 IsInternallyThreadSafe = TRUE).
- CONSEQUENCE: engine-side TArray growth and our binding's Realloc/Free
  reach the same allocator instance — cross-allocator block rejection is
  structurally impossible. VARIANT A: GO AS-IS (no re-bind; seed slot 3
  derivation re-verified). Claude's condition ("if 0x970add8 is the
  engine's GMalloc or a proven alias, A becomes the second test") is MET.
- Errata recorded: AE §3 (0x9340950 -> 0x9350950), AB §0.2/§3/§6
  (bootstrap/repoint/race don't exist), AH §1.1 (ap 0x7ec26e0 not ...e8).

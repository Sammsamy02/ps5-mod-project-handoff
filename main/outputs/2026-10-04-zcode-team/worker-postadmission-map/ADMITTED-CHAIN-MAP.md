# ADMITTED-CHAIN-MAP — what happens AFTER the FName pool is admitted (worker-postadmission-map, 2026-10-05)

Read-only trace + prediction map. LOCAL ONLY: no console contact, no edits outside this
directory, no composition, no engine/provider execution, no game claims. Every "PREDICTED"
below is an evidence-shape prediction for the successor's first admitting boot, not a
captured result; nothing here closes a rung.

## 0. Basis (re-derived today; all prior line numbers superseded)

| alias | file | sha256 (first 8) | lines |
|---|---|---|---|
| UI | `outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp` | `1660b749` (verified: shasum = 1660b749c86e4944…) | 2418 |
| LM | `outputs/2026-10-04-zcode-team/worker-next-candidate/staging/luamod-next-candidate-merged/LuaMod.cpp` | `facf993a` (verified) | 8555 |
| PROG | `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` | `cb6cb14a` | ~3100 |
| ASM | `outputs/2026-10-04-zcode-team/worker-walk-site-transitions/ASMHelper.guarded.cpp` | `694af016` (fold-0 form; composed object pin `68d22a1c`, worker-launch11-dry-compose REPORT:21) | — |
| BACKEND | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp`, `runtime_owner.cpp` | — | — |

Successor manifest: `worker-next-candidate/next-candidate.json` ("next-candidate-admission")
— luamod row = `facf993a…` (r3-ladder `6f97f8a1` + gate5-loadmap `9e4616d1` merged on base
`c92c92ff`); copyin trio (bridge + SeedValidation.hpp + initializer) admitted as ONE atom.
CAVEAT carried by that manifest itself: the copyin initializer diff is derived on `e7f349af`;
the live units initializer is now `1660b749` — the diff must be re-derived before admission.
In the CURRENT `1660b749` unit the listing-gap call still resolves as
`RangeMappedVerdict::Unavailable / ReadableCoverageSource::None` (UI:1025-1028; comment
UI:691-697), i.e. a listing gap refuses; the copyin probe (`bridge_kernel_write_copyin_probe`,
once wired) fires ONCE per boot on the first listing-gap miss and can convert
mapped-but-unlisted into admission.

Walkguard fold in the successor: fold-0 (`RC_ASM_WALK_RX_GUARD 0`, ASM:249/57-59) — the
12-edit manifest does not flip it; the flip is the measured M-A step (walkguard-gate REPORT
§3 step 2) and needs the T2 verdict first. All predictions below are written for BOTH worlds
where they diverge; the first admitting boot is a fold-0 boot unless the controller lands the
verdict+flip in the same admission.

R3 marshal mode: armed — in-file default `UE4SS_R3_MARSHAL_MODE 1` (UI:178-180); every rev-B
site below is preprocessor-guarded and the disarmed fold is object-identical, so the armed
behavior IS the current object's behavior.

Settings facts used (staged-root lineage, console-side facts to re-verify at staging):
`SecondsToScanBeforeGivingUp=120` (pool wait budget), `bUseUObjectArrayCache=false`,
`[EngineVersionOverride] MajorVersion=5/MinorVersion=6` (worker-engineversion-ini
package-v2, 834 B, `9570ead3…`), and — CONDITIONAL — `[Hooks] HookProcessInternal=0` +
`[Hooks] HookProcessLocalScriptFunction=0` (worker-settings-v3-package, +614 B). Whether the
v3 keys are in the staged root for the successor boot is a staging fact; step 15 below is
conditional on it.

---

## 1. The ordered chain, pool admission → banner

Emission sources: UE4SS.log lines via `Output::send`; runtime-status via the ack sink
(`activated` / `engine-ready` / `mods-started` / `failed <code> <detail>` / `stopped`,
runtime_owner.cpp:171-177); sdiag = bridge `diagnostic_event(tag, 1u)` tags. NO step in this
chain emits a NEW sdiag tag — the only sdiag near the boot are the bridge's module_start
spanpub set (`bridge_spanpub_count/env_low/env_high/total`, admission INPUT) and, if the
copyin bridge is admitted, the copyin probe tags. Walkguard refusals are log lines + the
runtime-status failed line only.

Pre-admission context (same consume call; if any of these refuses, admission never reaches
the pool): base check UI:845-850 → slot 0 GUA classify+probe+bind UI:852-867 → slot 3 GMalloc
classify+weak probe+V1+V2 UI:869-918. Evidence: post-seeds CHAIN §1 (order), GMalloc-CANDIDATES
rank-1 arming rule (worker-gmalloc-derivation, cited UI:893-902).

| # | step | file:line (current) | emits (UE4SS.log unless noted) | PREDICTED outcome on first admitting boot | evidence for the prediction |
|---|---|---|---|---|---|
| 1 | Pool shape+interned-id wait loop breaks | UI:989-1046 (break at 1034-1037; 50 ms retry 1045; budget 120 s refusal 1038-1044) | none on success (silent) | PASSES — Blocks[0] was observed in LOAD7's BSS tail inside the image extent (launch-9 region-class), i.e. inside a published readable span, so `check_pool_shape` membership passes without needing the copyin probe | worker-fname-validator-v4 span admission; FNAME-POOL-RE R2 storage separation (UI:661-672 comment); SeedValidation.hpp:347 truncation class |
| 1b | (copyin-wired successor only) listing-gap probe, once per boot | copyin trio diff (staged on e7f349af — RE-DERIVE onto 1660b749); probe = `bridge_kernel_write_copyin_probe` | sdiag `copyin_probe_*` / `copyin_sink_*` | UNKNOWN until wired+run; contract says mapped-but-unreadable vs truncated must refuse honestly | worker-copyin-unit ADMISSION-NOTE §2-3; worker-write-copyin design note |
| 2 | Blocks[0] entry-0 'None' content anchor | UI:1056-1062 | none on success | PASSES — same class as the entry reads; wrong-base protection is exactly this anchor | FNAME-POOL-RE R3 (UI:1048-1055 comment) |
| 3 | 'None' / 'ByteProperty' entry ASCII validation | UI:1064-1080 | none on success | PASSES | post-seeds CHAIN §1 phase 3; worker-fname-ctor-audit FNAME-VERDICT §5 |
| 4 | Pool validated log — THE ADMISSION LINE | UI:1081-1082 | `[Seeds] FName pool validated at 0x9765EC0 (CurrentBlock N, Blocks[0] 0x… inside a published readable span (image or non-image) of K; 'None' anchor ok; 'None' id 0x…, 'ByteProperty' id 0x…)` | PASSES (admission complete) | post-seeds CHAIN §1 (same line, older sha) |
| 5 | Slot 2 ctor bind + behavioral ByteProperty probe — FIRST live code entry of the boot | UI:1084-1107: classify CodeCallOnly 1092-1095; `assign_address` 1096; `FName ProbeName{"ByteProperty", FNAME_Find}` 1097 (CALL through the seeded ctor, X-fetch only); id check 1099-1104; latch 1105 | `[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified (ByteProperty probe id == pool id 0x…); detour verification skipped` 1106-1107 | PASSES expected; mismatch path = REFUSES-LOUDLY (`[Seeds] REFUTED: slot 2 FName ctor behavioral probe …`, terminal, UI:1100-1104); residual risk: a wrong-but-callable label would CRASH, but the label is 3-way cross-checked | worker-fname-runtime-probe REPORT (9 call sites, `E8 rel32`→0x23d590, fixtures re-verified; probe module NOT natively executed — derivation-grade only); FNAME-VERDICT §3/§5; SIGNATURES-INVENTORY slot 2 row |
| 6 | FLAG-1 note: the detour verifier is never armed on this path | skip at UI:1709-1713 (`bSeedBoundFNameCtor`); R3 compile-out UI:1584-1599 | none when seed-bound (the `[R3] refused: FName constructor verification detour …` line only exists in the unseeded R3 path) | skipped — by design; detour = code READ+WRITE on exec-only text (FLAG-1) | SIGNATURES-INVENTORY C1 ("dead-needs-remedy — FLAG-1"); UI:1701-1708 comment |
| 7 | Slot 4 SCO bind (constant only, nothing executes) | UI:1109-1124: classify 1118-1121; bind 1122 | `[Seeds] slot 4 static_construct_object_internal bound at 0x890aa0 (code; identity and ABI unverified)` 1123-1124 | PASSES (bind only) | SIGNATURES-INVENTORY slot 4/E2 (bound-by-seed; identity deliberately unverified because text reads are refused) |
| 8 | Scan complete; Lua override scans closed | UI:1559 (`bScanFullyCompleted=true`); armed log earlier at UI:1196-1197; second-pass gate 1541-1556 skipped | `[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm and both Lua override scans are skipped` + `[Seeds] Unbound by design: …` (both BEFORE admission, UI:1196-1197) | already emitted; nothing further | SLOTS-FEASIBILITY §0 G0-G4 (ps_scan structurally dead) |
| 9 | Engine version line | UI:1721 | `Using engine version: 5.6` | PASSES **iff** the staged root carries `[EngineVersionOverride] MajorVersion=5/MinorVersion=6`; otherwise Version=0.0 and the layout publish (step 11's `InitializeVersionedContainer` → `PublishStagedNativeLayoutMaps`) THROWS — a loud early stop BEFORE any walk | worker-engineversion-ini NEW-SETTINGS §1 (parser admission semantics; expected line `Using engine version: 5.6`; override must equal the staged 5.6 layout identity or the binder's identity-agreement refusal fires) |
| 10 | Object-construction wait (data-side NumElements ≥ 10000) | UI:1728-1743; owner-stop exit via `CheckStartupWait` UI:76-85 (wired to `m_shutdown_requested`, PROG:1020-1028) | `Waiting for object construction...` 1728; every ~400 attempts: `Still waiting for object construction, N objects constructed so far` (Warning) 1740 | PASSES expected (data-side read through bound GUObjectArray — the proven class); live construction cadence is unproven; only alternative exits are owner stop or the 5 ms-loop stall (observable via the repeating warning) | pump-chain CHAIN M1 "data-side reads OK"; igf4 data-read class (walkguard REPORT §1 READ-A) |
| 11 | Kismet chain (unbounded data-side loops) | locate 1747-1763; Conv_NameToString 1765-1782; CDO 1783-1796 | `Locating KismetStringLibrary...` → `KismetStringLibrary located: 0x…` → `…Conv_NameToString located…` → `…CDO located…`; every ~40 attempts a named Still-searching Warning 1757/1777/1792 | PASSES expected — fname_tostring is deliberately unbound; the Kismet fallback OWNS it (UI:1197), so this chain is load-bearing and expected to resolve in a shipping UE5.6 title; a stall here is an honest loud waiting state, not a crash | post-seeds CHAIN S2; SINI:1093-1110 fallback ownership (cited UI:1190-1192) |
| 12 | Required objects (list + ~8 min capped wait) | Add list UI:1799-1846 (5.6: no DynamicClass — `IsBelow(5,4)` false, 1842-1846); wait 1848-1913; FATAL check 2186-2196 | `Constructed [n / m]: <name>` per find 1877; every 30 s `Still waiting for required object '<name>' after N seconds` 1890; on timeout `Timed out waiting for required object '<name>' to construct; continuing without it` 1908; fatal path logs `Fatal error! The following objects were never constructed:` + names then `throw std::runtime_error{""}` 2189-2195 | PASSES expected; the timeout-continue path is real — if any required object never constructs in this title, the fatal throw at 2195 carries an EMPTY message (runtime-status `failed 1 ` with empty detail) — that shape is diagnosable from the preceding named lines | pump-chain CHAIN S3; UI:1881-1883 comment (named-wait rationale) |
| 13 | Engine CDOs / singletons (data-side finds) | Object 1926-1931, Struct 1933-1938, GameEngine 1940-1945, GameMode 1949-1958, Actor 1960-1965, GameViewportClient 1967-1973 | `Object CDO: 0x…`, `Struct CDO: 0x…`, `Finding 'GameEngine'`… `GameEngine CDO/Instance: 0x…`, GameMode/Actor/Viewport lines; `Could not find 'X', falling back to 'Y'` 1920 on miss | PASSES expected; named throws at 1929/1936/1943/1955/1963 are the loud refusal shapes; GameViewportClient miss is Warning-only and disables that one hook (1970-1971) | pump-chain CHAIN S4 |
| 14 | **LoadMap resolve — FIRST exec-text walk (GATE-1)** | UI:1975-1983; macro → `ASM::resolve_function_address_from_potential_jmp` → `ScopedRxGuard` ASM:286 (first ≤16-byte read of exec-only text) | last progress line BEFORE the gate: `Finding GameEngine::LoadMap` 1977. **Fold-0:** `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict (LAUNCH10-MATRIX D2). Fold RC_ASM_WALK_RX_GUARD=1 only for W_WORKS/W_FAILS_R_WORKS rows; reason in thrown message.` (Error, ASM:234) + throw `[WalkRxGuard] refused: guard_off_no_rx_verdict (walk text read not permitted; no read attempted; detail=0)` (ASM:235-237). **Fold-1:** `GameEngine::LoadMap address 0x…` 1980 + assign 1981 | **Fold-0: REFUSES-LOUDLY — THE PREDICTED FIRST STOP of the first admitting boot.** Fold-1: PASSES with a residual per-page `rx_transition_denied` loud refusal possible deeper in text (fail-closed, zero-read, ASM:156-161) | walkguard-gate REPORT §1 (full trace: READ-A/READ-B data OK, READ-C is the gate; igf6 crash class is what the guard prevents), §3 step 3 (pass path), W1/W2 shapes |
| 15 | Tick resolve + R3 deferral (only in a walk-permitted boot) | vtable walk 1987 (guarded); scan null under seeds → vtable fallback 2044-2053; R3 block 2069-2085 | `Finding GameEngine::Tick` 1986; `GameEngine::Tick address (vtable: 0x…; scan: 0)` 1990 (16-entry display walk UNREACHABLE — gated on non-null scan_address 1992-2022, documented UI:2029-2043); `Scan failed, falling back to vtable address for GameEngine::Tick` 2051; then `[R3] refused: EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch) (detour registration compiled out under R3 marshal mode)` 2082 (Warning), `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` 2083, `pump_pending=true` 2084 | PASSES (assignment) + R3 lines emitted as designed; NO detour is armed (GATE-2 stays closed; the R3 SetFuncPtr marshal is the designed bypass) | r3-ladder REPORT §1 (availability chain); UI:2069-2085 comments; pump-chain CHAIN M3 R3 world |
| 16 | Remaining virtual resolves (fold-1 world) | InitGameState 2110-2118; BeginPlay 2119-2127; EndPlay 2128-2136; AActor::Tick 2137-2145; ViewportTick 2146-2154 (skipped if step 13 disabled it); ProcessEvent 2155-2163; ProcessConsoleExec 2164-2172 (GET_ variant: THROWS if the layout name is absent from VTableLayoutMap); UStruct::Link 2173-2181 | one `Finding X` + `X address 0x…` pair per site | PASSES expected per site under fold-1; each site is an independent guarded walk (a deep-page `rx_transition_denied` refusal is loud and fail-closed); ProcessConsoleExec is the one layout-dependent throw risk (UNKNOWN until the staged VTableLayout coverage for that name is confirmed on this boot) | walkguard REPORT §1 item 8 (same-gate sites); VCM:124-139 via walkguard REPORT |
| 17 | FName globals + type system | 2183-2184; 2198-2202 | `Initializing global FName variables` 2183; `Constructed N of M objects` 2186; `Initializing type system` 2198 | PASSES expected; TypeChecker throw at 2201 is the loud refusal | pump-chain CHAIN S6 |
| 18 | ProcessInternal derivation — **conditional FLAG-3 crash class** | gate UI:2217 (`bHookProcessInternal || bHookProcessLocalScriptFunction`); guarded resolve 2226; UNGUARDED Zydis 164-byte decode loop 2247-2275 | if enabled: `Finding ProcessInternal` 2219, `ProcessInternal address: 0x…` 2232, `Finding ProcessLocalScriptFunction` 2238, `ProcessLocalScriptFunction address: 0x…` 2269 | **If staged settings carry `[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0` (settings-v3 bytes): SKIPPED — no lines, no reads (PASSES by absence). If NOT staged: CRASHES — the Zydis loop reads live exec text with no guard (naked read; no refusal line is possible; log just stops mid-boot; runtime-status gets no graceful failed line)** | SIGNATURES-INVENTORY C10 / FLAG-3 (UI:2204-2216 comment); settings-v3-package REPORT:26-27 (the +614 B keys); walkguard REPORT §1 item 8 ("FLAG-3 class, skipped only under the settings-v3 keys") |
| 19 | PostInitialize | call UI:2293; body 440-565 | `Post-initialization: GMalloc: 0x… -> 0x…` 447; (bUseUObjectArrayCache=false → NO `Initializing searcher pools` line, 460-504 skipped); `Adding shutdown listener` 506; `Starting callback garbage collector!` 521; `Hooking StaticConstructObject for searcher pools` 525; size lines `FAssetData/FVector/FQuat/FFieldVariant/FText/FName Size: 0x…` 555-564 | PASSES with EXPECTED NOISE: the SCO post-callback arm (UI:526-553) is the first live PLH detour attempt of the boot; per the composed owned-memory law it soft-refuses at its first step — one PLH SEV log + one AddCallback Error + ERROR_ID that the void overload ignores (Hooks.cpp:66-72) — crash-free, throw-free, futile; the hook feeds nothing (moot: searcher pools disabled) | gate5 comment in facf993a:4570-4600 (owned-memory registry law: zero deref, zero syscalls, hook() false at empty-insts); SIGNATURES-INVENTORY C-row machinery; staged `bUseUObjectArrayCache=false` (worker-config-final-review §2) |
| 20 | Program ready + acks | PROG:684-697 (`fire_unreal_init_for_cpp_mods` 684, `setup_unreal_properties` 685, `share_lua_functions` 688, `m_is_program_started=true` 691, `on_engine_initialized` 693-697) → BACKEND:38-46 | runtime-status: `engine-ready` then `mods-started` (mods-started only when ctor `start_native_mods` already verified mods — activated flow: yes; BACKEND:44-45) | PASSES expected — first runtime-status success lines of the boot; emitted BEFORE the event loop and BEFORE the banner | BACKEND:24-46 (ack ordering comment); post-seeds CHAIN S7 |
| 21 | Event loop + on_program_start | PROG:709 (`m_event_loop` jthread), update 1583-1586 → `on_program_start()` 1586 → LM:7657 | (no direct line) | PASSES expected | pump-chain CHAIN M2 |
| 22 | R3 marshal arm + LoadMap defer (merged-luamod behavior) | LM:7659 listener; 7665 `r3_marshal_calibration_consumer` (def LM:4935) → carrier arm LM:4884-4908; LoadMap gate LM:~7700-7717 | `LoadMap` defer Warning (the long `[R3] LoadMap detour registrations deferred: …` line, facf993a ~7717); on first carrier fire: `[R3] FuncPtr-marshal armed on carrier <UFunction full name> (callback id …)` LM:4908; NOT armed path: `[R3] marshal not armed: initializer owns the game-thread pump (pump_pending=false)` LM:4943 | In the R3 world: the big deferral Warning fires; the marshal arms on the FIRST matching carrier hook fire (Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation) — engine-driven, needs live ticks; carrier fire rate (F-1) is an OPEN question — if no carrier ever fires, the pump never drains (mods load but game-thread calls stall) | r3-ladder REPORT §1 (arm/drain sites); pump-chain CHAIN M3/S9; gate5-loadmap receipt rows 3+ (deferred arm semantics, host 10/10) |
| 23 | install/start mods | PROG:1557-1560 (`install_lua_mods` 1935; `LuaMod::on_program_start` already run; `start_lua_mods` 2218); `Setting up mods...` happened earlier at PROG:1828 (ctor) | `Starting mods (from mods.txt (<path>) load order)...` PROG:2101; `Mod 'TownfallItemSpawner' has enabled.txt, starting mod.` PROG:2210; not-installable skip warning otherwise | PASSES expected (loadlist/mods.txt staging is console-side; a lookup miss degrades to the known `Mod is absent from native Lua source` + not-installable shape, post-seeds CHAIN §0) | post-seeds CHAIN §0/S8; pump-chain CHAIN M2 |
| 24 | THE BANNER | LM:7203 `start_mod` → 7226 `load_and_execute_script(main.lua)` → lua_pcall → main.lua top-level `log` (main.lua:746 pin) | `[Lua] [TownfallItemSpawner] Loaded v1.0 …` | PASSES expected in a walk-permitted, correctly-staged boot; failure shapes are distinct and loud: `Failed to execute main script: …` LM:7228 / chunk error 6916-class / pcall error | post-seeds CHAIN §4-§5 (read path, state creation, first output); pump-chain CHAIN M2 |

Chain-end state to expect on a FULLY successful admitting boot: runtime-status ends
`mods-started` (state running observable), UE4SS.log ends near the banner + pad-defer silence
(wpd1: attached-but-unavailable proceeds; the wpd2/coexist `pad_open_class`/`pad_retry`/
`pad_retry_failed`/`pad_available` sdiag tags are NOT in this successor — the manifest
carries program-wpd1 only), and the marshal arm line may lag until the first carrier tick.

---

## 2. FIRST EXPECTED STOPPING POINT

**Step 14 — the LoadMap walk (UI:1978) under the unchanged fold-0 walkguard.** Everything
between admission and it is data-side or constant-bind work with derivation-grade evidence;
the LoadMap resolve is the first exec-text READ of the boot and the guard's fold input is
still 0 (no T2 verdict exists, so the measured flip has not landed). Steps 1-13 + 15-24 are
what a fold-1 (post-M-A) boot would traverse.

Exact refusal shapes of the predicted stop, per sink:
- UE4SS.log: last progress line `Finding GameEngine::LoadMap` (UI:1977), then
  `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict
  (LAUNCH10-MATRIX D2). Fold RC_ASM_WALK_RX_GUARD=1 only for W_WORKS/W_FAILS_R_WORKS rows;
  reason in thrown message.` (Error, ASM:234). Nothing after it from the initializer.
- runtime-status: `… failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict (walk text read
  not permitted; no read attempted; detail=0)` (throw text via copy_error_into_message →
  `ack->failed(1, …)`, runtime_owner.cpp:111-118).
- sdiag: NO new tag (the guard emits none); the boot-end teardown follows:
  `shutdown_native_program` → per-mod `Stopping mod '<name>' for uninstall` lines
  (post-seeds CHAIN §2), runtime-status `failed` then eventually `stopped`.
- NOT seen: any `[Seeds]` refusal, any `GameEngine::LoadMap address` line, `engine-ready`.

"This is expected, not a regression" readings, one line per candidate stop:
- `failed 1 [Seeds] REFUTED: … pool …` → the admission gate refused with fresh evidence;
  the chain under test never started — reconcile the refusal class, never replay it.
- `failed 1 [Seeds] REFUTED: … span table unavailable (bridge published 0 readable spans…)`
  → the bridge's span walk found nothing qualifying (check sdiag `bridge_spanpub_count`);
  admission refused honestly; in the copyin-wired successor this is exactly the class the
  probe exists to convert — its absence/verdict is the finding.
- `failed 1 [Seeds] REFUTED: slot 2 FName ctor behavioral probe …` → the seeded ctor id
  disagreed with the pool; terminal by law — the derivation, not the boot, needs new evidence.
- `[WalkRxGuard] refused: guard_off_no_rx_verdict` at `Finding GameEngine::LoadMap` → THE
  predicted first stop: admission machinery worked end-to-end and the boot reached the known
  GATE-1 blocker; zero reads were attempted (fail-closed), so nothing crashed — this is the
  T2/M-A experiment's starting line, not a failure of the admission chain.
- `rx_transition_denied` (fold-1 only, any resolve site) → T2 proved the tail page; a real
  code page refused its transition — loud, zero-read, the guard working as designed.
- repeating `Still waiting for object construction…` / `Still searching for
  KismetStringLibrary…` / `Still waiting for required object…` → honest named waiting on
  data-side reads; distinguish a slow engine from an owner stop (StartupWait deadline,
  PROG:1020-1028) by the final runtime-status line.
- `failed 1 ` with EMPTY detail right after `Fatal error! The following objects were never
  constructed:` → the UI:2195 empty-message throw; the names above it are the diagnosis.
- log ends mid-`Finding ProcessInternal` with NO failed line → the FLAG-3 naked Zydis read
  faulted: the staged root is missing the settings-v3 `[Hooks]` zero keys — a staging gap,
  not a chain regression (and it cannot occur if step 18's keys are staged).
- PLH SEV + one Error right after `Hooking StaticConstructObject for searcher pools` → the
  owned-memory soft-refuse of a futile arm (step 19); expected composition noise, boot
  continues.

---

## 3. Dependency table — what each stopping point needs

| stopping point / step | needs T2 console probe + measured fold flip (walkguard M-A) | needs merged-luamod `facf993a` (in successor: YES per manifest) | new work |
|---|---|---|---|
| steps 1-8 (admission: pool, ctor probe, SCO bind) | no — data-side + one call-only ctor entry | no (initializer-side) | copyin diff re-derivation onto `1660b749` IF the admitting compose includes the copyin trio (manifest-required atom; its initializer hunks are still on `e7f349af`) |
| steps 9-13 (version line, object wait, Kismet, required objects, CDOs) | no | no | staged-root confirmation only: `[EngineVersionOverride] 5/6` bytes in the root (834 B restage, worker-engineversion-ini §3 lockstep law) |
| step 14 LoadMap (and every later walk: Tick 1987, InitGameState…UStruct::Link 2113-2181) | **YES — this is exactly the M-A route**: run the staged armed probe (`modbridge_exec_write_probe_armed.prx`, T2 row 0), get `rc==0` ⇒ `W_WORKS`, controller flips `RC_ASM_WALK_RX_GUARD 0→1` (expected object `f5627427…`), recompose; without it the step REFUSES forever at fold-0 | no | the T2 run itself (console, user-coordinated; walkguard-gate REPORT §3 step 1) + the one-token measured flip + recomposition (controller) |
| step 15 R3 deferral lines | no (they fire INSTEAD of the detour in both worlds; the vtable Tick ASSIGNMENT needs fold-1, the deferral line itself does not) | initializer-side (UI R3 block) | none |
| step 18 ProcessInternal skip | no (independent of walks: the Zydis read is unguarded even in fold-1) | no | staged-root confirmation: settings-v3 `[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0` (+614 B lineage) — without them the boot CRASHES here even after GATE-1 is lifted |
| step 19 SCO soft-refuse noise | no | no | none (owned-memory layer law; futile by design) |
| steps 20-21 engine-ready/mods-started acks + event loop | only in the sense that everything after step 14 needs GATE-1 lifted first | no (program+backend side) | none |
| step 22 marshal arm + LoadMap deferral Warning | no for the Warning (pump_pending is set by the initializer regardless); the ARM needs live carrier ticks (F-1 open), not T2 | **YES — the calibration consumer, deferred LoadMap arm, and marshal-first ladders exist only in `facf993a`** | none beyond the successor admission itself |
| step 23-24 mods + banner | needs GATE-1 lifted (source-order: setup_unreal must RETURN before the event loop exists) | **YES** (start_mod/load path incl. the string fix lineage) | loadlist/mods.txt/main.lua staging correctness (console-side; `Mod is absent…` shapes on miss) |
| pad `pad_*` sdiag tags | no | no (wpd2/coexist staging is NOT in the successor manifest) | new work, and NOT required for the banner — wpd1 defers instead |

---

## 4. First admitting boot — evidence interpretation card

| # | observable pattern (any sink) | verdict |
|---|---|---|
| 1 | `[Seeds] slot 0 g_uobject_array bound` + `[Seeds] slot 3 gmalloc bound … V1 instance + V2 vptr match` + `[Seeds] FName pool validated at 0x9765EC0 …` in order | ADMITTED — the pool probe worked; chain proceeds to the ctor bind. Expected. |
| 2 | gmalloc bound line present, then runtime-status `failed 1 [Seeds] REFUTED: FName pool …` and nothing else | Pool admission REFUSED after slots 0/3 bound (no rollback, by design). The admission gate did its job; the chain never started. Expected refusal class — reconcile, never replay. |
| 3 | `failed 1 [Seeds] REFUTED: … span table unavailable (bridge published 0 readable spans …)` (sdiag `bridge_spanpub_count=0`) | Bridge span walk empty at module_start. Honest loud refusal; in the copyin-wired successor, check whether the probe ran (its tags) — its absence is the finding. |
| 4 | `[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified …; detour verification skipped` present | The boot's first live code entry through a seeded address returned and matched the pool id — the biggest live unknown of admission is cleared. Expected on PASSES. |
| 5 | `Waiting for object construction...` then repeating `Still waiting for object construction, N objects constructed so far`, eventually a stop | Data-side reads healthy; engine object construction below 10000 at stop time, or an owner stop via StartupWait. A waiting state, not a crash — read the final runtime-status line to disambiguate. Expected shape if it appears. |
| 6 | `Finding GameEngine::LoadMap` as the LAST initializer line + `[WalkRxGuard] REFUSED: NO read attempted; …` (Error) + runtime-status `failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict (walk text read not permitted; no read attempted; detail=0)` + later `Stopping mod '<name>' for uninstall` lines | **THE PREDICTED FIRST STOP (GATE-1, fold-0).** Admission succeeded; the boot walked into the known exec-read blocker and died fail-closed with zero reads. This is expected, not a regression — it is the exact precondition the T2/M-A experiment exists to lift. |
| 7 | `GameEngine::LoadMap address 0x…` present and NO `[WalkRxGuard]` lines | Fold-1 world: the T2 verdict landed and the measured flip was composed in. GATE-1 lifted; continue reading down the chain (step 15 next). Expected only if M-A was deliberately landed. |
| 8 | `Scan failed, falling back to vtable address for GameEngine::Tick` + `[R3] refused: EngineTick pre-callback registrations …` + `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` | Correct R3 posture: Tick assigned data-side via vtable, detour registrations compiled out, pump deferred to the marshal. Not errors — the designed deferral. |
| 9 | Log ends at/near `Finding ProcessInternal` with NO runtime-status failed line (process gone) | FLAG-3 naked Zydis read of exec text faulted: the staged root lacks the settings-v3 `[Hooks] HookProcessInternal=0`/`HookProcessLocalScriptFunction=0` keys. A staging gap (fixable by restage), not a chain regression; cannot occur with the keys staged. |
| 10 | `Post-initialization: GMalloc: 0x… -> 0x…` then a PolyHook SEV line + one Error right after `Hooking StaticConstructObject for searcher pools`, then the size lines continue | The owned-memory soft-refuse of the SCO post-callback arm: crash-free, throw-free, futile by composition (ERROR_ID ignored). Expected noise; boot continues toward `engine-ready` → `Starting mods…` → `[Lua] [TownfallItemSpawner] Loaded v1.0 …` (the banner). |

---

## 5. Honest bounds

- Source-and-report derived only; zero console contact; nothing staged, flipped, composed,
  or launched; no outcome claims. Every prediction is an evidence-shape prediction.
- Line numbers are from the units/staging files as read today (shas in §0); any further
  controller edit re-anchors them. The copyin initializer diff remains derived on `e7f349af`
  and MUST be re-derived onto `1660b749` before its admission (the successor manifest says
  the same).
- The ctor-probe PASSES prediction rests on derivation-grade evidence (fixtures + 9 call-site
  cross-checks + pool corroboration); the FC1 read of the ctor bytes was never natively
  executed. A first-boot mismatch or fault here would be NEW evidence, not a contradiction.
- GATE-5 (LuaMod eager LoadMap) is CLOSED in the successor by the merged gate5-loadmap fold
  (defer + one-shot re-arm; host-tested 10/10, worker-gate5-loadmap receipt); GATE-2 stays
  closed by design (R3 marshal is the only pump); GATE-3 (pad) is deferred by wpd1 and
  independent of this chain; GATE-4's five availability-first ladder sites are repaired only
  in `facf993a` (r3-ladder fold, host 40/40) — one more reason the merged luamod must be in
  the admitting compose.
- F-1 (carrier invocation rate) is open: `[R3] FuncPtr-marshal armed …` proves arming only;
  a draining pump is proven by drained closures, which no evidence shape in this map can
  substitute for.

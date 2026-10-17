# CARRIER-CANDIDATES.md — offline closure attempt for R3 open fact F-1 (the FuncPtr-marshal calibration carrier)

Worker: worker-r3-carrier-prep, 2026-10-04. OFFLINE DERIVATION ONLY: no console contact,
no nested subagents, nothing executed in-game, nothing staged. Read-order contract
honored: `TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md` (R3D) read in full first;
its §4.2 calibration protocol and §6 condition 1 (F-1) are the contract this document
feeds. Read-order credit: `TEAM:worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` (L10M),
`TEAM:worker-initializer-integration/SEED-TABLE-FORMAT.md` (STF), `TEAM:worker-fc1-validator-probe/STAGE-PLAN.md`
(FC), `TEAM:worker-record-sink-addon/STAGE-PLAN-QDRV1.md` (QV1),
`TEAM:worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (IS).

Path abbreviations:
- `TEAM=` `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/`
- `IMG=` `.../what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  (sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194,241,424 B —
  sha re-verified this session; the exact image pinned by STF §1 and consumed by the seed table)
- `PORT=` `.../native-dependency-spike/native-coherent-runtime-1/source-reconstruction-final-4/port/`
- `THIS=` `TEAM:worker-r3-carrier-prep/`

---

## 0. Executive summary

1. **F-1 is NARROWED, not CLOSED.** Offline evidence settles *which candidate UFunction
   names exist as reflected, startup-constructed names in the shipped image* and *which
   plausible names do not exist at all*. It cannot settle FUNC_Native (binding tables are
   not statically readable in this build — §2), invocation rate, or game-thread identity.
   Those are exactly R3D §4.2 steps 2–4's measurements; this document hands that
   calibration a ranked, name-resolved, string-vaddr-anchored candidate list so its first
   in-game session can be one-pass.
2. **The community-standard carrier is dead on this title:** `PostRender` — the usual
   per-frame UE4SS hook target — has ZERO exact occurrences in the image (UTF-16 or
   ASCII). Same for `DrawHUD`, `PlayerTick`, `UpdateCamera`, `UpdateGameMode` (the task
   hint), `TickComponent`, `TickActor`, `NativeTick`, `K2_GetActorLocation` (UTF-16),
   `BeginDeferredActorSpawnFromClass` (UTF-16), `FinishSpawningActor` (UTF-16). These are
   name-level refutations, decidable offline, now removed from the calibration budget.
3. **Two surviving image-confirmed families, one ambiguous:**
   `AActor::ReceiveTick` (ctor-region LEA at 0x2b45cbc + 4 additional references from
   game-region code that looks the name up at runtime) and
   `UAnimInstance::BlueprintUpdateAnimation` (ctor-region LEA at 0x2bba9e5), plus the
   ambiguous exact name `"Tick"` (owner unknown offline; census resolves). Two
   thread-inadmissible names are EXCLUDED with reasons: `ReceiveAsyncPhysicsTick`
   (physics thread) and `BlueprintThreadSafeUpdateAnimation` (parallel worker threads).
4. **Native-ness (FUNC_Native) is NOT offline-resolvable here** (§2 — three oracles, one
   refuted mechanism). R3D §4.2 step 1's filter (`FUNC_Native` + Func non-null) and the
   §4.3 identity pre-check therefore run IN-GAME as calibration step 1. A recorded
   tension exists if the top candidates turn out script-only (§7.3) — it is the
   controller's decision, not a change this worker claims.
5. **Deliverables for the launch-10/11 both-fail branch (L10M row R5/R9):** the cc1
   ridealong probe spec (§6) — a standalone, data-side-only, fc1/qdrv1-lineage prx that
   answers the NAME census without the port runtime — plus the exact jsonl record shapes
   for the in-runtime R3 calibration (§5), so F-1 is falsifiable in ONE session either way.

---

## 1. Carrier requirements (precise, from R3D)

A calibration carrier is a UFunction F on this title such that ALL of:

| # | requirement | source | offline status |
|---|---|---|---|
| C-1 | **Reflected**: a real UFunction UObject created at class construction (name interned; ctor region exists) | R3D §4.1 (`RegisterPostHook` needs a UFunction object) | CLOSED per candidate (name string + ctor-region LEA + FName-ctor call evidence, §3/§4) |
| C-2 | **Native + bound**: `FUNC_Native` set and Func non-null (R3D §4.2 step 1; §4.3 identity pre-check requires non-null, != ProcessInternal, native) | R3D §4.2/§4.3; gate shape `LUA:4618-4620` | **NOT offline-resolvable** (§2) — in-game calibration step 1 |
| C-3 | **Per-frame or near-per-frame invocation**, steady-state, NOT player-action-gated (R3D §4.2 step 4 refuses action-gated carriers; input-correlated class acceptable as secondary per R3D §4.2 end) | R3D §4.2 | NOT offline-resolvable — in-game rate measurement |
| C-4 | **Game-thread identity**: invocations arrive on one thread, the game thread (drain gate `IsInGameThreadRaw()` + first-publication identity, R3D §4.3/§4.4a) | R3D §4.3 | Engine-knowledge EXCLUSIONS applied (async-physics / worker-thread names removed, §4); final proof in-game |
| C-5 | **SetFuncPtr-safe**: the UFunction instance is heap RW (igf4 proved the write class on GUA memory; per-span fresh mapscan before first store per the C-2 discipline) | R3D §1.2/§4.3 | Method-level, carried unchanged; per-candidate nothing new offline |
| C-6 | **Callable early** (post-init, before/independent of mods-started; R3D §4.4a defers arming to post-`setup_unreal()`) | R3D §4.4a | Ranking input: viewport/world-frame candidates rank above gameplay-only ones |
| C-7 | **Engine-driven**: fires independently of the mod (a function the MOD itself calls can never drain the queue the mod is waiting on — IS's 6 `ExecuteInGameThread` closures, `IS:695,700,705,710,716,725`, are the queued work, not the pump) | derived this session from R3D §2 + IS | Applied: mod-called natives (LineTraceSingle etc.) EXCLUDED as carriers |

---

## 2. The offline oracle: what the pinned image can and cannot answer

### 2.1 Image identity and segment map (verified)

`IMG` parses as ELF64 (14 phdrs). Loadable segments (vaddr = linked label convention;
runtime = base 0x400000 + label, per STF §3):

| seg | flags | vaddr range | content |
|---|---|---|---|
| 0 | X-only (0x1) | `[0x0, 0x7479b1c)` | text (~122 MB) |
| 1 | R | `[0x747c000, 0x7EBA918)` | rodata incl. packed UTF-16 + ASCII string pools |
| 3 | RW | `[0x7ebc000, 0x913c000)` | data |
| 7 | RW | `[0x913c000, …memsz 0x4f7418)` | L7 span (runtime bias 0x953c000 — matches igf3 `bias_l7_start_pred`); FName pool label 0x9365ec0 sits in its BSS tail |
| 8 | R | `[0x9633420, …)` | dynamic/reloc data |

Cross-checks vs retained evidence passed: seg7 vaddr == L7 linked base; ctor label
0x23d590 lands in seg0 text (it is also visible as a repeated `E8` callee in ctor
regions, §2.3); pool label lands in seg7 BSS. The X-only text segment is consistent with
the prot=4 read-refusal facts (igf6 class).

### 2.2 Oracle results (three attempts, one refuted mechanism)

1. **In-place `{name-ptr, thunk-ptr}` pairs: REFUTED.** Zero hits for every candidate
   including must-be-native positive controls (`LineTraceSingle`, `SetText`), across both
   fwd and reversed 16-byte shapes. The image does not carry baked native-registration
   pointer tables at the candidate names.
2. **DT_RELA addend pairs: REFUTED.** 1,649,556 R_X86_64_RELATIVE entries (DT_RELA
   0x9653430, RELACOUNT 0x192b94, addends: 1,337,475 text / 182,690 rodata / rest data)
   were indexed (`rela_index.bin`, left beside IMG, not copied). A blind census of
   adjacent relocs (Δoffset=8) shaped {rodata-string, text-thunk} found EXACTLY ONE pair
   in the whole image — decoding to `"pt"`, a false positive. Positive controls fail
   here too. Conclusion: UE5.6's native binding in this build is NOT a static
   name→thunk data table readable from the image.
3. **RIP-relative LEA code references: POSITIVE and load-bearing.** 9,248 LEA sites
   reference the candidate string vaddrs (`THIS:lea-refs.json`). The decisive shape:
   each candidate function name is referenced from a TIGHT class-construction region
   whose functions call the FName ctor **0x23d590 — the seeded slot-2 label** (`THIS:ctor-windows.json`
   shows the repeated `call 0x23d590` adjacency). This is the UFunction-construction
   path: name string → FName ctor → UFunction ctor. It evidences C-1 (reflected,
   startup-constructed) per name.
4. **Flags immediate extraction: attempted, not reliable.** The recurring immediate
   `0x1ba04` adjacent to candidates decodes under the port's authoritative
   `EFunctionFlags` (`PORT:unreal/include/Unreal/UnrealFlags.hpp:343-380`) to
   Final|BlueprintAuthorityOnly|Exec|Event|NetResponse|Static|UbergraphFunction|MulticastDelegate —
   a garbage composite, i.e. it is NOT the flags argument. Extracting the real
   `FunctionFlags` (FUNC_Native = 0x00000400) needs a real sequential disassembler
   (capstone) over the ctor functions — recorded as FUTURE OFFLINE WORK, not a claim.

### 2.3 Offline-verdict vocabulary used below

- `NAME-CONFIRMED` — exact NUL-terminated UTF-16 occurrence in IMG + ≥1 text LEA
  reference from a class-construction region (C-1 evidence).
- `NAME-REFUTED` — zero exact occurrences (UTF-16 AND ASCII) in IMG.
- `AMBIGUOUS` — name exists but owning UClass not decidable offline (multiple classes may
  declare same-named UFunctions; the in-game census resolves by object walk).
- Native-ness has NO offline vocabulary in this build (§2.2).

---

## 3. The ranked candidate list

Rate/thread columns are PREDICTIONS for calibration ranking only, labeled per R3D §4.2's
refusal discipline. All vaddrs are linked-label convention (runtime = 0x400000 + vaddr).

### Rank 1 — `ReceiveTick` (engine knowledge: AActor::ReceiveTick, confidence high)

| record | value |
|---|---|
| name string (exact UTF-16) | vaddr `0x76c0264` (rodata pool; `"K2_OnReset\0ReceiveTick\0RecoveredFaults\0…"` neighborhood, `THIS:lea-refs.json`) |
| LEA references | 9 sites: ctor clusters `0x2b45cbc` (the AActor-family blob, §4.1) AND `0x2dd0ba0` (a Character-family blob — `ReceiveBeginPlay`/`ReceiveEndPlay` are referenced at `0x2dd0b70`/`0x2dd0b88` in the same function run) + **game-region references `0x4409eb1`, `0x4414368`, `0x44179a4`, `0x442fb44`** — the title's own code resolves this name at runtime (the same 0x44xxxxx region that constructs the game's `ItemDatabaseSubsystem` per §4.1's sibling evidence at 0x45a5587) |
| C-1 | NAME-CONFIRMED |
| C-2 (native) | UNKNOWN offline — calibration step 1 |
| C-3/C-4 (rate/thread) | PREDICTION: invoked via `TickActor` for every ticking actor; a loaded world always contains ticking actors (controllers, managers, WorldSettings) so a steady ≥1/frame aggregate rate is plausible; on the game thread. Refuted only by measurement |
| calibration record | `carrier_candidate:"ReceiveTick", expected_class:"Actor", name_vaddr:"0x76c0264", ctor_cluster:"0x2b45cbc", game_refs:4` |

Why it outranks everything else: the only candidate that is BOTH name-confirmed in a
ctor region AND independently referenced by the game's own runtime code; world-frame
independence (fires in menus and in-game alike if any actor ticks).

### Rank 2 — `BlueprintUpdateAnimation` (engine knowledge: UAnimInstance, confidence high)

| record | value |
|---|---|
| name string | vaddr `0x773e91e` (AnimInstance ctor region, `THIS:lea-refs.json` 0x2bba9e5) |
| LEA references | ctor region only (1 site) |
| C-1 | NAME-CONFIRMED |
| C-2 | UNKNOWN offline |
| C-3 | PREDICTION: per-frame per animating mesh component during gameplay — rate can EXCEED frame rate; but ZERO on a static main menu and it collapses when no animating skeletal mesh ticks |
| C-4 | THREAD RISK: anim updates can run on worker threads (parallel update); the region also contains `BlueprintThreadSafeUpdateAnimation` — proof this title has the TS-anim machinery compiled — so identity publication must prove game-thread stability before this carrier is accepted |
| calibration record | `carrier_candidate:"BlueprintUpdateAnimation", expected_class:"AnimInstance", name_vaddr:"0x773e91e", ctor_cluster:"0x2bba9e5", thread_risk:"parallel_anim_update"` |

### Rank 3 — `"Tick"` (AMBIGUOUS owner — discovery-tier)

| record | value |
|---|---|
| name string | vaddr `0x7521050` (single exact UTF-16 hit; 588 substring occurrences overall — most are non-UFunction uses) |
| LEA references | 2 sites: `0x2941013` (engine region, near PlayerCameraManager-family ctor names K2_OnBecomeViewTarget/K2_OnEndViewTarget) and `0x4571967` (game region) |
| C-1 | NAME-CONFIRMED, owner AMBIGUOUS |
| C-2/C-3/C-4 | unknown; the census/calibration resolves owner (UClass walk) and rate per owner |
| calibration record | `carrier_candidate:"Tick", expected_class:"unresolved_multi", name_vaddr:"0x7521050", lea_sites:["0x2941013","0x4571967"]` |

Kept because the calibration's bounded trial set can afford one ambiguous name and the
census will enumerate its owning class(es) cheaply.

### Rank 4 (excluded with reasons — do NOT arm)

| candidate | verdict | reason |
|---|---|---|
| `PostRender` | NAME-REFUTED | 0 exact hits (UTF-16 + ASCII). The community-standard carrier does not exist in this build |
| `DrawHUD`, `ReceiveDrawHUD` as carriers | REFUTED / LOW | `DrawHUD` 0 hits; `ReceiveDrawHUD` NAME-CONFIRMED (vaddr `0x76c26a0`, LEA 0x30a67cd, AHUD region) but fires only via the AHUD canvas path the game may not use; BIE-class risk |
| `ReceiveAsyncPhysicsTick` | NAME-CONFIRMED, EXCLUDED | AActor ctor region lists it (`§4.1`) — async physics tick runs on the PHYSICS thread: fails C-4 by construction |
| `BlueprintThreadSafeUpdateAnimation` | NAME-CONFIRMED, EXCLUDED | runs on parallel anim worker threads: fails C-4 by construction |
| `PlayerTick`, `UpdateCamera`, `UpdateGameMode`, `TickComponent`, `TickActor`, `NativeTick`, `EngineTick`, `ProcessEvent` (as names), `K2_GetActorLocation`/`GetControlRotation`/`LineTraceSingle`/`BeginDeferredActorSpawnFromClass`/`FinishSpawningActor`/`GetItemIDFromDeprecatedEnum`/`GetInteractablePickupIDActorPath`/`SetText` (UTF-16) | NAME-REFUTED (UTF-16 exact) or EXCLUDED | C++-virtual (not UFunctions) or mod-driven (C-7: LineTraceSingle exists UTF-16 at `0x79ac4a0` but is port-API-called; the game natives appear ASCII-only — likely pak-side or log strings; either way C-7 excludes them) |
| `ReceiveBeginPlay`, `ReceiveEndPlay`, `ReceiveDestroyed`, `UserConstructionScript`, `BlueprintInitializeAnimation`, `ReceiveAnyDamage` | NAME-CONFIRMED, LOW | once-per-lifetime or spawn-gated: fail C-3 (kept in the census list as decoder controls — their ctor-region membership validates the name pipeline) |

---

## 4. Offline-derived data the in-game walk consumes

### 4.1 The AActor-family function-name inventory (ctor region 0x2b45000–0x2b48000)

54 distinct names enumerated (`THIS:region-names.json`). Function names (the UFunction
set): `K2_OnBecomeViewTarget, K2_OnEndViewTarget, K2_OnReset, ReceiveActorBeginCursorOver,
ReceiveActorBeginOverlap, ReceiveActorEndCursorOver, ReceiveActorEndOverlap,
ReceiveActorOnClicked, ReceiveActorOnInputTouchBegin, ReceiveActorOnInputTouchEnd,
ReceiveActorOnInputTouchEnter, ReceiveActorOnInputTouchLeave, ReceiveActorOnReleased,
ReceiveAnyDamage, ReceiveAsyncPhysicsTick, ReceiveBeginPlay, ReceiveDestroyed,
ReceiveEndPlay, ReceiveHit, ReceivePointDamage, ReceiveRadialDamage, ReceiveTick,
UserConstructionScript` — plus property names (`RootComponent0, bReplicateMovement, Role,
RemoteRole, Owner, bHidden, bTearOff, bCanBeDamaged, Instigator, AttachmentReplication,
ReplicatedMovement`) and the PlayerCameraManager K2_* trio in the same blob. This is the
offline stand-in for the in-game UClass function-map enumeration: the calibration's
per-class candidate list for `Actor` is THIS list, filtered to the §3 ranks.

### 4.2 UAnimInstance family (ctor region 0x2bba000–0x2bbc000)

`BlueprintBeginPlay, BlueprintInitializeAnimation, BlueprintLinkedAnimationLayersInitialized,
BlueprintPostEvaluateAnimation, BlueprintThreadSafeUpdateAnimation, BlueprintUpdateAnimation`.

### 4.3 Class names for the GUA walk (all NAME-CONFIRMED, exact UTF-16 vaddrs)

`Actor` 0x75dd4a4, `Pawn` 0x78275c4, `Character` 0x75f1fc2, `PlayerController` 0x7765882,
`GameModeBase` 0x783a91a, `GameStateBase` 0x76c22e0, `GameViewportClient` 0x761c342,
`HUD` 0x7577a14, `PlayerCameraManager` 0x77bb500, `WorldSettings` 0x78b4150,
`LevelScriptActor` 0x7812e02, `AnimInstance` 0x75f17ee, `UserWidget` 0x773d344,
`ItemDatabaseSubsystem` 0x77eff56, `Info` 0x7a151a4, `Engine` 0x782706e, `PlayerState`
0x77927fe, `Controller` 0x78b53b6, `CameraComponent` 0x759e336. The runtime walk
addresses come from the seeded table, not from these strings: GUObjectArray slot 0
(label 0x93ba110, runtime_expected 0x97ba110) and FName ctor slot 2 (label 0x23d590) are
already `present:true` in the landed table (STF §3) — the walk needs NOTHING new seeded.

### 4.4 The GUA walk itself (protocol, post-launch-9)

Exactly the STF §5 chain plus the port's own object services (R3D §4.2 step 1):

1. `FName::ConstructorInternal.assign_address(base + 0x23d590)`; validate with the STF §5
   behavioral probe (`FName(STR("ByteProperty"), FNAME_Find)` == pool-read `+0x14044` id).
   Under the seed-bound armed path this is ALREADY DONE before any DoScan arm (STF §4/§5).
2. For each §3 candidate + §4.3 class: `FName(STR(<name>), FNAME_Find)` — never FNAME_Add
   (pool mutation guard).
3. `FindFirstOf`/StaticFindObject-style GUObjectArray iteration (port data-side walk,
   proven class: igf4 reads at 0x97ba110; SINI:1425-1432 ExecuteUbergraph resolution is
   the same pattern) to resolve the UClass by name, then the UFunction by name.
4. Read `FunctionFlags` + Func pointer data-side; compare Func against the §2.2-negative
   insight: there is NO offline thunk constant to cross-check for THIS build, so the
   identity anchors are (a) flags bits, (b) Func != ProcessInternal address, (c) Func
   non-null — exactly R3D §4.3's pre-check. Record all three per candidate.

---

## 5. Per-candidate calibration records — the probe jsonl shapes (in-runtime, R3D §4.2)

Envelope discipline follows the v3 writer lineage (FC §7 / QV1 §2): strict JSON < 1 KiB
per line, `marker=modbridge.ingamefacts, schema=1`, distinct `build_id`, own log path.
The calibration (R3D §5.2 item 4) writes into the module's saved state AND this channel.
Proposed build_id prefix: `r3cal-<8 hex>`; log path `/data/modbridge-carrier-facts.jsonl`
(NEW file — cc1 (§6) shares this path only if it is the same module; as separate prx it
uses the same path under its own build_id, single-writer by the same-module rule).

```json
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":1,"exp":"entry","rc":0,"errno":0,"fields":{"module":"r3cal"}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":2,"exp":"startup","rc":0,"errno":0,"fields":{"gate":"post_setup_unreal","fname_ctor_bound":1,"byte_property_probe":1}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":3,"exp":"carrier_discover","rc":0,"errno":0,"fields":{"candidate":"ReceiveTick","name_vaddr":"0x76c0264","fname_id":<u32>,"uclass_found":1,"ufunction_found":1,"func_flags":"<hex>","func_native":0|1,"func_ptr":"0x<16hex>","is_processinternal":0|1,"mapscan_gate":1}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":4,"exp":"trial_armed","rc":0,"errno":0,"fields":{"candidate":"ReceiveTick","trial_index":0,"pre_hook":1,"counter_slot":0,"trials_armed":1,"trials_cap":64}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":5,"exp":"rate_sample","rc":0,"errno":0,"fields":{"candidate":"ReceiveTick","window_s":<port-clock sec>,"invocations":<n>,"rate_hz":<f>,"thread_id_key":"0x<16hex>","identity_first_publication":1,"steady":0|1,"idle_window_invocations":<n>}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":6,"exp":"carrier_selected","rc":0,"errno":0,"fields":{"selected":"ReceiveTick","rate_hz":<f>,"refusals":{"action_gated":0,"thread_unstable":0,"native_missing":0},"demoted":["Tick@<class>","BlueprintUpdateAnimation"],"f1_outcome":"OPEN->FOUND" }}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":7,"exp":"f1_verdict","rc":0,"errno":0,"fields":{"f1":"FOUND|FALSE","reason":"<rate/class evidence>","trial_table_records":<n>,"marshal_armed":0|1}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"r3cal-<hash>","seq":8,"exp":"summary","rc":0,"errno":0,"fields":{"candidates_tried":<n>,"candidates_native":<n>,"write_failures":0}}
```

Selection rule (R3D §4.2 step 4, made concrete): max `rate_hz` among candidates with
`steady:1`, `identity_first_publication:1`, one stable `thread_id_key` across windows,
`idle_window_invocations > 0` when idle (player-action-gate refusal otherwise), and
`func_native:1` (see §7.3 for the recorded tension if the top ranks come out
`func_native:0`).

---

## 6. The launch-10/11 ridealong probe spec — cc1 (fc1 → qdrv1 → cc1 lineage)

### 6.1 Why a standalone prx is needed and what it honestly can/cannot deliver

Under the both-fail branch (L10M rows R5/R9) the composition's runtime-status may end
before any Lua or calibration runs; cc1 rides INDEPENDENT of the composition exactly
like qdrv1 (QV1 §1: no symbol resolution against the composition; reads only pinned
engine memory + its own log). cc1 is DATA-SIDE ONLY — it can therefore run in EVERY
launch window including `T-A` sessions where nothing else binds.

**cc1 delivers:** (a) the carrier-NAME census — which §3/§4.1 candidate names are
interned in the LIVE FName pool (runtime truth, including BP-class names the image can
never show, e.g. any `"PostRender"`-style surprise or game-specific per-frame events);
(b) GUA admission + survival evidence (qdrv1 lineage); (c) non-interference proof.

**cc1 does NOT deliver:** FUNC_Native (needs the UFunction object walk — UClass field
offsets are worker-2's open B0; cc1 must not assume them), invocation rate (needs
hooks), thread identity. Those stay with the R3D §4.2 in-runtime calibration (§5).
This division is the honest answer to "close what's closable offline/standalone".

### 6.2 Mechanism (all pieces already proven in lineage)

- Pool decode reuses fc1's VALIDATED structural offsets and decoder: `CurrentBlock`
  @ pool+0x8, `Blocks[0]` @ pool+0x10, entry u16 header (bIsWide bit0 / Len bits15:6),
  `Blocks[id>>16] + (id&0xFFFF)*2` (FC §7 shapes; pool label 0x9365ec0 from the landed
  seed table `fname_remedy`; heap span 0x8018c000–0x100118000).
- Admission gates reuse qdrv1's: header-window GUA admission (admission_rcode 1..7 → 0),
  `refused_bindings_absent` wiring-lag-by-one-poll, `refused_layout_gate_closed` count
  riding above the 10000 threshold (QV1 §2).
- Cadence: ONE detached thread, 30 polls x 4 s (~2 min), bounded spin, NO mutex imports;
  writer = own `sceKernelOpen/Write/Close` trio, `O_APPEND|O_CREAT` 0600, one
  `sceKernelWrite` per <1 KiB line (C-8 single-writer discipline: NEW path
  `/data/modbridge-carrier-facts.jsonl` — NOT qdrv1's file).
- Census budget: bounded entries-scanned counter; match ONLY the fixed candidate list
  (≤ 96 names; the §4.1/§4.2 families + §3 + controls `"None"`, `"ByteProperty"`,
  `"PostRender"`-negative, `"ZZZNotARealFunctionQQ"`-negative); emit matched names +
  ids only; progress marker every 4096 entries; hard stop at budget with
  `census_truncated:1` honesty field.

### 6.3 Expected record shapes

```json
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":1,"exp":"entry","rc":0,"errno":0,"fields":{"module":"cc1"}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":2,"exp":"startup","rc":0,"errno":0,"fields":{"polls":30,"poll_s":4}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":3,"exp":"mapscan","rc":0,"errno":0,"fields":{"trusted":0|1,"pool_probe":"0x9365ec0","current_block":<u32>}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":4,"exp":"qh_counter","rc":0,"errno":0,"fields":{"poll":P,"phase":"bindings_absent|gate_closed|census|terminal","admission_rcode":0..7,"gua_num_elements":<n>,"refused_bindings_absent":B,"refused_layout_gate_closed":G,"census_entries_scanned":<n>,"census_progress_markers":<n>}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":5,"exp":"census_result","rc":0,"errno":0,"fields":{"verdict":"CENSUS:VALIDATED","controls":{"None_id_zero":1,"ByteProperty_ascii_ok":1},"negative_hits":{"PostRender":0,"ZZZNotARealFunctionQQ":0},"matched":[{"name":"ReceiveTick","id":<u32>},{"name":"Tick","id":<u32>},{"name":"BlueprintUpdateAnimation","id":<u32>}],"matched_count":<n>,"census_truncated":0,"budget_entries":<n>}}
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-<hash>","seq":6,"exp":"summary","rc":0,"errno":0,"fields":{"write_failures":0,"polls_run":<n>,"polls_admitted":<n>}}
```

Refusal shapes (terminal, never re-rated — FC §7 discipline): `{"exp":"census_result",
"fields":{"verdict":"CENSUS:REFUSED","reason_code":"pool_window_uncovered|none_ascii_mismatch|current_block_insane|pool_uninitialized_after_grace"}}`.
A grace refusal means launch-window timing — the remedy is a NEW staged window, never a
re-rating of unchanged evidence.

### 6.4 Staging

fc1 STAGE-PLAN §0–§4 pattern verbatim: hash-pin the prx before upload, SHT-strip
readback law (RETR oracle: decrypted inner ELF bytes + expected size + inner sha),
append-only ploader.ini line at the NEXT FREE index (never edit the composition line),
count-verified ini, rollback = restore backup / remove the appended line; logs are owned
evidence, never deleted. Launch ritual and one-launch-per-staged-build per QV1/igf3 §3.
Nothing is built or staged by this worker; a builder worker owns compile+receipts.

### 6.5 Non-interference bounds (audit-facing)

One thread, bounded sleeps, no engine writes of any kind (read-only pool/GUA header
windows), no hooks, no mutex, single log file, ≤ 30 polls. Rides any row R1–R9 including
command-free rows (R6/R7/R8) since it stages without recomposition.

---

## 7. The honest verdict on F-1

### 7.1 What offline evidence CLOSED this session

- The candidate SPACE: `PostRender`-class per-frame names are absent from the shipped
  image (§3 rank 4) — the calibration budget no longer spends trials on them.
- The surviving candidate SET is 2 ranked names + 1 ambiguous, each with exact string
  vaddrs and ctor-region anchors (§3), and the walk's class-name list (§4.3) with all
  address slots it needs already seeded and launch-proven (STF §3/§5).
- The image contains NO offline-resolvable native binding table (§2.2) — a negative
  result that changes the calibration: step 1 (flags/Func read) is mandatory per
  candidate, and no offline thunk crosscheck exists for this build.

### 7.2 What remains OPEN and which probe closes it

| open sub-fact | closed by | when |
|---|---|---|
| candidate names interned in the LIVE pool (incl. BP-only names) | cc1 census (§6) | any launch-10/11 session, even `T-A` |
| `FUNC_Native` + Func non-null per candidate | R3D §4.2 step 1 / §5 record `carrier_discover` | in-runtime calibration |
| steady ≥1/frame game-thread rate, not action-gated | R3D §4.2 steps 2–4 (`rate_sample`) | in-runtime calibration |
| game-thread identity stability | R3D §4.4a publication logic | in-runtime calibration |

### 7.3 The recorded tension (controller decision, pre-declared)

R3D §4.3's identity pre-check and the `LUA:4618-4620` gate REFUSE non-native UFunctions.
If calibration finds `ReceiveTick`/`BlueprintUpdateAnimation` are script-only on this
title (possible: both are BP-event-class names; BIE-class risk is stated in §3), then a
STRICT gate makes F-1 lean FALSE **even though the functions still fire and a SetFuncPtr
hook would mechanically intercept them** (R3D §1.4, with the `:198-202` ProcessInternal
fallback). R3D §4.6 already scoped a per-function script-function extension as possible
but not required. This document does NOT change the gate; it RECORDS that the §5
`f1_verdict` record must distinguish `"FALSE: no native carrier"` from
`"FALSE: native gate refused a firing non-native carrier"` — the second outcome is a
design decision the controller can take up with the R3D owner (a scoped §4.6-style diff),
not a measurement failure.

### 7.4 Bottom line

F-1 is no longer "does ANY carrier exist on Townfall" — it is "is `ReceiveTick`
(or `BlueprintUpdateAnimation`, or the `Tick`-owner census resolves) native, steady,
game-thread-stable". That is ONE calibration session against a ≤3-candidate trial set,
falsifiable either way, with the cc1 census ridealong available in EVERY launch window
as the independent name-truth channel. If the calibration records `f1:"FALSE"` with the
trial table, R3D §4.2 step 6 fires and the R3-only spawner verdict becomes NO loudly —
exactly as R3D promised; this document just made that outcome cheap to reach and cheap
to believe.

---

## 8. Explicit non-claims

- No claim any candidate IS native, IS per-frame, or HAS been hooked: C-2/C-3/C-4 are
  open (§7.2). No claim F-1 is closed true or false.
- No claim the flags-immediate extraction failed permanently: a real disassembler pass
  (capstone over the ctor functions, target the UFunction-ctor call argument) is the
  named future offline work and may still resolve FUNC_Native image-side.
- No claim about the exact owning UClass for `"Tick"` or the Receive* family beyond
  engine-knowledge confidence labels (AActor/UAnimInstance); the census is the decider.
- Nothing executed in-game, nothing staged, no console contact; the oracle ran read-only
  against the sha-verified pinned image inside this workspace.

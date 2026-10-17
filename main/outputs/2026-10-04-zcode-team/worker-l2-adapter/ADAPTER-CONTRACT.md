# ADAPTER-CONTRACT — the L2 ENGINE-ADAPTER boundary, formalized

Worker: `worker-l2-adapter` · 2026-10-05 · LOCAL ONLY (source + receipt reads; no console, no
execution, no canonical edits). Machine-readable twin: `ADAPTER-CONTRACT.json` in this directory.

**What this is.** Today the interface between the game profile and the compatibility runtime
exists only implicitly in the composition. This document makes it an explicit CONTRACT — the
foundation for the adapter system, where a second game/engine reuses L1 unchanged, swaps L3,
and implements exactly this boundary.

**Layer model**

| layer | what | today's instantiation |
|---|---|---|
| L1 | compatibility runtime (UE4SS-ps5 port + Lua VM + composition/packaging) | the 368-unit composed runtime, PRX candidates |
| **L2** | **ENGINE-ADAPTER — this contract** | the seams named below, spread across bridge/initializer/backend/luamod/program units |
| L3 | game profile (per title + title-update data) | `worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json` + v3 staged package + `SEED-TABLE.json` |
| L4 | game mod (consumer, not contract party) | TownfallItemSpawner `main.lua` + `UEHelpers.lua` |

**Status vocabulary** (each element carries one, with provenance):

- **DELIVERED-VERIFIED** — implemented AND executed in the real game process with recorded evidence.
- **DELIVERED-HOST-ONLY** — implemented, host-compiled/host-tested/composed into a candidate, never executed in-game.
- **IN-FLIGHT** — a named worker/edition is converging it with a defined next gate.
- **GAP** — not implemented, proven not composable, or structurally impossible in the current candidate.

---

## 1. The contract at a glance

```
        L3 (game profile)                          L2 (engine adapter)                         L1 (runtime)
  title id + activation declaration  ──IN-ACT──▶  activated mod source (logical keys)   ──▶  discovery + Lua VM
  seed slots {0,2,3,4,6,10} +        ──IN-SEED──▶ arming gate + validators +           ──▶  consumers (GUObjectArray,
    validation classes                              loud refusal naming                       FName ctor, GMalloc, SCO, Tick)
  settings keys (13 + hook defaults) ──IN-SET──▶  admission + native-services gate   ──▶  parse + hard throws
  PS5Pad.ini + provider              ──IN-PAD──▶   ConfigureRetainedPS5PadInput       ──▶  Handler keybind dispatch
  capture + geometry + extent        ──IN-POL──▶   vmmap walk / frozen snapshot /     ──▶  safe reads (leases), spanpub
                                                   lease registry
                                                    ─────────────────────────────────
  ◀── OUT: Lua API (14-element load-time surface + dynamic reflection)            ◀── L4 mods
  ◀── OUT: internal APIs (tick pump, ExecuteInGameThread enqueue/drain, mod chain, layout inis)
  ◀── OUT: diagnostics (runtime-status.txt states, UE4SS.log + mirror, diag_event + JSONL, layout gate, t= stamps)
  ◀── OUT: lifecycle (module_start → activation → engine-ready → mods → tick loop → stop/failed)
  ◀── OUT: failure taxonomy (every named refusal; loud, terminal, replay-forbidden)
```

---

## 2. INPUT surface — what L3 must supply

### 2.1 Activation (`IN-ACT`, all DELIVERED-VERIFIED — consumed in-game, launch-9)

| id | element | requirement / refusal |
|---|---|---|
| IN-ACT-1 | title match | exact expected title (e.g. PPSA33286); refusal `UE4SS activation must match the supplied game title` |
| IN-ACT-2 | framework request | engine `unreal` + framework `ue4ss-ps5` in the loadlist; refusal `Activation does not request the native UE4SS framework` |
| IN-ACT-3 | file role filter | only `lua_mod`/`config`/`script` roles; refusal `Unsupported UE4SS activation file role: <path>` |
| IN-ACT-4 | preload requirement | every startup file preloaded (0-byte `enabled.txt` needs a non-null pointer); refusal `UE4SS file was not preloaded: <path>` |
| IN-ACT-5 | logical key layout | `ue4ss/…` keys (settings, layouts, PS5Pad.ini, Mods/<Name>/Scripts/main.lua, enabled.txt, shared/<Name>/<Name>.lua); no filesystem scan exists |

### 2.2 Seed slots (`IN-SEED`)

Policy: offline-derived **per title-update** against the sha-pinned capture; recorded derivation or
it does not ship; under-determined slot stays unseeded and blocks startup loudly. Shape A
(launch-10): all four REQUIRED seeds `{0,2,3,4}` must be present or startup refuses.

| id | slot | validation class | Townfall-PS5 value (linked/runtime) | status |
|---|---|---|---|---|
| IN-SEED-0 | g_uobject_array | permits_pointer_slot (readable+8-aligned via lease) + optional 96-byte header probe | 0x93ba110 / 0x97ba110 | **DELIVERED-VERIFIED** (only slot with live structural validation: layout A, 71959 objects, GC-order predicate re-validated) |
| IN-SEED-2 | fname_ctor_wchar | code address — no data-side validation; flows around `permits_code` (G4); FC-1 probe staged, not executed | 0x23d590 / 0x63d590 | DELIVERED-HOST-ONLY |
| IN-SEED-3 | gmalloc | V1 `qword@seed==instance` + V2 `qword@instance==RELRO-vtable`; arms only if both pass, else loud `GMalloc is uninitialized.` | 0x930add8 / 0x970add8 | DELIVERED-HOST-ONLY |
| IN-SEED-4 | static_construct_object_internal | code address; igf4-class 16-byte prologue check DESIGN ONLY | 0x490aa0 / 0x890aa0 | DELIVERED-HOST-ONLY |
| IN-SEED-6 | engine_version | settings route (`[EngineVersionOverride]`), removes bit 6 from required mask; S2 layout gate is the witness | 5.6 | DELIVERED-HOST-ONLY |
| IN-SEED-10 | gameengine_tick (OPTIONAL) | vtable fallback (vptr read + entry read + Zydis decode at target — the igf6 crash class); not seeded on launch-10 | 0x33ac8c0 = 0x400000+0x2fac8c0 | DELIVERED-HOST-ONLY (oracle F29 decides at launch-10) |

### 2.3 Settings keys (`IN-SET`, all DELIVERED-HOST-ONLY — parser host-verified, console ini readback-verified, runtime consumption never reached in-game)

Consumed ONLY from logical key `ue4ss/UE4SS-settings.ini`; missing/NUL = hard throw;
`ModsFolderPath`/`ControllingModsTxt` refused; unlisted keys keep code defaults.

| id | key | v3 value | admission note |
|---|---|---|---|
| IN-SET-1..4 | bUseUObjectArrayCache / SecondsToScanBeforeGivingUp / DefaultExecuteInGameThreadMethod / DefaultFNameToStringMethod | false / 120 / EngineTick / Scan | EngineTick selects the drain path every main.lua callback uses; scan bound throws `PS scan timed out` |
| IN-SET-5..7 | ConsoleEnabled / GuiConsoleEnabled+Visible / RenderMode / EnableDumping | 0 / 0 / ExternalThread / 0 | native services gate: any other value hard-throws at ctor |
| IN-SET-8 | [EngineVersionOverride] MajorVersion/MinorVersion | 5 / 6 | both set + uint32 range else throw; parser-effective bound int32 (host-measured) |
| IN-SET-9 | [Hooks] HookProcessInternal + HookProcessLocalScriptFunction | **0 / 0 (REQUIRED)** | either key left 1 keeps init-time ExecuteUbergraph jmp-resolve + 164-byte Zydis reads against game text; only consumer is the script hook, silently not installed at 0/0 |
| IN-SET-10 | hook defaults unset (HookLoadMap / HookInitGameState / HookEngineTick / EngineTickResolveMethod=Scan / bHookUObjectProcessEvent) | must stay TRUE | mod-load-critical; ProcessEvent resolves from the vtable |

### 2.4 Pad input source (`IN-PAD`, all DELIVERED-HOST-ONLY — hardware contact never run)

- **IN-PAD-1** `ue4ss/PS5Pad.ini`: read once during backend ctor; absent = no pad bridge; present +
  failing = startup throws. Parser: hard refusals with line numbers; ≤128 bindings; no nested
  chords; v3 mapping OPTIONS→F7, arrows, CROSS→RETURN (exactly the 6 keys the mod registers).
- **IN-PAD-2** `context.input_source`: Handler init throws `Input registration unavailable` without
  a source; PS5Pad.ini + supplied source conflict = throw.
- **IN-PAD-3** scePad provider: first hardware contact designed to be the standalone probe PRX
  (`padp-79947e016dfe`, frozen) — runbook staged, **no in-game result claimed**.

### 2.5 Capture / lease / spanpub policy (`IN-POL`)

| id | element | status |
|---|---|---|
| IN-POL-1 | configured roots (Resolution B): absolute/normalized/regular-file/file_size gate | DELIVERED-VERIFIED (launch-9) |
| IN-POL-2 | vmmap walk (structsize-respecting) + frozen capture snapshot; r14 registers from the FROZEN snapshot (kills the launch-8 two-snapshot race) | walk+registration VERIFIED; r14 fix HOST-ONLY (70/70) |
| IN-POL-3 | read-lease registry: true protection (R|X never W), cap 128, anchors gated on registry, uncoverable lease refuses LOUDLY before any lease exists (`code=1 lease coverage gap: requested [..] outside registered readable spans; nearest registered [..]`, no lmf events on that path) | registration ran in-game (spans=6/reg=6); hardening HOST-ONLY |
| IN-POL-4 | spanpub publication: ≤16 published non-image readable spans + truncation-honest envelope markers (`bridge_spanpub_count/total/cap/env_low/env_high`) | HOST-ONLY (as-built fc06fe52 failed packaging compile; repaired bridge 663e7ce9 carries them; prediction-pinned) |
| IN-POL-5 | extent law: runtime = [base, base+0xbc602c8); 0xfc602c8 is the known-wrong launch-8 clamp, never reuse; whole-extent reads refused | arithmetic VERIFIED; application HOST-ONLY |

---

## 3. OUTPUT surface — what L1/L2 provides

### 3.1 Lua API (the L4-facing surface; all DELIVERED-HOST-ONLY — launch-9 loaded NO mods)

Load-time surface (every global main.lua/UEHelpers.lua touch is registered — zero load blockers):

`print` · `require` (native buffer searcher, 5 candidate keys) + native `loadfile`/`dofile` ·
`FindFirstOf` / `FindAllOf` / `StaticFindObject` / `StaticConstructObject` / `CreateInvalidObject` ·
`FName` ctor + `NAME_None` + `EFindName` · `FText` · `LoadAsset` · `RegisterKeyBind` + `Key` table ·
`ExecuteInGameThread` (main-state, EngineTick method) · `RegisterLoadMapPreHook` ·
`ModRef:Get/SetSharedVariable` · `UnrealVersion{Major,Minor,IsBelow}` · `IsValid` metatable method ·
dynamic UFunction calls + property read/write (ProcessEvent machinery — registered-by-mechanism,
in-game marshalling unproven, out-param marshalling historically highest-risk).

Degraded/gapped:
- `RegisterHook`/`RegisterCustomEvent`/script hooks — **GAP by design under v3** (both hook keys 0 →
  never derived → falls through with no else and no log).
- `ExecuteInGameThread` drain — source-complete on the readpath-guard lineage; **GAP on the
  r3-armed lineage** (pump has zero consumers in the composed object).

### 3.2 Internal APIs

| element | role | status |
|---|---|---|
| RegisterEngineTickPreCallback(HookedEngineTick) | first genuine tick publishes NativeGameThreadIdentity then RemoveSelf — a DRAIN PREREQUISITE | HOST-ONLY |
| RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch) | persistent per-tick witness | HOST-ONLY |
| engine_tick_hook (lazy at first ExecuteInGameThread) | IsInGameThreadRaw gate → process_simple_actions → lua_pcall; needs NO ProcessEvent | HOST-ONLY |
| QueryHarness::InstallDefaultEngineBindings() | binds 4 engine seams once before the init thread; host builds refuse the default binding | HOST-ONLY |
| R3 FuncPtr-marshal pump + enqueue-first ladder | marshal-first arm; enqueue before arming; loud stall log on refused arm | IN-FLIGHT (host-tested 23/23, staged, not composed) |
| enqueue (calling thread) / drain (game thread) | queue under mutex; enqueue touches nothing engine-side; partial stall impossible (one shared queue) | HOST-ONLY |
| mod loading chain | MakeActivatedUE4SSModSource → NativeModSource discovery (main.lua key, shared-skip, enabled.txt/mods.txt) → backend ctor (pad, Deferred, start_native_mods) → event loop on_program_start → start_mods<LuaMod> → start_mod → load_and_execute_script | VERIFIED activation / HOST-ONLY the rest |
| layout ini consumption | MemberVariableLayout.ini + VTableLayout.ini from logical keys → member offsets + VTableLayoutMap; identity-agreement refusal on version-pin mismatch | HOST-ONLY |

### 3.3 Diagnostics channel

| channel | role | status |
|---|---|---|
| runtime-status.txt | the durable lifecycle ack channel (see §4); writer = injected runtime only, during module_start; descriptor kept open all session | **DELIVERED-VERIFIED** (launch-9 sessions 234/273 written and read back) |
| UE4SS.log + append-only mirror | `Output::send` sink; Lua print prefixed `[Lua]`; hosts the mod-executed oracle line | **DELIVERED-VERIFIED** (launch-9 mirror artifact) |
| diagnostic_event + JSONL sink | legacy entry channel (byte-frozen) + additive `modbridge.supplier-diag/v1` lines to `/data/modbridge-supplier-diag.jsonl` (tolerant IO, 64-attempt cap, seq gap = dropped write, never fatal) | HOST-ONLY (23/23 scenarios + compile-gate rehearsal; staged) |
| QueryHarness layout gate + tick witness | `ArmLayoutGate({Major,Minor,NonShipping,DebugBuild,…})`; per-tick dispatch | HOST-ONLY |
| t= prefixed lines + PATH_TEST stamps | timing for free, no separate diagnostic launch | **DELIVERED-VERIFIED** |

---

## 4. LIFECYCLE

State channel: `activated → engine-ready → mods-started → (failed|stopped)` in runtime-status.txt;
`running` ⟺ last complete state is `mods-started`; new session replaces the file; crash mid-line
ignored by readers. **`mods-started` certifies INVENTORY, not script execution** — the
`[Lua] Loaded` line is the mod-executed oracle.

| seq | stage | key steps | status |
|---|---|---|---|
| 1 | module_start / activation | mfa_activate → RuntimeAckConfig (pid, monotonic ms, status_root) → InstallDefaultEngineBindings → MakeActivatedUE4SSModSource → RuntimeAckSink open → `activated` | VERIFIED |
| 2 | RuntimeOwner start | serialized init thread; Prepared→Starting→EngineInitialized; double-start impossible; self-join forbidden | VERIFIED (machine exercised through failed paths in-game) |
| 3 | engine bootstrap | seed arming gate (SHAPE A) → consume seeds (read-path-guarded binds) → GMalloc V1/V2 → ps_scan skipped → consumer assignments → LoadMap walk → Tick vtable fallback → pump arms → PostInitialize → InitializeVersionedContainer → PublishStagedNativeLayoutMaps + ArmLayoutGate | HOST-ONLY (never reached in-game; launch-9 refusals were upstream) |
| 4 | engine-ready + mods-started ack | real on_engine_initialized → engine_ready; + mods_started iff ctor inventory succeeded | HOST-ONLY (states never observed in-game yet) |
| 5 | event loop + Lua mods | ~5ms jthread loop → on_program_start → install/start Lua mods → start_mod → main.lua via native read + pcall → `[Lua] Loaded` | HOST-ONLY (a synthetic Lua VM ran in-game in the launch-9 entry harness — first fully-green in-game Lua — but not the mod chain) |
| 6 | tick loop + input | real GameEngine::Tick through detour → identity publish → harness witness → drain; pad polled ~5ms; keybind callbacks on UpdateThread defer bodies to game thread | HOST-ONLY (decided by launch-10 F29/FLAG-1) |
| 7 | stop / failure | request_stop → shutdown → join → `stopped`; any throw → `failed(code, sanitized detail)` → rethrow preserved | VERIFIED (failed paths in-game) |

Deadlines: StartupWait 5 min (`Unreal initialization cancelled` / `startup deadline exceeded`),
`SecondsToScanBeforeGivingUp` (`PS scan timed out`). Everything else unbounded, single-pass.

---

## 5. FAILURE TAXONOMY — every named refusal (loud, terminal, replay-forbidden)

Eleven groups; full itemized list with file:line in `ADAPTER-CONTRACT.json` →
`failure_taxonomy.runtime_owned_named_refusals`. Summary:

1. **activation** — title/framework/role/preload refusals (AMS:23–41).
2. **context/roots** — ValidateNativeProgramContext, context-already-has-mod-source, `init returned
   without a stop request`, construction/inventory rejected.
3. **input** — PS5Pad.ini parser refusals with line numbers; provider factory/availability; `Input
   registration unavailable`.
4. **settings** — missing/NUL hard throw; Overrides refused; native-services gate; EngineVersionOverride range.
5. **seeds/validation** — `[Seeds] Required-slot seed table incomplete` (SHAPE A), `[Seeds] REFUTED: …
   Terminal for this launch; derive afresh with new evidence, never replay.`, GMalloc V1/V2 →
   `GMalloc is uninitialized.`, `PS scan timed out`, FC-1 fail-closed.
6. **bridge/lease** — `code=1 lease coverage gap …` (pre-conversion, no lmf events), `code=2 Owned
   read range includes unregistered bytes` (launch-8 in-game; r14 makes recurrence structurally
   impossible = REGRESSION if seen), `no observed mapping interval intersects the module image
   extent with valid geometry` (launch-9 terminal), `Mapped text base must be registered readable
   executable bytes`, lease/anchor/spanpub diag tags.
7. **initializer detours** — FName verification, game-thread callback registration (the FLAG-1
   exec-write class), harness tick dispatch, required-object `Default__*` failures, the
   intentionally-EMPTY required-objects throw (observer trap), StartupWait, `GetGameThreadId called
   too early`.
8. **walkguard fold-0** — W1/W2 expected loud refusal (no read, zero syscalls; W2 = the pump-arm
   terminal, `guard_off_no_rx_verdict`, status F30 shape); W3–W7 regression-only reasons
   (seeing any = composition identity drift).
9. **r3-armed variant (not in the launch-10 candidate)** — `[R3] refused …` + deferral with zero
   consumers → OPTIONS totally silent forever; expected on that variant, not new info.
10. **lua/execute layer** — `EngineTick hook is not available (AOB scan failed)` at first press,
    enqueue-first loss throw (LuaMod.cpp:4807; staged fix), mod load error shapes
    (`Mod is absent from native Lua source`, `Error loading/executing script`,
    `Failed to execute main script`), require unresolvable (5 candidate keys).
11. **mod-level (L4-facing)** — FAILURE-TAXONOMY.md §A–F: load-time, menu/scan, spawn stages
    `player / ground trace / load pickup class / spawn actor / configure pickup`, silent no-ops,
    runtime-level drain/input failures, controller decision shortcut. The mod never crashes the
    game and never dies itself (pcall discipline; `busy` resets; invalid partial actor destroyed).

---

## 6. COMPLETENESS DASHBOARD

| status | elements |
|---|---|
| **DELIVERED-VERIFIED** | activation mechanics (IN-ACT-1..5) · slot 0 address (in-game validated) · configured roots · vmmap walk + capture · lease registration (launch-9 shape 6/6) · activation consumption · runtime-status channel · UE4SS.log mirror · t= stamps · lifecycle seq 1–2 and failed paths · in-game synthetic Lua VM (entry harness) |
| **DELIVERED-HOST-ONLY** | seeds 2/3/4/10 + validators · settings consumption · pad chain · spanpub · extent application · Lua API registrations · tick pump + engine seams · enqueue/drain · mod chain + layout inis · JSONL sink · layout gate · lifecycle seq 3–6 · r14 lease hardening · walkguard fold-0 shapes · detour refusal strings · engine-ready/mods-started/stopped ack emission |
| **IN-FLIGHT** | launch10-final-b build (gate not green; v4 fold pending `worker-fname-validator-v4`) · L11 luamod variant-B (admission GO, packaging unproven) · L11 enqueue-ladder fix (host 23/23, staged, not composed) · L11 marshal integration · IGW row-0 probe (armed) · pad hardware probe runbook (not run) |
| **GAP** | spawn capability end-to-end (candidate LOAD-CAPABLE, NOT SPAWN-CAPABLE by construction) · marshal pump consumer in a composed object (proven not composable through frozen composition-6) · ProcessEvent functional marshaling incl. out-params · script-hook subsystem under v3 (silent by design) · PC-Townfall L3 profile (placeholder only) · any second-game profile |

---

## 7. SECOND-GAME REUSE ANALYSIS

Context: the first "second game" is already staged — **PC Townfall** (`inputs/pc-townfall`):
same title, different platform/build. A different UE game exercises the same boundary.

### 7.1 UE-generic — reusable as-is by the adapter system (L2 owns)

- Activation mechanics minus the literal identity strings (framework/role filter, preload
  requirement, logical-key layout).
- The seed-slot **mechanism**: SHAPE-A arming gate, `[Seeds] REFUTED` naming, read-path-guarded
  bind classes, validator admission classes (pointer-slot / code / FC-1-class / V1V2-chain).
- Settings schema + admission semantics (native-services gate, hook defaults, version override).
- The **Lua API surface** (all registrations + the dynamic reflection machinery).
- Lifecycle states + runtime-status.txt format + ack ordering.
- Tick-pump contract shape (registration order, first-tick publication prerequisite, drain gate,
  single shared queue) and ExecuteInGameThread enqueue/drain semantics (enqueue-first, loud stalls).
- Mod loading chain + diagnostics channels + failure-naming discipline (a second game must
  reproduce the named refusal shapes for the acceptance oracle to port).
- Capture/lease/spanpub **policy** (only the geometry numbers change).

### 7.2 Game-specific — must come from the new L3

Title id + expected_title · the captured executable + build manifest · build_id · PT_LOAD geometry ·
image base/slide (0x400000 is PS5-Townfall-proven; PC differs) · extent constant · **seed values
{0,2,3,4} + their derivations** (anchor strings, xref censuses, prologue bytes — per title-update,
never transferred) · engine version pin + profile axes · **layout INI contents** (identity header
must agree with the version pin or the binder refuses) · loadlist/mod set · pad binding profile ·
optional slot-10 address identity · spawn-rung addresses (FuncPtr marshal target, IGW route) ·
L4-facing oracle rows parameterized by the mod's strings.

### 7.3 Engine-version-specific — L2-internal conditionals (enumerated)

Driver: `Unreal::Version::Major/Minor` provisioned by the settings route or scan. Census of the
consumed port tree (2026-10-05): **~317 `IsBelow`/`IsAtLeast` sites, ~40 distinct predicate pairs
spanning 4.8–5.8**. Top pairs: `IsBelow(4,25)`×26, `IsAtLeast(5,1)`×25, `IsAtLeast(4,27)`×21,
`IsBelow(5,1)`×18, `IsAtLeast(4,25)`×18, `IsBelow(5,7)/(5,0)/(4,9)/(4,15)`×9 each.

Named families:
- **FField vs UProperty world**: `IsBelow(4, 25)` gates (FField.cpp — 19+ sites).
- **Script-hook route split**: `ProcessLocalScriptFunctionInternal.is_ready()` (≥4.22) ||
  `ProcessInternalInternal.is_ready()` (<4.22) at LuaMod.cpp:7682 — the silently-degraded subsystem.
- **UDynamicClass** required-object: `IsAtLeast(4,11) && IsBelow(5,4)` (UnrealInitializer.cpp:1110).
- **Noexport outering**: `IsBelow(4,8)` required-object path shape; `IsAtLeast(4,12)`
  MovieSceneEditorData (UI:1083–1101).
- **FAssetData**: 4.17+ hard floor; `AssetClassPath` 5.1+ (FAssetData.cpp:14,258,447).
- **FFrame** 5.1 branch (FFrame.cpp:14).
- **Versioned container publication**: `InitializeVersionedContainer` (UnrealInitializer.cpp:290) →
  `PublishStagedNativeLayoutMaps(Major, Minor, NonShipping, DebugBuild)` + `ArmLayoutGate` (UI:1281–1291)
  — the S2 layout gate consumes exactly the provisioned values.
- **Layout identity agreement**: MemberVariableLayout.ini/VTableLayout.ini identity headers
  (EngineMajor=5, EngineMinor=6, NonShipping=0, Stats=0, `EngineRevision=5.6.1-0+++UE5`,
  `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`) — mismatch fires the binder's refusal.
- **Layout-width coverage**: 419 canonical accessor entries, 304 available after the
  `UFunction.Func` 16-byte delta — the per-version accessor availability table.
- **UnrealVersion Lua exposure** so mods branch themselves (UEHelpers.lua:198,200).
- **FName routes**: ctor + name-pool resolution and FName→string methods are version-sensitive.

Platform conditionals (orthogonal — swap with the PLATFORM, not the game): **249 `__PROSPERO__`**
sites (223 in the port across 25 files + 26 in the notify-cancel luamod unit): settings from
logical keys with hard throws, no filesystem mod scan, no GUI console/dumper/crash-dump, VTable
disk-read compiled out; plus the PS5 substrate itself (sysctl vmmap walk, frozen capture, lease
registry, mprotect/FLAG-1 class, WalkRxGuard, scePad provider, PRX packaging + 191-import frozen
contract, prospero clang, mfa activation). A PC port swaps these, not the contract.

### 7.4 Porting sequence sketch

1. **L3**: capture the new executable (+ manifest) → derive geometry → derive seeds {0,2,3,4}
   offline against the sha-pinned capture with recorded derivations → pin engine version + emit
   layout INIs → write pad profile + loadlist → assemble the activation bundle.
2. **L2 platform check**: platform unchanged → substrate reused as-is; platform changed (PS5→PC) →
   the `__PROSPERO__`-gated units + provider substrate need platform counterparts (out of current scope).
3. **L1 unchanged**: same composed runtime, lifecycle, Lua API, diagnostics; acceptance
   re-parameterized by the new L3 strings + seed log shapes.
4. **Never reuse**: seed constants, geometry numbers, title identity, layout ini contents. Every
   one of those transferring is the silent-wrong-address failure class the loud-refusal design
   exists to prevent.

---

## 8. Honesty notes

- No element is marked DELIVERED-VERIFIED beyond its cited evidence; **launch-9 loaded NO mods** —
  every Lua-API/mod-chain element is at best host-verified.
- The seeds→engine-ready→mods chain has never executed in-game: launch-9's two refusals were
  upstream (lease coverage, facts-geometry), both root-caused and fixed host-side, pending the
  launch-10 rebuild (gate not green; no determinism proof).
- spanpub markers and walkguard rows W1–W7 are prediction-pinned oracle shapes, not observed behavior.
- The predicate/platform census is a static grep of the consumed source trees (2026-10-05); line
  pins shift with re-staging (e.g. the +69-line initializer shift in launch10-final-b).

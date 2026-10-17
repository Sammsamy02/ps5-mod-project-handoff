# RUNTIME-STAGING-PLAN.md — post-promotion staging of the composed UE4SS module (Townfall / PPSA33286)

Worker: worker-post-promotion-runtime, 2026-10-04. Workspace root for all relative
paths: `/WORKSPACE/.codex/worktrees/translation-core/what`. No console contact,
no network, no nested subagents. All code citations below were read from the retained
composed-runtime sources (port tree
`.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/port/ue4ss/`,
bootstrap `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/`, lifecycle
`outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/`). Every sha256/byte size in
§4 was recomputed on disk by this worker on 2026-10-04.

Honest status line: this is a staging plan, not a performed staging. Gameplay remains a
prototype; nothing here claims the runtime has run on the console. Two wiring facts are
measured, not assumed, and bound what evidence each launch can produce (see §6 and §7):

- composition-3's linked object DEFINES `PrepareActivatedUE4SSRuntime` but its symbol
  table does NOT contain `MakeRuntimeAckConfig`/`StartActivatedRuntimeWithAck`
  (grep of `worker-query-s2-arm/probe/target-probe-composition-3/nm-composed-defined.stdout`:
  0 matches) — the module_start-side ack supplier is a composition-4 extra unit
  (`worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.hpp` says so
  itself). Until the STEP-6 recomposition (worker-integration-checklist), the PRX's
  module_start path is the supplier/facts path; the full activated-start + ack-emission
  chain requires that recomposition.
- The staged pair is emitted (LIVE-EVIDENCE-EMITTED, parsepair OK) with code-anchor
  confirmation explicitly PENDING and non-blocking per its task; the pair's own receipt
  says "stageable YES once the pending code-anchor byte confirmation lands". Re-verify
  hashes at execution time; worker-emit-final-pair was in flight when this plan froze them.

---

## 1. How the composed runtime finds its module, working directory, mod source and activation files

Trace, in execution order, with exact anchors:

1. **`module_start`** (`lifecycle/module_entry.c:48`): entry_guard makes re-entry
   impossible; opens the diagnostic channel; emits `module_start_entered`, stack-alignment
   and `constructor_count` events, then `probe_lifecycle_start(...)` runs all init-array
   constructors and starts the probe worker. The diagnostic sink writes the profile-pinned
   module log (supplier-composition line: `/data/townfall_supplier_composition.log`,
   pinned by `worker-prx-packaging/package_prx.py:666`).
2. **Live module facts**: the supplier adapters composed as extra units
   (`lmf-live-module-facts`, `lmf-sce-ops`, `lmf-loader-adapter`,
   `worker-supplier-ingame-update/src/`) observe the retained module set.
   `sceKernelGetModuleInfo` is the single SDK import composition-3 ADDED (measured:
   undefined delta added `[sceKernelGetModuleInfo]` only — worker-composition-3 REPORT §0);
   `live_module_os_ops_sce.cpp:93` binds it locally. `lmf_to_native_loader_facts.cpp`
   converts supplier facts and takes `module_file_path` as a CALLER-BOUND argument
   (`TryMakeNativeLoaderFactsFromSupplier(supplier, module_file_path, options)`, :305) —
   it must be the absolute normalized path of THIS PRX.
3. **Context construction**: `RC::Platform::MakeNativeProgramContext(NativeLoaderFacts,
   serialized_elf)` (`.superpowers/.../ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp`)
   builds `NativeProgramContext{module_file_path, game_executable_path, module_source
   = ModuleSnapshot, scanner_source, input_source}` and validates it
   (`include/Platform/NativeProgramContext.hpp:85-101`): both paths absolute, normalized,
   filename-bearing, NUL-free, and must name EXISTING regular files; `module_source`
   (owning loader snapshot) is mandatory.
4. **Activation**: the game-bound entry calls `mfa_activate` (vendored
   `modforge_activation.c/h`, `worker-f-r2-fix/composition/vendored/`) ONCE, during
   module_start — "that is the only moment /data is readable from the game process (a
   fresh open afterwards fails with ENOENT)" (modforge_activation.h:4-6). It reads and
   fully validates `<root>/loadlist.txt` (title must match PPSA33286), preloads every
   file whose role it consumes (default roles: `lua_mod`, `config`, `script`) into owned
   memory, checks each against the loadlist-recorded SIZE, lists everything else by
   path/size, and returns one immutable `mfa_activation` for the process lifetime.
   All-or-nothing: any failure releases everything and the game runs unmodded.
5. **Activated mod source**: `PrepareActivatedUE4SSRuntime(context, activation,
   expected_title, activation_root, runtime_ack)` (`bootstrap/ue4ss_backend.cpp:84-123`)
   rejects a context that already has a mod source, validates the context, then sets
   `context.mod_source = MakeActivatedUE4SSModSource(activation, expected_title,
   activation_root)` (`bootstrap/activated_mod_source.cpp:20-84`): title must match,
   engine must be `unreal`, framework must include `ue4ss-ps5`; exactly the activation
   files whose path starts with `ue4ss/` and whose role is `lua_mod`/`config`/`script`
   are forwarded as immutable logical keys; native plugins must sit directly under a
   mod's `dlls/` (none staged — Lua-only first run). It then opens the RuntimeAckSink on
   `<status_root>/runtime-status.txt` (ack point 1) and builds the backend with buffered
   mods: `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)`
   reads logical key `ue4ss/PS5Pad.ini` (`ue4ss-native-pad-config-1/src/NativePadInput.cpp:13`),
   `start_native_mods()` builds the Lua inventory eagerly.
6. **`UE4SSProgram` construction on `__PROSPERO__`** (`src/UE4SSProgram.cpp:213-218`):
   validates the context again, then `setup_paths(m_native_context.module_file_path)`
   (:749-785):
   - `m_root_directory = moduleFilePath.parent_path()` (:752)
   - `m_working_directory = m_root_directory` (:758, :770)
   - WITH a mod source the function returns EARLY (:776-785): `m_log_directory =
     m_working_directory`; `m_settings_path_and_file` gets `UE4SS-settings.ini` appended
     for DIAGNOSTICS ONLY — "Configuration is selected exclusively from retained logical
     ue4ss/ keys." The game-specific-config directory scan and all legacy fallbacks are
     skipped. **So `m_working_directory` == the directory that contains the PRX.**
7. **Settings**: `settings_manager.deserialize(...)` consumes logical key
   `ue4ss/UE4SS-settings.ini` from the mod source (`SettingsManager.cpp:60-68`; missing
   or NUL-containing throws). `validate_native_services` (:719-746) then throws on
   `CrashDump.EnableDumping`, `Debug.SimpleConsoleEnabled`, `Debug.DebugConsoleEnabled`,
   `Debug.DebugConsoleVisible` true or `RenderMode != ExternalThread` — the staged ini
   (§4) passes all five gates (host-verified by worker-ue4ss-settings).
8. **Log device**: `Output::set_default_devices<NewFileDevice>` opens
   `m_log_directory / "UE4SS.log"` (:255-257) — **the runtime writes UE4SS.log NEXT TO
   THE PRX**, i.e. `/data/plugins/UE4SS.log`. The PRX directory must be writable or the
   startup fails at log open (worker-ue4ss-settings flags this as a caller decision).
9. **init() -> setup_unreal() -> LAYOUT GATE**: `load_unreal_offsets_from_file()`
   (:894-908): with a mod source it reads BOTH keys `ue4ss/MemberVariableLayout.ini` and
   `ue4ss/VTableLayout.ini` from the mod source and calls
   `Platform::NativeLayouts::StageNativeLayoutTexts(members->bytes, virtuals->bytes)`.
   **If either key is absent it throws "Native activation requires both member and
   vtable layout files".** This runs inside `setup_unreal()` BEFORE any Lua mod work.
   The staged maps publish to the engine only later, via
   `NativeProvidedVirtualVC::set_virtual_offsets()` ->
   `PublishStagedNativeLayoutMaps(major, minor, non_shipping, stats)`, which THROWS on
   staged-vs-resolved identity/axis disagreement (worker-query-harness-integration
   S2-ARM.md §1) — the pair's identity must equal the engine's own resolution
   (5 / 6 / non_shipping=0 / stats=0 as recorded in the emitted pair).
10. **Mod discovery** (`setup_mod_directory_path`, :832-839): with a mod source the mods
    directory is fixed to the LOGICAL `ue4ss/Mods`; `UE4SS_MODS_PATHS` is refused.
    Mods enumerate from mod-source keys only: `ue4ss/Mods/<Mod>/enabled.txt` presence
    enables; `ue4ss/Mods/<Mod>/Scripts/main.lua` is the entry;
    `ue4ss/Mods/shared/<n>/<n>.lua` is the require() fallback; optional
    `ue4ss/Mods/mods.txt` (not staged — enabled.txt suffices) and optional
    `ue4ss/UE4SS_Signatures/*` (not staged — the real engine scan runs, no Lua scan
    override). Limits enforced by `NativeModSource`: <=4096 files, <=8 MiB/file,
    <=64 MiB total, key hygiene rules (`NativeModSource.cpp:10-75`).
11. **Engine-ready**: on `__PROSPERO__`, `init()` fires `on_engine_initialized` after
    Unreal setup, property setup and Lua sharing, only if shutdown was not requested
    (UE4SSProgram.cpp:654-661, via the backend chain ue4ss_backend.cpp:33-46); the ack
    sink emits `engine-ready` then `mods-started` (ack points 2+3) only when the
    deferred mod startup had already completed.

**Answer to "what does the PRX need in its directory?"** Only three things: the PRX file
itself; writability of the directory (UE4SS.log is created there); and a `ploader.ini`
entry that loads it. NOTHING else is read from the PRX directory on the native path —
every `ue4ss/` byte (settings, layout pair, pad config, Lua mod) comes from the
activation root through the retained mod source.

## 2. Console directory structure (exact expected layout)

ploader loads PRX modules from `/data/plugins`; mode-2 entry gives
`module_file_path = /data/plugins/<module>.prx`, hence `m_working_directory =
/data/plugins` and `m_root_directory = /data/plugins`. The activation root is
installer-managed under `/data/modbridge/roots/` (installer verbs use
`--state /data/modbridge/state --roots /data/modbridge/roots`,
worker-ue4ss-settings/STAGING-COMMANDS.md Route A2). The ack status file lives in the
EXACT `mfa_config::root` string the game-bound entry passes
(`runtime_ack_supplier.hpp`: "status_root MUST be the exact mfa_config::root string …
Never the activation_root" — for the install layout this is the installed root
`/data/modbridge/roots/PPSA33286`; if a controller generation hands a different exact
root string, the status file follows THAT string).

```
/data/plugins/                                          <- PRX dir == m_working_directory
  ploader.ini                                           [PPSA33286] -> <module>.prx = 2
  modbridge_supplier_composition.prx                    the composition-3 candidate (30756791 B)
  UE4SS.log                                             CREATED BY THE RUNTIME at startup (must be writable)
/data/townfall_supplier_composition.log                 module diagnostic log (profile-pinned; keep FRESH per launch)
/data/modbridge/state/                                  installer records (installer-owned)
/data/modbridge/intake/                                 installer intake (installer-owned)
/data/modbridge/roots/PPSA33286/                        <- mfa_activate root (installer-committed generation)
  loadlist.txt                                          header + title/engine/framework/mod + file rows
  runtime-status.txt                                    CREATED BY THE RUNTIME (ack emissions; root must be writable)
  ue4ss/UE4SS-settings.ini                              key 1  (779 B, corrected canonical)
  ue4ss/MemberVariableLayout.ini                        key 2  CRITICAL PREREQUISITE (588 B, emitted pair)
  ue4ss/VTableLayout.ini                                key 3  CRITICAL PREREQUISITE (186 B, emitted pair)
  ue4ss/PS5Pad.ini                                      pad binding profile (2960 B)
  ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua       key 5  (21415 B)
  ue4ss/Mods/TownfallItemSpawner/enabled.txt            key 6  (0 B presence = enabled)
  ue4ss/Mods/shared/UEHelpers/UEHelpers.lua             key 7  require() fallback (9962 B)
```

Deliberately NOT staged: `ue4ss/Mods/mods.txt`, `ue4ss/UE4SS_Signatures/*`, any `dlls/`,
any `ue4ss/` file under `/data/plugins` (the native path never reads the PRX directory
for configuration). Ownership rule stands: only `/data/modbridge/**` is ever written by
the installer route; `/data/plugins` writes are the ploader-staging route with backup +
hash-verified pre-image (worker-integration-checklist STEP 8).

## 3. ploader.ini update (which entry, what mode)

Console file `/data/plugins/ploader.ini`, section = title id, one line per module:

```ini
[PPSA33286]
/data/plugins/modbridge_supplier_composition.prx = 2
```

- Mode `2` = LATE load (the only mode shape evidenced for this line;
  worker-prx-packaging/STAGING-CHECKLIST.md §2, precedent: the frozen aligned-entry
  module used exactly this shape and the SDK stamp 0x11000000 — which the composition-3
  candidate also carries — is what turned the earlier ESDKVERSION 0x80020063 load
  failure into a real OK).
- Per-launch re-send: mode-2 payload entries must be RE-SENT for each game launch; a
  module sent for launch N is not present for launch N+1 without re-send.
- Never overwrite the frozen `modbridge_runtime_aligned_entry.prx` or any prior module;
  reuse of the name `modbridge_supplier_composition.prx` from a prior generation is
  allowed ONLY after hash-verifying the pre-image and backing it up
  (`/data/plugins/<existing>.<tag>.bak`).
- Never improvise ini edits beyond the documented shape.

## 4. File placement (every file: local path, destination, sha256, bytes)

All hashes/sizes recomputed on disk by this worker. Console column assumes the §2 layout.

| # | console destination | local file (workspace root = `/WORKSPACE/.codex/worktrees/translation-core/what`) | bytes | sha256 |
|---|---|---|---|---|
| 1 | `/data/plugins/modbridge_supplier_composition.prx` | `outputs/2026-10-04-zcode-team/worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx` | 30756791 | `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35` |
| 2 | `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini` | `outputs/2026-10-04-zcode-team/worker-ue4ss-settings/staged/ue4ss/UE4SS-settings.ini` | 779 | `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` |
| 3 | `/data/modbridge/roots/PPSA33286/ue4ss/MemberVariableLayout.ini` | `outputs/2026-10-04-zcode-team/worker-emit-final-pair/emission-final/MemberVariableLayout.ini` | 588 | `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32` |
| 4 | `/data/modbridge/roots/PPSA33286/ue4ss/VTableLayout.ini` | `outputs/2026-10-04-zcode-team/worker-emit-final-pair/emission-final/VTableLayout.ini` | 186 | `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86` |
| 5 | `/data/modbridge/roots/PPSA33286/ue4ss/PS5Pad.ini` | `outputs/2026-10-04-zcode-team/worker-real-input-pad/PS5Pad.ini` | 2960 | `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e` |
| 6 | `/data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | `outputs/2026-10-04-zcode-team/worker-ue4ss-settings/staged/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` |
| 7 | `/data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | `outputs/2026-10-04-zcode-team/worker-ue4ss-settings/staged/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| 8 | `/data/modbridge/roots/PPSA33286/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | `outputs/2026-10-04-zcode-team/worker-ue4ss-settings/staged/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` |

Identity embedded in files 3+4 (must survive readback unchanged):
`EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`,
`EngineMajor=5`, `EngineMinor=6`, `NonShipping=0`, `Stats=0`,
`Target=prospero-x86_64-sysv`, `PointerSize=8`. Sections: FUObjectArray (10 members,
`UEP_TotalSize=0xB8`) + TUObjectArray (7 members, `UEP_TotalSize=0x20`) — rung 1->2 ONLY.

### Required loadlist rows (the installer commits loadlist.txt; sizes are commit truth)

The existing `worker-itemspawner-bundle/bundle/loadlist.txt` is NOT stageable as-is:

- its settings row is `file config ue4ss/UE4SS-settings.ini 221` — the bundle carries the
  OLD 221-B ini (sha `f6d06cad…`), which worker-ue4ss-settings proved hard-fails
  `validate_native_services` (crash-dump/simple-console/debug-GUI throws). It MUST be
  replaced by the corrected 779-B canonical content (file 2 above) and the row size
  updated to 779; mfa verifies each staged file against the loadlist-recorded size and
  refuses all-or-nothing on mismatch (MFA_E_SIZE).
- it lacks rows for the layout pair and the pad config. Required final `file` rows:

```
file	config	ue4ss/UE4SS-settings.ini	779
file	config	ue4ss/MemberVariableLayout.ini	588
file	config	ue4ss/VTableLayout.ini	186
file	config	ue4ss/PS5Pad.ini	2960
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962
```

Header rows stay: `modforge-loadlist 1` / `title PPSA33286` / `engine unreal` /
`framework ue4ss-ps5` / `mod TownfallItemSpawner 1.0.0`. Roles `config`/`lua_mod` are
exactly mfa's default preload roles, and exactly the roles
`MakeActivatedUE4SSModSource` forwards. Totals: 35390 preloaded bytes — far below the
8 MiB/file and 64 MiB total limits. Regenerate the bundle zip with the corrected bytes
BEFORE `register-mod`/`prepare-archive`/`install` (Route A of
worker-ue4ss-settings/STAGING-COMMANDS.md; never mkdir inside `roots/` by hand; gate A0
STOR write-probe must pass first; readback-verify every file via FTP RETR + sha256).

## 5. Launch sequence (user steps; controller prepares, USER launches)

1. Verify the composition-3 candidate on the Mac: `build-receipt.json` gaps `[]`,
   20/20 static checks, sha256 == `b8bfae77…` (measured). Recomposition/packaging steps
   per worker-integration-checklist STEP 5-7 are the controller's, already done for this
   candidate (composition-3 recomposed, packaged, 0 gaps).
2. Stage the activation tree (§4 files 2-8) through the installer route; readback-hash
   every file; confirm `loadlist.txt` rows/`loadlist=<sha256>` print.
3. Stage the PRX: backup any prior module, upload file 1 to
   `/data/plugins/modbridge_supplier_composition.prx`, FTP RETR readback must hash-match
   `b8bfae77…`; ftpsrv auto-DECRYPTS FSELF, so the readback must start `7f 45 4c 46`
   (inner ELF). A returned `4f 15 3d 1d` proves the decryption oracle is broken — stop.
4. Update `/data/plugins/ploader.ini` per §3.
5. Freshness: delete/move `/data/townfall_supplier_composition.log` and note that
   `/data/plugins/UE4SS.log` and `<root>/runtime-status.txt` must not predate this
   launch. ONE launch per staged build.
6. Game fully CLOSED. User arms ploader (Payload Manager `ploader.elf` or
   `GET http://ps5-host-not-configured.invalid:8084/loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`).
   Re-send the mode-2 entry for THIS launch.
7. USER launches Townfall. Expect the ploader surface to report injection ~1 minute in.
8. Collect evidence (§6) via FTP; do not relaunch repeatedly without a changed
   implementation or a specific open question.

## 6. Expected evidence

Ordered by when it appears; collection is FTP RETR against
`ftp://ps5-host-not-configured.invalid:2121` (anonymous).

1. **Module diagnostic log** `/data/townfall_supplier_composition.log` (fresh mtime):
   the module loaded and module_start ran — `module_start_entered`, alignment events,
   `constructor_count`, constructor begin/return pairs, then the supplier/facts events.
   This is the evidence composition-3 AS LINKED produces (measured wiring state, §0).
2. **Ack emissions** `<activation root>/runtime-status.txt` (fresh; TAB-separated,
   append-only; last `state` line is current; a new session REPLACES the file):
   `modforge-runtime-status 1`, `session <pid> <monotonic ms>`, then state lines
   `activated` -> `engine-ready` -> `mods-started` on success; `failed <code> <detail>`
   otherwise; `stopped` at most once at module_stop. The sink is opened during
   module_start while `/data` is still reachable and kept open through the descriptor.
   PREREQUISITE HONESTY: these five states are emitted only from
   `PrepareActivatedUE4SSRuntime`/`RuntimeOwner`, and the module_start-side caller that
   births the ack config (`runtime_ack_supplier`) is NOT in the composition-3 object
   (§0). Absent runtime-status.txt with a healthy diagnostic log means the composition-4
   recomposition (worker-integration-checklist STEP 6) has not landed in the launched
   PRX — it is a wiring state, not by itself a runtime failure.
3. **Runtime log** `/data/plugins/UE4SS.log` (fresh) — expected startup lines on the
   native path (UE4SSProgram.cpp:414-431):
   `No specific game configuration found, using default configuration file`
   (the game-specific scan is skipped with a mod source), then
   `Config: /data/plugins/UE4SS-settings.ini`, `root directory: /data/plugins`,
   `working directory: /data/plugins`, `game executable directory: ...`,
   `game executable: ... (N bytes)`, `mods directories: [0] ue4ss/Mods` (logical key,
   not a disk path), then engine-version/scan lines ("Using engine version: 5.6") and
   per-mod startup output.
4. **Lua mod print line**: `main.lua` prefixes every log call with
   `MOD_TAG = "[TownfallItemSpawner]"` (main.lua:5-6), e.g. its startup
   `log(...)` lines appear in UE4SS.log as
   `[TownfallItemSpawner] ...` once the mod's Lua runs (after engine init + deferred
   `start_native_mods` inventory -> start on engine-ready).
5. **Negative evidence is evidence**: a `failed` ack line with detail, a missing
   `UE4SS.log` next to a healthy module log (startup died before log open), or a
   layout-gate refusal detail (§ FAILURE-TRIAGE) are all real findings. "No crash" and
   empty logs are never success; a supplier that cannot retain yields ZERO facts by
   design.

## 7. CRITICAL PREREQUISITE — the staged layout pair

The pair (files 3+4) MUST be staged in the activation (`ue4ss/MemberVariableLayout.ini`
+ `ue4ss/VTableLayout.ini`, both preloaded via loadlist roles) BEFORE the runtime
starts: `load_unreal_offsets_from_file()` reads both keys during initialization
(setup_unreal, inside init(), before ANY Lua) and throws
"Native activation requires both member and vtable layout files" when either is absent
(UE4SSProgram.cpp:899-906). Without the pair the run always ends inside setup_unreal —
no engine scan completion, no mods, no engine-ready. Placeholders are forbidden and
pointless: the embedded identity must round-trip (ParsePair verified;
stripped-BuildId refused exit 3; null identity refused exit 2 — emission-receipt.json
negative controls), and `PublishStagedNativeLayoutMaps` additionally refuses any
staged-vs-resolved axis disagreement at bind time. The pair also gates the query
harness: a failed publish leaves the gate closed and every later harness dispatch is
refused `RefusedLayoutGateClosed` and counted (S2-ARM.md §2).

Secondary hard prerequisites from the same init window, both composition-fixed but
environment-relevant: (a) an injected, available input source — `General.InputSource`
defaults to "Default" and is never parsed from the ini, so with HAS_INPUT composed in,
the retained PS5 pad provider must activate (user/pad init, open handle) and
`ue4ss/PS5Pad.ini` must parse, else "Native input service is unavailable";
(b) the corrected settings ini — the old 221-B content throws in
`validate_native_services` before the log device is even set up.

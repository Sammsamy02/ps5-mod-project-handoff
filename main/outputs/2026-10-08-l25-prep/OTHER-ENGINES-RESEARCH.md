# OTHER ENGINES — PS5 portability research for five PC modding frameworks

Agent GH, 2026-10-08. Claude round-5 Part 2 (OTHER ENGINES, research only).
Mac on battery: no builds, no code-to-tree, no console contact, no web. This
file + `agent-GH-engines.md` are the only writes.

Evidence labels used throughout:
- **[LOCAL-SOURCE]** — from files in this worktree (paths cited).
- **[TRAINING-KNOWLEDGE]** — from my training data; may be stale. Every
  version-specific fact (game list, API shape, license) is flagged and must be
  re-verified against a pinned primary source before any code depends on it.
- **[INFERENCE]** — my reasoning on top of the above.

Capabilities referenced (mission definitions,
`outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md` "CAPABILITY 1-8
STATUS"): #1 kernel reads, #2 AOB scanner, #3 code patching/detours, #4 early
loading, #5 debugger (GDB stub), #6 kernel input path, #7 file overlay, #8
asset conversion. Current stack status per that section: #2 scanner staged
(38/38), #5/#7/#8 host-proven, #1/#3/#6 gated on Phase B kstuff-lite
(`FZ-PHASEB-KSTUFF-LITE.md`), #4 suspended for eboot patching — but see the
timing note below: ploader injection already gives process-start load.

**The one fact that reframes every "#4 early loading" row:** our UE4SS runtime
loads at process start via ploader PRX injection, before engine init (proven:
startup pool admission + init run before menu on Townfall)
[LOCAL-SOURCE: PS5-MODDING-MISSION.md "THE DELIVERY STACK", l24/l26 console
verdicts]. That is the same timing class as every PC proxy-DLL loader below.
PHASE C (#4) is only about *earlier-than-injection* eboot patching
[LOCAL-SOURCE: MISSION "PHASE C — early loading (#4)"]; none of the five
frameworks needs that earlier class on PC either — they all load as DLLs at
process start. **Net: #4 is effectively solved for all five by the existing
ploader route** [INFERENCE], modulo per-game verification that module_start
lands before each engine's init point (see per-framework notes).

Self-test laws followed in section 3 for every framework (from the ueselftest
design brief + zzdiag precedent): (a) game-agnostic base-surface calls only —
no mod-specific, save-specific or menu-state-dependent probe may be a required
rung; (b) every call wrapped in pcall/error-guard, one probe = one marker line;
(c) zero side effects (read-only or register+immediately-revert only; no
behavior-changing hook may stay armed); (d) PASS = N of M core probes, with
optional discriminators excluded from the count; (e) no probe may use a
hardcoded offset — anchors come from the per-game profile (offset law,
agent-FT's ActorDumper 0x98 verdict [LOCAL-SOURCE: agent-FT-static-scan.md
item 11]); (f) crash-law: no probe that dereferences a name-resolution result
without a pcall'd IsValid-class guard first (zzdiag v2 FindFunction crash
lesson [LOCAL-SOURCE: MISSION "CONSOLE VERDICT 2026-10-08 night"]).

---

## 1. REFramework (RE Engine — praydog)

### 1a. What it is on PC [TRAINING-KNOWLEDGE unless noted]

- **Load mechanism**: proxy DLL (version.dll/dinput8.dll-class) placed next to
  the game exe; loads at process start, waits for RE Engine init, then
  initializes its SDK from the engine's own RTTI. Open source (MIT, I believe
  — verify license before vendoring) [TRAINING-KNOWLEDGE, version-sensitive].
- **Modding surface**: Lua scripting (`sdk`, `imgui`, `json`, `re` global
  tables — e.g. `sdk.get_thread_context()`, `sdk.get_native_singleton(name)`,
  `sdk.get_managed_singleton(name)`, `sdk.find_type_definition(name)`,
  `sdk.hook(...)` pre/post callbacks, `re.on_frame`/`re.on_draw_ui`), plus
  native C++ plugins and an ImGui overlay hooked on the present call
  [TRAINING-KNOWLEDGE: exact API names vary by REFramework version; the
  family — RTTI-driven Lua API + native singletons + hook callbacks — is
  stable].
- **The key architectural fact**: RE Engine ships a rich runtime type system
  (`via::reflection` / RETypes / REManagedObject). REFramework reads types,
  fields and methods from the engine's RTTI, so its per-game offset surface is
  SMALL — mostly bootstrapping signatures (thread context, type DB, a few
  function hooks) per game+patch, exactly analogous to UE reflection carrying
  UE4SS [TRAINING-KNOWLEDGE + INFERENCE]. REFramework ships per-game config/
  signature sets per title+version [TRAINING-KNOWLEDGE, version-sensitive].

### 1b. Capability table

| Capability | Need | Why |
|---|---|---|
| #1 kernel reads | OPTIONAL | Everything is in-process; kernel only if a detached host tool wanted cross-process reads. |
| #2 AOB scanner | MUST | Bootstrap sigs (thread-context, type DB, context singletons) per game+patch; RE RTTI reduces but does not eliminate them. |
| #3 code patching | MUST | Lua hook callbacks = engine-function detours; native plugin trampolines. |
| #4 early loading | MUST (timing) / SOLVED | Needs to be in before engine init to observe RTTI setup; ploader process-start injection is that class. |
| #5 debugger | OPTIONAL | Dev aid for sig work; REFramework itself does not need it. |
| #6 kernel input | HELPS | Menu/input for its ImGui UI on PC comes from Windows input hooks; on PS5 the pad-chord route (PS5Pad.ini mapping, proven for RegisterKeyBind [LOCAL-SOURCE: API-PARITY-TABLE.md §1a RegisterKeyBind row]) is the substitute until a kernel path exists. |
| #7 file overlay | HELPS | Addons/scripts/config load from disk; our NativeModSource logical-key snapshot already solves the "no mod disk root" variant for Lua content [LOCAL-SOURCE: API-PARITY-TABLE.md §4-5]. |
| #8 asset conversion | OPTIONAL | Framework core is scripts+hooks; texture/model mods would need RE PCK handling, separate from the framework. |

### 1c. PS5 feasibility

- **Transfers directly**: (1) the whole Lua hosting stack — LuaMadeSimple/
  LuaRaw are engine-agnostic and console-proven [LOCAL-SOURCE: API-PARITY-TABLE
  §4]; REFramework's Lua API would bind onto it like LuaMod.cpp does. (2) The
  ploader injection timing (process start, pre-engine-init) [LOCAL-SOURCE].
  (3) The profile-generator pattern: RE anchors become a new anchor pack for
  `ref_axes`/`offline_anchor_scan`-class tooling — the machinery already
  correctly REFUSES non-UE binaries (Astro Bot: 0/244 markers, 0 anchors, clean
  refuse [LOCAL-SOURCE: agent-FZ-next-tracks.md Task B]), which is exactly the
  admission discipline a second engine needs. (4) String-seam lessons: the
  UTF-8/UTF-16 bridge + shortwchar harness and the FName Add-vs-Find pool seam
  (construct with Find, never Add, verify index-Equals; the l26 root-cause)
  [LOCAL-SOURCE: STATE-L26-WAVE.md, agent-EY/EX/FL] map directly onto RE
  Engine's interned type-name tables and any wide-string ABI differences
  [INFERENCE]. (5) The R3-marshal soft-refusal pattern (eager detours stay
  unregistered with a loud refusal instead of arming blind) is precisely the
  safety shape a foreign-engine hook layer needs [LOCAL-SOURCE: API-PARITY-TABLE
  §1a InitGameState-family row].
- **New work**: (1) RE Engine anchor pack + profile generator generalization
  (RTTI type-DB discovery, via.* string anchors, thread-context sig) — this is
  the "profile-generator per engine" problem, moderate. (2) A present/overlay
  route: REFramework's ImGui renders through D3D11/12/Vulkan present hooks;
  PS5 is GNM/sceVideoOut — no existing ImGui backend. Mitigation already
  proven in-family: drive the GAME's own UI (the spawner's UMG menu) or defer
  UI (script-only mods first) [LOCAL-SOURCE for the UMG proof: MISSION "CONSOLE
  VERDICT", ACCEPTANCE-LADDER rung 2]. (3) PS5 RE executables are stripped
  eboots in PFS containers — sig scanning runs against a decrypted eboot image
  plus live process memory, same as Townfall's pin profile [INFERENCE from
  LOCAL-SOURCE patterns: FZ-GAME2 decrypt-off acquisition procedure].
- **BLOCKED**: nothing fundamental. No managed runtime, no signed-code load,
  no eboot modification required for the script+hook core.

### 1d. Self-test shape (game-agnostic base surface)

The UE4SS equivalent of "a known UObject/UClass reachable in every UE game" is
"the via.* reflection base surface reachable in every RE game". Concrete
[FT2]-style probe list (Lua, pcall-per-call, marker per probe, PASS = 4 of 5
core; R5/R6 optional discriminators):

```
RF1 core   pcall ctx = sdk.get_thread_context()          -> non-nil
RF2 core   pcall td  = sdk.find_type_definition("via.Object") -> non-nil;
           pcall n = td:get_full_name()                  -> round-trips "via.Object"
RF3 core   pcall rt  = sdk.get_native_singleton(<profile base singleton,
                 e.g. via.System/via.SceneView class>)   -> non-nil
RF4 core   pcall cnt = <type DB size / registered via.* type count> -> > threshold
RF5 opt    pcall ms  = sdk.get_managed_singleton(<profile app.* singleton>)
           -> game-specific; discriminator only, NEVER a required rung
RF6 opt    pcall h = sdk.hook(<benign always-called native, pre=nil post=nil>);
           pcall sdk.unhook(h)                           -> register+immediate-revert
           round-trip; no callback body, no behavior change
```

Crash-law guards: every RF2-RF5 result is IsValid/pcall-checked before any
member dereference (the null-wrapper lesson: a non-nil Lua value can wrap a
null object — only a pcall'd validity call proves resolution
[LOCAL-SOURCE: SESSION-L26V2-ANALYSIS / MISSION null-REMOTE wrapper note]).
Names in angle brackets come from the per-game profile, never hardcoded
offsets (offset law).

---

## 2. BepInEx (Unity)

### 2a. What it is on PC [TRAINING-KNOWLEDGE + LOCAL-SOURCE]

- **Load mechanism**: Doorstop — a Unity loader injected via proxy DLL
  (Windows) or LD_PRELOAD-equivalent (Linux); for IL2CPP games it intercepts
  `il2cpp_init`, then boots the BepInEx manager inside a managed runtime
  (Mono for BepInEx 5; CoreCLR for the IL2CPP/"Bleeding Edge" line via
  Il2CppInterop-generated proxy assemblies from `global-metadata.dat`).
- **Modding surface**: C# plugin assemblies patched in-process with
  (MonoMod/)Harmony — prefix/postfix/transpiler patches on managed and
  (IL2CPP case) engine-native methods; config/console layer; plugins are
  managed DLLs, not data files.
- **Local primary-source findings already in tree**
  [LOCAL-SOURCE: `outputs/2026-09-30-roadmap-technical-audit.md` Phase 7]:
  Il2CppInterop is a CoreCLR-to-IL2CPP bridge whose pinned class injector uses
  Unity-version-specific native layouts, GC handles, delegate function
  pointers and generated `DynamicMethod` IL; its detour interface needs a
  platform detour provider; native Harmony-style detours are NOT automatically
  the managed-Harmony patching contract; "most IL2CPP PC mods work" needs a
  corpus and does not follow from metadata availability.

### 2b. Capability table

| Capability | Need | Why |
|---|---|---|
| #1 kernel reads | OPTIONAL | In-process interop is the design; kernel only for detached tooling. |
| #2 AOB scanner | MUST | PS5 IL2CPP games export nothing (il2cpp_* API is internal to the stripped eboot); il2cpp_init/domain/class-from-name/metadata pointers must come from sigs per game. |
| #3 code patching | MUST | Method hooks/detours are the entire modding mechanism. |
| #4 early loading | MUST (timing) / SOLVED-by-ploader | Must precede il2cpp_init / first scene; process-start injection is that class (verify per game that module_start precedes Unity main). |
| #5 debugger | OPTIONAL | Dev aid. |
| #6 kernel input | HELPS | Plugins read Unity input; not framework-core. |
| #7 file overlay | HELPS | plugins/ + config/ + custom asset loads; NativeModSource-pattern applies. |
| #8 asset conversion | HELPS | Asset mods need Unity asset conversion per platform. |

**Off-table blocker (must be stated honestly)**: the managed runtime. PS5
console Unity titles build with the IL2CPP scripting backend (AOT C++); Mono
is not a console scripting backend [TRAINING-KNOWLEDGE — Unity console
requirement; long-standing, low staleness risk, but verify against Unity
docs]. There is no Mono and no CoreCLR in a PS5 Unity game, PS5 userland has
no JIT (W^X), and the local audit already concluded: interpreter-only managed
execution is a *candidate*, and iOS's Mono interpreter is evidence
interpretation can avoid JIT — not evidence of a PS5 BepInEx surface
[LOCAL-SOURCE: roadmap-technical-audit Phase 7].

### 2c. PS5 feasibility

- **Transfers directly**: (1) the IL2CPP *data* machinery — locating
  global-metadata (magic sanity), the metadata-name problem is cousin to our
  FName/name-table work (interned name table lessons apply)
  [INFERENCE from LOCAL-SOURCE seam history]; (2) ploader timing; (3) profile/
  anchor tooling and the refusal discipline; (4) detour soft-refusal safety
  pattern.
- **New work** (large): (1) native IL2CPP interop layer — find il2cpp
  functions by sig, domain/thread/class/method resolution, a native "call
  engine method" dispatcher (our ProcessEvent dispatcher is the exact analog
  [LOCAL-SOURCE: API-PARITY-TABLE §2a K2_/reflected call dispatch]); (2) a
  scripting surface over it (Lua over IL2CPP metadata — like UE4SS-Lua over UE
  reflection). That yields a "UE4SS-for-Unity", NOT BepInEx.
- **BLOCKED (honest)**: running actual PC BepInEx plugins (managed C# DLLs)
  as-is. Requires a managed runtime on PS5: either a Mono-interpreter port
  (research-scale: threads/signals/IO on the payload SDK, plus HarmonyX and
  Il2CppInterop runtime paths the local audit flagged — DynamicMethod IL,
  GC handles, delegate fn-ptrs) or an IL interpreter of our own. Until then
  "BepInEx on PS5" means IL2CPP interop via our own machinery with a
  plugin-surface subset; PC C# plugins do not run. Also BLOCKED-class: any
  route that loads the game's own signed modules unsigned (console rules;
  irrelevant here since we inject our own PRX, which is proven legal-shape in
  our stack [LOCAL-SOURCE: MISSION delivery stack]).

### 2d. Self-test shape

Two layers. The IL2CPP precondition layer is testable NOW; the true BepInEx
round-trip (Harmony patch on a System method) is gated on a managed runtime
and listed only as target-shape.

```
IL2CPP precondition probes (native, pcall-equivalent guarded; PASS = 4 of 5):
BP1 core  resolve il2cpp_domain_get via profile sig; call -> non-null domain
BP2 core  il2cpp_domain_get_assemblies (or assemblies_count) -> count > 0
BP3 core  il2cpp_class_from_name(core image, "System", "String") -> non-null
BP4 core  il2cpp_class_get_name(cls) -> "String" (read-only round-trip)
BP5 core  il2cpp_class_get_method_from_name(cls, <profile benign getter>) -> non-null
BP6 opt   global-metadata sanity: locate metadata in memory; magic/field-count
          sanity vs profile; read-only
Gated (BLOCKED today, target-shape only):
BP7       Harmony prefix on System.String::Concat; call Concat; counter fired;
          postfix fired; unpatch; call again -> original behavior (register-
          revert round-trip, zero lasting side effect)
```

---

## 3. Bethesda SKSE-class loader (SKSE/F4SE family)

### 3a. What it is on PC [TRAINING-KNOWLEDGE + LOCAL-SOURCE]

- **Load mechanism**: a launcher (skse64_loader.exe) starts the game with
  native DLLs injected, hooking engine startup; the plugin runtime then hooks
  dozens of engine functions (memory manager, Papyrus VM, Scaleform, input)
  and exposes a native plugin API (CommonLibSSE(-NG) with the Address Library
  per-build offset database).
- **Modding surface**: native C++ SKSE plugins (.dll) that often register new
  **Papyrus** native functions consumed by script mods; plus the data layer
  (.esp/.esm plugins, .bsa/.ba2 archives) the GAME itself loads.
- **Local work already in tree** [LOCAL-SOURCE]:
  `outputs/2026-10-01-bethesda-header-inspection.md` — a TES4 generic plugin
  header inspector exists (20/24-byte root layouts, HEDR-first, Mutagen/
  xEdit-grounded, 179 core tests, compiles for PS5 target); the feasibility
  doc names the family: "Bethesda scripts/extenders: ordinary data/scripts
  and SKSE/F4SE services; needed evidence: equivalent console VM/engine
  services, object layouts and hook sites"
  [LOCAL-SOURCE: `outputs/ps5-mod-manager-feasibility.md` backends table].

### 3b. Capability table

| Capability | Need | Why |
|---|---|---|
| #1 kernel reads | OPTIONAL | In-process by design. |
| #2 AOB scanner | MUST | PC uses the Address Library (per-build offset DB); PS5 builds have none — pinned per-title+patch offset profiles replace it (the exact profile-generator generalization problem). |
| #3 code patching | MUST | SKSE's value is deep engine hooks (VM, allocator, Scaleform); biggest hook surface of the five. |
| #4 early loading | MUST (timing) / SOLVED-by-ploader | Must precede Papyrus VM init; injection timing class is fine. |
| #5 debugger | OPTIONAL | Dev aid. |
| #6 kernel input | HELPS→MUST for UI mods | MCM-style UI + hotkeys need an input route; pad-chord mapping substitutes short-term. |
| #7 file overlay | MUST (data layer) | .esp/.esm/.bsa must reach the game's data scan + load order; on PS5 game data lives in encrypted PFS containers, so overlay-at-file-layer (not on-disk replacement) is required. |
| #8 asset conversion | HELPS | nif/dds/ba2 cross-platform variants for asset mods (console-format variants exist historically — Skyrim shipped on PS3/PS4, proving format portability at the data level). |

### 3c. PS5 feasibility

- **Transfers directly**: data-layer inspection (TES4 header inspector);
  profile/anchor tooling; ploader timing; Lua hosting if the "extender"
  surface is re-exposed as script (a "SKSE-lite" that only reads/writes forms
  and runs Papyrus-external scripts).
- **New work**: per-build offset profiles with NO address-library shortcut;
  Papyrus VM binding (read stacks, call natives) to give script mods the SKSE
  function surface; form-data overlay into the game's data scan.
- **BLOCKED / cold**: (1) PC SKSE plugins are native Windows PE DLLs — they
  do not run, period; everything is a reimplementation. (2) **There is no
  PS5-native Skyrim/Fallout** (PS4 versions run via BC; the ploader/etaHEN
  injection stack targets PS5-native PPSA processes — a PS4-BC title is a
  different environment) [TRAINING-KNOWLEDGE + INFERENCE; verify which
  injection route, if any, reaches PS4-BC processes on this setup]. (3)
  Papyrus is an interpreted VM inside the game — binding it is feasible but
  sizeable.

### 3d. Self-test shape (TESForm query core)

```
BS1 core  pcall read DataHandler/TES globals via profile pin -> version
          fields sane (read-only)
BS2 core  pcall GetFormByID(0x14) (player form) -> non-null TESForm
          [TRAINING-KNOWLEDGE: 0x14 = player ref, stable since Oblivion;
          verify per title]
BS3 core  pcall total loaded-form count / FormDB size -> > threshold
BS4 core  pcall read Papyrus VM stack count (BSTHashMap-style size field) ->
          > 0 after a game save is loaded
BS5 core  pcall read a known global (e.g. TESGlobal "GameHour"-class) ->
          plausible float range (0 < v < 24)  [name per title profile]
BS6 opt   pcall resolve a profile hook-site function address; verify pinned
          prologue bytes match (read-only — no install, no patch)
```

PASS = 4 of 5 core; every read goes through profile pins (offset law). BS6 is
the register-free analogue of RF6 — proving the *site* is found without
touching it, honoring the no-side-effect law.

---

## 4. FromSoftware DSMS/ModEngine-class loader

("DSMS" itself I could not find in local trees or pin down in training data —
treated as a member of the ModEngine/launcher-redirect family per the task
framing; the local ModEngine2/me3 research below is the pinned member.
[INFERENCE])

### 4a. What it is on PC [LOCAL-SOURCE unless noted]

- **Load mechanism**: a launcher starts the game process with a Windows DLL
  injected (ModEngine2 model); hooks the file-open layer to redirect archive/
  file reads, applies config-driven memory edits, manages mod package order.
  ModEngine2 is discontinued upstream; successor is **me3** (Windows; Linux
  via Proton, macOS via CrossOver — i.e. the non-Windows paths still run
  win64 binaries) [LOCAL-SOURCE:
  `.superpowers/native-dependency-spike/mod-engine-runtime-options-2/{notes.md,evidence.json}`
  — pinned 2026-10-01 with URLs]. me3's profile schema separates **packages**
  (asset overrides) from **natives** (DLLs) [same local source].
- **Modding surface**: overwhelmingly DATA mods — param/regulation edits,
  models/textures in the game's own container formats, load-order packages.
  Scripting is not the framework's core (unlike the other four); "natives"
  entries are DLLs and explicitly out of scope for us
  [LOCAL-SOURCE: same notes — "Native DLL entries remain unresolved/
  unsupported under existing capability policy"].

### 4b. Capability table

| Capability | Need | Why |
|---|---|---|
| #1 kernel reads | OPTIONAL | Alternative route: a sysentvec-level file-syscall redirect via Phase B kstuff would replace in-process fd hooks — powerful but kernel-gated. |
| #2 AOB scanner | MUST-lite | Few anchors: the game's file-open/read functions + version globals per title+patch; much smaller than SKSE/SHV surfaces. |
| #3 code patching | HELPS→MUST | In-process route = hook the game's file-device open/read (FromSoft's FD layer). Data-only edits may need no code patch at all if redirect happens at the fd/syscall layer. |
| #4 early loading | MUST (timing) / SOLVED-by-ploader | Must precede the game's first archive open; process-start injection class. |
| #5 debugger | OPTIONAL | Dev aid. |
| #6 kernel input | OPTIONAL | ModEngine has no in-game UI; hotkey toggles are minor. |
| #7 file overlay | **MUST (the core)** | The framework IS a file-overlay orchestrator (package order + virtual file paths preserved as data [LOCAL-SOURCE]). |
| #8 asset conversion | HELPS | DCX/param/model formats are mostly platform-shared with per-platform texture variants; conversion needed for asset-heavy mods. |

### 4c. PS5 feasibility

- **Transfers directly**: (1) #7 host work and the overlay design pattern —
  packages/order/virtual paths as DATA with natives kept separate is exactly
  the shape our importer should consume [LOCAL-SOURCE: mod-engine notes]; (2)
  ploader timing; (3) profile machinery; (4) if the overlay rides the
  game's own file layer, no detour of game CODE is needed for pure data mods
  — the lowest-risk hook budget of the five.
- **New work**: per-title FD-layer anchors (the profile generalization
  problem again); a register/unregister overlay service with hash-verified
  original reads (the revert path is already a mission law for #7
  [LOCAL-SOURCE: MISSION PHASE A item 3]).
- **BLOCKED / cold**: "natives" entries (PC DLLs) — unsupported class.
  Container reality: PS5 FromSoft titles ship inside PFS; the game's inner
  archive layout differs from PC (e.g. PC's data0.bdt-style naming is not
  guaranteed) [TRAINING-KNOWLEDGE, version-sensitive — must be verified from
  an owned title]. Online/ban risk: any FromSoft title with network features
  must be tested offline only [INFERENCE; standing console-policy scope in
  MISSION §5 item 10].

### 4d. Self-test shape (overlay pair + FSM lookup)

```
ME1 core  pcall open a known base-game file through the game's own fd layer,
          NO redirect -> success + sha256 == profile pin (baseline sanity)
ME2 core  pcall register overlay redirect for a NON-ESSENTIAL probe path to a
          served buffer of identical size; open through fd layer -> served
          hash matches; pcall UNREGISTER -> ME1 rerun == original hash
          (register/revert round-trip; no gameplay file ever modified)
ME3 core  pcall enumerate the game's file-device/archive registry -> count
          > threshold
ME4 core  pcall read the engine/game version global -> matches profile
          expectation
ME5 core  pcall look up a known behavior FSM / AI-table entry (per-title
          profile name) -> non-null   [TRAINING-KNOWLEDGE: FromSoft behavior
          tables are per-build — profile-only, version-sensitive]
ME6 opt   pcall param-table count > threshold (data layer alive)
```

PASS = 4 of 5 core, and ME1+ME2 must BOTH pass for any overlay-capability
claim (an overlay that breaks baseline reads is a failure even if the other
probes pass). ME2 is the in-game proof of capability #7 the mission already
wants — this self-test double-counts toward that.

---

## 5. RAGE / ScriptHookV-class (GTA V)

### 5a. What it is on PC [TRAINING-KNOWLEDGE]

- **Load mechanism**: ScriptHookV is a dinput8.dll-class proxy injected at
  process start; it scans memory to locate the game's **native function
  table** (hash-indexed), hooks the script tick (the game's script VM
  "RunningScript" pump) and presents a C++ API where plugins (.asi) run as
  scripts and invoke natives by hash. **Closed source** (Alexander Blade);
  per-game-build offsets shipped as updates. Open ecosystem exists around it
  (LUA-plugin add-ons, .asi plugins); FiveM is a separate, much larger
  multiplayer client with its own runtime (partially open).
- **Modding surface**: native-call scripting (C++ .asi, Lua/JS via add-ons);
  asset mods via RPF archive replacement (OpenIV-class tooling, separate from
  SHV proper).
- Version-sensitive: SHV build coverage tracks PC patches; console versions
  never supported upstream [TRAINING-KNOWLEDGE].

### 5b. Capability table

| Capability | Need | Why |
|---|---|---|
| #1 kernel reads | OPTIONAL | In-process by design. |
| #2 AOB scanner | MUST | The natives table + script-tick anchors are per-build signature work; SHV's whole release cadence is per-build offset updates. |
| #3 code patching | MUST | Script-tick hook + native invocation stubs; overlay .asi need present/pump hooks. |
| #4 early loading | HELPS / SOLVED-by-ploader | Process-start load; strictly it can also attach late — the most timing-tolerant of the five. |
| #5 debugger | OPTIONAL | Dev aid. |
| #6 kernel input | HELPS | Input is normally read through natives themselves; PC console-hotkeys are Windows hooks — pad route substitutes. |
| #7 file overlay | HELPS | RPF asset replacement is OpenIV-style overlay, adjacent to (not part of) SHV core. |
| #8 asset conversion | HELPS | RPF/YTD/model formats per platform. |

### 5c. PS5 feasibility

- **Transfers directly**: ploader timing; profile machinery; Lua hosting (a
  Lua-native-invoker surface would ride the proven Lua stack); the self-test
  discipline.
- **New work**: full reimplementation (closed-source core — nothing to port);
  natives-table discovery + a safe invoker (call-convention marshaling per
  native signature — our LuaUObject param marshaling is the cousin).
- **BLOCKED / cold**: (1) closed source upstream = clean-room rebuild of the
  loader; (2) per-build natives DB maintenance against PS5 builds nobody else
  maintains offsets for; (3) GTA V PS5 "Enhanced" console edition differences
  vs PC builds (layout divergence beyond sig tolerance) [TRAINING-KNOWLEDGE,
  version-sensitive]; (4) highest legal-heat franchise for console modding
  [INFERENCE]; (5) Online anti-cheat/ban surface means offline-only testing.

### 5d. Self-test shape (script handler count + natives round-trip)

```
RG1 core  pcall locate natives table via profile sig -> entry count > threshold
RG2 core  pcall resolve a known benign native by stable hash (e.g. a
          GET_CURRENT_LANGUAGE/GET_FRAME_COUNT-class getter) -> non-null fn
          ptr   [native HASHES are stable across builds; per-title verify]
RG3 core  pcall read script-handler / script-thread pool count -> > 0 after
          the game reaches gameplay (game-agnostic-ish state gate: count a
          known set of ambient scripts)
RG4 core  pcall invoke the RG2 native via invoker -> plausible return value
          (e.g. language id in range) — the marshaling round-trip proof
RG5 core  pcall invoke PLAYER-class read-only native (player handle) when
          in-game -> non-zero handle; NO spawn/teleport/write native may
          appear in the self-test (side-effect law)
RG6 opt   pcall register a tick callback that only increments a counter,
          unregister after N ticks (register/revert; no game-state writes)
```

PASS = 4 of 5 core; RG4 is the invoker proof and should be required for any
"natives callable" claim (N-of-M may pass 4/5 without RG4 only if RG4 is
explicitly marked not-yet-armed).

---

## 6. RANKING — best SECOND target after UE4SS

Criteria per task: (a) overlap with existing machinery (ploader injection +
offset-pin profile pattern + Lua/string-seam lessons), (b) mod availability,
(c) risk. Scored:

| Rank | Framework | Machinery overlap | Mod availability | Risk | One-liner |
|---|---|---|---|---|---|
| **1** | **REFramework** | **Highest** — reflection-driven like UE (small offset surface), Lua stack ports as-is, injection timing proven, NativeModSource pattern fits addons | Large Lua-addon ecosystem; many PS5-native RE Engine titles exist (RE2R/RE3R/RE4R/RE7/RE8/DD2/MH Rise/MH Wilds/SF6-class) [TRAINING-KNOWLEDGE, verify per title] | Medium: UI/present route is the main new machinery; script+hook core unblocked | "UE4SS's architecture wearing an RE jacket" |
| 2 | ModEngine-class | High on #7/#8 (overlay-as-data matches our importer design), zero Lua reuse | Huge FromSoft data-mod scene; PS5-native FromSoft titles exist (Elden Ring/AC6/Demon's Souls) [TRAINING-KNOWLEDGE] | Low-Medium for data-only mods (smallest hook budget); online/ban discipline required; no UI needed | Lowest-risk track; doubles as the in-game #7 capability proof |
| 3 | BepInEx | Medium (IL2CPP probe layer shares machinery; everything else new) | Biggest mod universe of the five | BLOCKED-class on managed runtime for real plugins; "our own interop" is honest scope | IL2CPP probes now; plugins = research-scale |
| 4 | SKSE-class | Data layer already locally built (TES4 inspector); runtime layer = full reimpl | Large but native-plugin-heavy | No PS5-native title (PS4-BC environment mismatch); biggest hook surface | Data-layer work stands; extender runtime is cold |
| 5 | RAGE/SHV | Low (closed source; everything clean-room) | Large but .asi/native-heavy | Highest (per-build natives DB alone, plus legal heat, plus Online) | Last |

**Recommendation: REFramework is the second target**, with ModEngine-class
held as the parallel low-risk data track (its ME1/ME2 self-test is also the
mission's pending in-game #7 proof — no wasted work).

Why REFramework over ModEngine despite ModEngine's lower complexity: the
project's stated goal is PS5-as-PC MODDING parity with unmodified-PC-mod
execution [LOCAL-SOURCE: MISSION "unmodified PC Lua goal" precedent]. Only
REFramework reuses the entire proven center of our stack (Lua hosting,
reflection-driven binding, name-table seam solutions, hook soft-refusal
safety) while adding just one genuinely new subsystem (the UI/present route —
which can be deferred; script-only mods don't need it). ModEngine is simpler
but is a *different* product (data overlay) with little reuse of the hard-won
Lua/reflection machinery.

**First three concrete steps (REFramework):**

1. **Mac-local engine-classification dry run** — acquire the decrypted eboot
   of a user-owned PS5-native RE Engine title using the documented
   decrypt-off FTP procedure (never re-upload; exactly the FZ-GAME2 Stellar
   Blade recipe [LOCAL-SOURCE: agent-FZ-next-tracks.md Task B]) and run the
   existing classifier + anchor tooling with a NEW RE anchor pack
   (via::reflection type-name strings, RETypes markers, thread-context sigs).
   Expected outcome per the Astro Bot precedent: clean pin-or-refuse with zero
   console contact [LOCAL-SOURCE: FZ-GAME2 0/244 refusal proof].
2. **RE self-test PRX** — implement RF1-RF5 (read-only, no hooks, no UI) as a
   ploader PRX following the ueselftest laws: pcall-per-call, profile-sourced
   names only, N-of-M marker lines in the log, one-launch test ladder with
   harvest before any relaunch [LOCAL-SOURCE standing routine rules].
3. **First bounded hook round-trip** — RF6 only after 1-2 pass three console
   boots: register+immediately-unregister one benign always-called native
   (empty pre/post callbacks), verify clean revert and no crash; decrypt-off
   backups, one user-launched test per boot, klog to NEW files
   [LOCAL-SOURCE: MISSION session rules].

**Honest gaps in this research**: REFramework API/license details, the exact
DSMS tool identity, RE/FromSoft/GTA PS5 title file layouts, and native-hash
stability claims are all [TRAINING-KNOWLEDGE] and must be pinned from primary
sources (vendored repos, same as UE4SS was) before step 1 code. No BepInEx/
REFramework sources exist in the local trees today (surveyed; only ModEngine2/
me3 notes, the IL2CPP audit, and the Bethesda inspector are local).

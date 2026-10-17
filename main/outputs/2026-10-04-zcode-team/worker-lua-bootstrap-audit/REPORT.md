# REPORT — worker-lua-bootstrap-audit

Date: 2026-10-04. Scope: static audit only (saved sources + llvm-nm on saved objects).
No console contact, no nested subagents, no execution of game/runtime code, no writes outside
`outputs/2026-10-04-zcode-team/worker-lua-bootstrap-audit/`.

## Deliverables

- `CLAIM.json` (written first) — 6 claims + explicit unverified/risks.
- `MODS-STARTED-PATH.md` — ordered step table with file:line cites, gating answer, Lua-linkage
  section, failure-mode table.

## Task results (condensed)

### 1. `RuntimeOwner::start()` → `mods-started` trace
Two phases, two threads. Phase A (loader thread, inside `module_start`): `mfa_activate`
snapshot read (`runtime_module_entry.c:250`) → bridge lmf/context build
(`runtime_start_bridge_r2.cpp:155-184`) → `StartActivatedRuntimeWithAck`
(`runtime_ack_supplier.cpp:68-95`: `MakeRuntimeAckConfig` → `InstallDefaultEngineBindings` →
`PrepareActivatedUE4SSRuntime` → `owner->start()`). Inside prepare:
`MakeActivatedUE4SSModSource` (`activated_mod_source.cpp:16-46`) → `NativeModSource` ctor
enumerates `ue4ss/Mods/<Name>/Scripts*/…/main.lua` mods and computes the startup list from
mods.txt rows then enabled.txt markers (`NativeModSource.cpp:79-135`); `RuntimeAckSink::Open`
(`runtime_owner.cpp:130-153`); `UE4SSBackend` ctor forces `Deferred`, constructs
`RC::UE4SSProgram` (paths, `validate_native_services` with pad-provider init, publish
`s_program`), then `start_native_mods()` (`ue4ss_backend.cpp:27,47,53` →
`UE4SSProgram.cpp:549-592`) → `setup_mods` (LuaMod ctor creates each Lua VM,
`LuaMod.cpp:900-902`) → `start_cpp_mods` → native branch starts `startup_mod_names()`
(`UE4SSProgram.cpp:1946-1956`) → `LuaMod::start_mod` executes `main.lua`
(`LuaMod.cpp:6440-6480,6371-6438`) → `mods_startup_verified_=true` → `activated` ack.
Phase B (init thread, spawned by `start()` which returns immediately, `runtime_owner.cpp:23-42`):
`UE4SSProgram::init` → `setup_unreal` (retained snapshot ModuleSource, StartupWait 5-min
deadline, `UE4SSProgram.cpp:930-957`; `StartupWaitControl.hpp:102`) → properties →
`share_lua_functions` → `on_engine_initialized()` (`UE4SSProgram.cpp:654-661`) → backend chain
writes `engine-ready` then `mods-started` (`ue4ss_backend.cpp:41-45`; `runtime_owner.cpp:171-173`)
→ event loop starts afterwards (`:673`).

### 2. Gating
Lua VM creation + `main.lua` execution happen on the loader thread during `module_start`,
**before** the init thread exists — gated on nothing (no tick, no engine-ready). The
`mods-started` line is gated on engine-ready: written on the init thread inside the
`on_engine_initialized` site, before the event loop starts; expected latency is
UnrealInitializer scan time over the retained snapshot, bounded by the 5-minute cooperative
`StartupWait` (expiry = `failed`, not a hang), with no game-frame dependency.

### 3. LuaRaw dependency
Exactly one Lua implementation is statically linked: vendored PUC-Rio **Lua 5.4** from
`ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw`. Evidence:
composition compile commands carry `-I …/deps/first/LuaRaw/include`; the packaged
`combined-full.o` defines 145 `lua_*`/`luaL_*` symbols including `lua_newstate`,
`luaL_newstate`, `lua_pcallk`, `luaD_call`, `luaV_execute` (core, not headers-only); the
composition-4 composed object alone defines the same (so the UE4SS port links the identical
core); the aligned-entry probe `memory_lua.c` uses the same headers with an
`lua_Integer == long long` ABI assert and produced the observed `lua_call_result value=…` log.

### 4. Failure modes (full table in MODS-STARTED-PATH.md §4)
- Script syntax/runtime error: **loud** — `Error loading script:` / `Error executing script:` +
  `Failed to execute main script:` (Error level) — but **non-fatal**: `m_is_started` is set
  before execution, so `mods-started` still arrives.
- Missing UEHelpers: **loud** — searcher's `module 'UEHelpers' not found:…` surfaces through
  pcall as `Error executing script: …`.
- RegisterKeyBind before pad provider: **cannot happen at load** — provider activation is in the
  program constructor (`validate_native_services`) before mods start; a provider failure fails
  the session loudly (`Native supplied input source is unavailable or failed activation` →
  `ack->failed` → `modbridge module_start failed: runtime_start_failed` to UE4SS.log +
  `failed` status). Registration itself is silent (queues into Input::Handler).
- NotifyOnNewObject at load: quiet — queues a pending callback, logs only at Verbose; delivery
  later gated by `ShouldRunNativeModCallbacks`. Risk flagged: FName find/add before name-pool
  setup is unguarded in LuaMod.cpp (unverified).
- FindFirstOf at load: **silent nil** — null-`GUObjectArray` guard returns nullptr pre-init.
- Success: **quiet** — only `Setting up mods...` (+ first-mod `[Lua] Exposing size …` lines);
  there is no per-mod "started" line on the native path. Observable success =
  `runtime-status.txt` `activated` → `engine-ready` → `mods-started`.

## Method + honesty notes
- Composition-5 inputs were pinned precisely: `runtime_owner.cpp` verified byte-identical to the
  composition-3 replacement unit; the `ue4ss_backend.cpp` actually linked (per
  `build_runtime_composition5.py:167` + `overlay-manifest.json`) differs from the older
  `outputs/ps5-mod-bridge` copy only in the 4-arg prepare signature; both were read.
- Symbol evidence via `llvm-nm --defined-only` on saved `combined-full.o` and composition-4
  run-3 `coherent-tick-file-witness-runtime.o`. Nothing was relinked, executed, or captured.
- Line numbers cite the saved files as read on 2026-10-04; any future source revision invalidates
  them. "Silent"/"loud" classifications are from code reading only, not from a live UE4SS.log.
- No claim is made about in-game behavior beyond this static path; this audit does not replace
  regression tests or director verification.

Tool budget: ~38 calls. All writes confined to this worker directory.

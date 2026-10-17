# REPORT — worker-post-seeds-chain (2026-10-05)

LOCAL ONLY: no console contact, no dispatches, no builds. Read-only over the
receipt-pinned composed units; deliverables in this dir (CLAIM.json first, CHAIN.md,
this REPORT last). All claims line-cited in CLAIM.json/CHAIN.md; nothing here is a
runtime acceptance claim.

## Units traced (why these files)

The launch-11 combined candidate's composition receipt
(`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-2/receipt.json`,
built 2026-10-05T18:37, gate GO at 18:37) pins:
initializer `launch11-combined/units/UnrealInitializer.cpp` (22f06286…), luamod
`worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` (3c39a86b…), program
`worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` (7b5d4636…); bridge
`launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` (faec2336…, from
`EDITIONS.json` bridge_cli); stage headers SeedValidation.hpp b3d81b0f (already
**kMaxPublishedSpans=64**, line 143) and the fixed Helpers/String.hpp 7c983db4
(`launch11_one_pass.py:163`). Which fself pid 672 actually ran is a controller-side fact;
where it changes the reading, I say so.

## Q1 — Seeds order, and why GUA/GMalloc bound despite the pool refusal

Order (initializer `consume_required_slot_seeds`, UnrealInitializer.cpp:833):
Phase 1 base check (837-842) → Phase 2 slot 0 GUA bind+log (857-858) → Phase 2b slot 3
GMalloc V1/V2 bind+log (908-909) → **Phase 3 FName pool validation (916-1039; pool =
kBaseObserved 0x400000 + label 0x9365ec0 = 0x9765EC0, SeedTable.h:11-12)** → Phase 4 ctor
bind + ByteProperty probe (1053-1063) → Phase 5 SCO bind (1079-1080).

**The pool validation runs strictly AFTER both binds, and a refusal throws without rolling
back** — so `[Seeds] slot 0 ... bound` and `[Seeds] slot 3 ... bound` are printed before
any pool refusal can exist. That is the whole mechanism; nothing "retries past" a refusal.

Boot-path corollary that matters for reading pid 672: staged settings have no DoEarlyScan
key and the default is false (SettingsManager.hpp:43), so ScanGame runs exactly once —
from `Initialize` (1644-1647) inside the PROSPERO try whose only catch disarms+rethrows
(2241-2248). The runtime_error swallow at UE4SSProgram.cpp:1329-1332 belongs to the dead
DoEarlyScan call site. **On this source, a pool refusal in the same boot necessarily
aborts init** — seeds lines followed by a healthy continuing startup is not a reachable
combination.

## Q2 — "Stopping mod ... for uninstall"

It is the first log line of `LuaMod::uninstall()` (LuaMod.cpp:7039/7051). On PROSPERO it
is reached only via `UE4SSProgram::uninstall_mods()` (2224-2286; Lua mods uninstalled
unconditionally at 2241-2259 — even a not-installable mod prints the line), and
`uninstall_mods()` runs only inside `shutdown_native_program()` (546), which itself runs
from `~UE4SSProgram` (511), ctor catches (494/502), init post-join error (716-718), or the
init catch (727-743, shutdown at 740).

**There is no seeds-refusal→mod-loader signal.** The only coupling is the shutdown path:
seeds throw (1178→833) → init catch → `copy_error_into_message("[Seeds] REFUTED: …")`
(737) → shutdown_native_program (740) → uninstall_mods (546) → "Stopping mod …" (7051);
the same throw then becomes the runtime-status **failed** line via backend rethrow
(ue4ss_backend.cpp:67-70) → RuntimeOwner catch (runtime_owner.cpp:111-118) →
`ack->failed(1, …)`. "Mod is absent from native Lua source" (LuaMod.cpp:941) is an
independent ctor-time mark (`set_installable(false)` 942) and never uninstalls.

Three candidate triggers fit pid 672's shape, with discriminators:
- **A — seeds refused this boot**, immediate-refusal class only (span table unavailable
  943-962, 'None' anchor 1013-1019, entry mismatch 1021-1031, class/presence refusals).
  The truncated-shape refusal retried in the loop burns the FULL budget (120 s; staged ini
  `SecondsToScanBeforeGivingUp=120`; SettingsManager.cpp:146-153 coerces 30→120) — **an
  11 s gap cannot be the truncated-shape class on this boot**.
- **B — seeds passed; the stop was owner-initiated** (StopRuntimeWithAck → request_shutdown
  → CheckStartupWait/ThrowIfStoppedAt throw at initializer 76-85/983/1688, or event-loop
  exit → shutdown) — then the runtime-status REFUSED line is stale from a prior session.
- **C — another init throw** (required-objects fatal 2143-2153, type system 2156-2159,
  PostInitialize, …) in the same shutdown path.

Decide from pid 672 evidence: does UE4SS.log continue past the slot-3 line with
`[Seeds] FName pool validated at 0x9765EC0 …` (1038), slot 2 (1063), slot 4 (1080),
`Using engine version:` (1678), `Waiting for object construction...` (1685)? Does the
pid-672 session block of runtime-status carry `failed` with the `[Seeds] REFUTED` text vs
`engine-ready`/`mods-started`?

One chronology correction from source: "Setting up mods…" is emitted during the backend
ctor's `start_native_mods` (UE4SSProgram.cpp:585-628, log 1811) and ALL seeds lines are
emitted later, inside `init()` (655 → setup_unreal → Initialize → ScanGame). The mission's
listing of seeds lines before "Setting up mods…" cannot be literal log order on this
source — re-check raw timestamps before building on that ordering.

## Q3 — Post-seeds chain (stages + evidence lines)

S1 seeds consumed, `bScanFullyCompleted=true` (1516) → S2 ctor-verifier skip (1666-1670),
`Using engine version:` (1678), `Waiting for object construction...` ≥10000 objects
(1685-1700, through the bound GUA), KismetStringLibrary + Conv_NameToString + CDO
(1704-1753) → S3 required objects (1756-1870) → S4 engine singleton CDOs (1883-1930) →
S5 hook resolution: LoadMap (1932-1940), EngineTick — seeds leave TickInternal not-ready
so the **vtable fallback binds Tick** (1981-2010); R3 marshal **compiles out** both
EngineTick detour registrations and logs `[R3] refused: EngineTick pre-callback
registrations …` / `[R3] EngineTick detour unavailable; deferring game-thread pump to R3
marshal` with `pump_pending=true` (2039-2041); then InitGameState/BeginPlay/EndPlay/
AActor::Tick/ViewportTick/ProcessEvent/ProcessConsoleExec/UStruct::Link (2067-2138) →
S6 FNames (2140), required-objects fatal check (2143-2153), type system (2155-2159),
ProcessInternal derivation (2174-2235), disarm-on-throw (2241-2248), PostInitialize
(2250; GMalloc check 442-447, searcher pools 460-495) → S7 program ready:
`m_is_program_started=true` (691), on_engine_initialized → **ack engine_ready**
(program 693-697; ue4ss_backend.cpp:33-45), event-loop jthread (709) → S8 mods:
on_program_start (1586) → install_lua_mods (1557) → start_lua_mods (1560 → PROSPERO
branch 2019-2027, only `startup_mod_names()` mods that are INSTALLED) → S9 **game-thread
pump arm**: the EngineTick detour is deliberately not armed; the pump is deferred to the
R3 FuncPtr-marshal carrier drain (initializer bridges 2316-2373; LuaMod carrier post-hook
4571 identity publish, 4576-4580 pump_pending consumption; arming by the calibration
owner 4717+). No carrier armed → no identity publication, no query-harness dispatch.

## Q4 — Lua file read path

`read()` (NativeModSource.cpp:48-55) matches **exact UTF-8 logical keys below `ue4ss/`**
— the loadlist-staged file-table strings ("never filesystem paths", header 23-25), via
sorted binary search after `validate_path` (10-45: canonical, no `\`/`:`/control bytes,
strict UTF-8). The expected key is `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`,
which is exactly the namespace `lua_mods()` is derived from (77-105) and the value the
ctor stores as `scripts_path` (LuaMod.cpp:945). So "loadlist path vs filesystem path" is
not a choice on this target — they are the same bytes; **after the path fix
(LuaMod.cpp:42-54 `path.string()` + `\`→`/`; plus the pinned fixed String.hpp 7c983db4)
read() returns the Lua bytes for these pure-ASCII paths**. Failure modes are distinct:
key miss → `Failed to execute main script` (6968); mangled/invalid key → validate_path
throw → `Exception in load_and_execute_script` (6938). Residual honesty: `path.string()`
on this target converts wide→narrow through the libc++ locale codecvt — a different facet
from the `codecvt_utf8/utf16 do_in/do_out` helpers proven broken
(worker-construction-error ANALYSIS §3); compile-verified in situ, no console read-back
yet.

## Q5 — Lua state creation

The Lua state exists from construction: `m_lua(LuaMadeSimple::new_state())` in the ctor
init list (LuaMod.cpp:932; `luaL_newstate`, LuaMadeSimple.cpp:1088-1090). Script execution
requires the full ladder: ctor lookup matched (936-946, else not installable) → enabled
via mods.txt `Name:1` or enabled.txt (NativeModSource.cpp:106-135) → installed
(UE4SSProgram.cpp:1898-1904; start_mods find with IsInstalled::Yes 2024) → `start_mod`
(6943: prepare_mod 6949, make_main_state 6950, global functions 6951, make_async_state
6952, async thread 6953, started 6955, module loader 6959) →
`load_and_execute_script` (6874: read 6883, chunk `@name` 6904, `native_lua_chunk` 6912,
`lua_pcall` 6923). The first `[Lua] [TownfallItemSpawner] Loaded v1.0...` line is the
script's own top-level log (main.lua:746) executing during that pcall; every failure mode
has a named line (6968/6916/6927/6981).

## Recommended next probes (controller-owned, console ground truth)

1. From pid 672's UE4SS.log: presence of `[Seeds] FName pool validated at 0x9765EC0` and
   the slot-2/slot-4 lines → settles A vs B in Q2 immediately.
2. The runtime-status pid-672 session block: `failed` detail text vs `engine-ready`.
3. If A with a truncated-shape text is confirmed, note it must have come from a LONGER
   boot (>120 s budget), not the 11 s window — i.e. the line belongs to the prior session.
4. When staging the 64-span candidate: the units dir already carries b3d81b0f (64) and the
   merged luamod fix; the remaining open item is the on-console byte-exactness of the
   path conversion (first `[Lua] Loaded v1.0` line is the proof).

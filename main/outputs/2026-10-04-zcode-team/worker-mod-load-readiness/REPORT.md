# REPORT — worker-mod-load-readiness (honest limits)

Worker: worker-mod-load-readiness · 2026-10-04 · source-level readiness trace of the UNCHANGED Townfall Item Spawner against the launch-10 composed runtime. LOCAL ONLY: no console contact, no execution, no canonical edits, no files outside this directory written. Deliverables: CLAIM.json (file:line for every claim), READINESS.md (verdict table + API inventory + settings), API-GAP.md (gaps + error shapes + fixes), this file.

## What was done

1. Pinned the actual composed unit set for launch-10 from the composition receipts (368 units, `selected_runtime_units`): run-3 + l10r3 run-a/run-b. Confirmed the mod-loading units are composed: `mod`, `luamod` (notify-cancel lineage copy — NOT the port-tree LuaMod.cpp), `program` (comp6-program staged UE4SSProgram.cpp), `settings`, `output`, `input-handler`, `native-pad-input`, full LuaRaw VM, all LuaType units, bootstrap `runtime_owner`/`ue4ss_backend`/`module-runtime-ack-supplier`, `native-mod-source`.
2. Traced the entire post-engine-ready mod-load path end-to-end in the composed sources (module_start → ack supplier → activated mod source → NativeModSource discovery/enabled gate → UE4SSProgram ctor → start_native_mods inventory → init/engine-ready → event loop → on_program_start → start_lua_mods → LuaMod::start_mod → main.lua chunk load + pcall), with file:line at every hop.
3. Enumerated every Lua API global/method main.lua and UEHelpers.lua touch and verified each against the composed registrations. Result: zero absent APIs at load time.
4. Analyzed settings v3 against the composed SettingsManager (zero key drift; the two disabled hooks gate only the custom-event script hook, unused by this mod; all mod-relevant hooks keep true defaults; native-services gate passes).
5. Traced the PS5 input path (PS5Pad.ini → retained pad provider → Handler → RegisterKeyBind → UpdateThread callbacks → ExecuteInGameThread) and cross-checked it against the prior linked-object audit (worker-pad-chord-audit).
6. Identified the one variant-conditional blocker: the R3 fold-1 candidate compiles out the EngineTick pump with no consumer in the composed object (menu can never open on that variant), while the witness-verified readpath-guard lineage keeps the pump.

## Honest limits

- **No launch-10 runtime evidence exists yet.** Everything above is static source reading of composed units and staged packages. "READY" means source-complete, not proven-on-target. The engine-ready stage itself (armed SeedTable walk, `[Seeds] REFUTED` possibility) is a live gate this worker cannot verify from source.
- **Ambiguity: which initializer launch-10 actually composes.** The build witness (2026-10-05 12:56 UTC) verified the readpath-guard staging as the launch-10 input but recorded that the controller one-pass had not yet run at that time; the l10r3 (R3) candidate exists in parallel for the BOTH_FAIL matrix rows. My verdicts are therefore stated per-variant. If a newer one-pass receipt exists outside the dirs I read, the R3-caveat may be moot — re-check `worker-launch10-*` receipts at acceptance time.
- **Composed-lineage line numbers.** The composed `luamod` unit compiles `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp` (sha `0f0697a3…` per the l10r3 REPORT), not the port-tree copy; line numbers I cite for LuaMod.cpp are from that composed lineage. Where I cite port-tree files (SettingsManager, UnrealInitializer hooks block, LuaUObject, LuaLibrary, LuaModRef, Handler), those are the pinned composition sources per the receipts. I did not re-derive object-level symbol presence myself; where object-level proof exists (pad bridge symbols, armed strings, seed constants) I cite the owning workers' measured receipts rather than restating it as my own measurement.
- **Host-side activation semantics unverified.** Whether the host mfa preload yields a non-null data pointer for the 0-byte `enabled.txt` (and the exact loadlist→activation wiring) lives outside these sources; flagged as the one READY-with-unknown in the gate table.
- **In-game behavior is out of scope.** Popup, spawn, out-param marshalling, property layouts, FName-to-string and ProcessEvent behavior on the shipping binary are UNKNOWN here by construction; the report defines exactly which log/capture lines would prove each.
- **Prior-art reliance.** The pad-dispatch hop trace (worker-pad-chord-audit/PAD-DISPATCH-PATH.md) and the host-trial API matrix (worker-itemspawner-lua-audit/LUA-API-MATRIX.md) were read and cross-checked against my own source reads; they agree. I did not redo their linked-object symbol measurements.
- **No quality claims.** Nothing here rates mod or runtime quality; "READY" is path-completeness only. Gameplay remains a prototype; no >8 acceptance is implied.

## Files

- `CLAIM.json` — every claim with absolute-path file:line evidence, per-variant initializer note, log oracle.
- `READINESS.md` — load-path walkthrough, 7-stage verdict table, full API inventory, settings interaction, input path, UE4SS.log oracle lines.
- `API-GAP.md` — absent-API analysis (none at load), silently degraded subsystem, R3 drain blocker + fix candidates, in-game-gated items, exact error shapes.

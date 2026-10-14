# Native Tick identity and ProcessEvent admission

A private mutex-backed identity replaces the native plain thread ID/initialized flag. Only the genuine HookedEngineTick call site publishes the current non-default ID; the first publication is immutable. Later different-thread Tick callbacks return without changing identity or running Tick follow-up work. Windows keeps its original globals/publication/getter code.

All existing getter signatures remain. Checked GetGameThreadId/IsInGameThread throw while unknown. Raw getters return unknown/false without throwing; IsInGameThreadRaw uses one synchronized snapshot rather than a boolean read followed by a throwing getter. No identity lock spans engine operations, callbacks, or logging.

The native process_event_hook now returns unless the current thread equals the known Tick publisher, before the first Lua action-state or action-mutex access. It neither reads action state nor creates a worker identity on refusal. Work remains queued until a genuine Tick has published a matching thread; ProcessEvent-only fallback is not ready without that event.

Both sources were copied from the final hook-registration-errors-1 overlay, preserving its ERROR_ID cleanup/message behavior. The two patches describe only this successor's native changes. No public header, Config/context/result layout, engine mapping, or scanner policy changed. The private helper is included only by UnrealInitializer.cpp; exactly LuaMod and UnrealInitializer production objects replace their predecessors.

Host ASan/UBSan execution passes15 focused assertions with actual owned threads: unknown checked/raw behavior, concurrent readers, first/repeated publication, rejected replacement, exactly one concurrent publication winner, actual live winner ID, and only the publishing thread admitting an owned action counter. The counter is not Lua or a fabricated engine hook. No fixture establishes an actual game identity.

The source-extracted original baseline compiled under ThreadSanitizer. Its saved host sample reached __tsan::ReportRace, but Apple Clang17's error/abort reporting stalled. These abandoned executions are preserved with kill requests and are not completed TSan test receipts. No TSan pass is claimed; no further retry was attempted after the parent's direction.

native-1 records build-harness mistakes (source argument order and missing LLVM_CONFIG), without a production object. The corrected native-2 build uses the original actual SDK target profiles, final registration-error source bases and this helper include first. Both production units compile successfully. All1485 unique compiler dependency hashes match current files. llvm-nm confirms all five existing thread API definitions in the native object. Native execution/console/game access was not performed. Parent integration/linking and any actual game-thread lifecycle remain separate.

The identity covers one loaded runtime lifetime; no reset/rebind is supplied. This gate does not prove object lifetime or callback quiescence at unload, and no actual game teardown/unload execution is claimed.

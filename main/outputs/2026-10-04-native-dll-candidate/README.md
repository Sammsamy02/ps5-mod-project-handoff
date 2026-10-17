# Native DLL candidate and Lua Tick integration

A coherent owned DLL test payload now builds with the pinned PS5 SDK. The host
run executes the consumer and lazily acquired helper, exercises ten consumer
imports and six helper imports, and produces 86 and 93 through the typed Windows
calling convention. Cleanup retries preserve the first failure and never rerun
the DLL. A failed thread unregister can be retried only on its owning thread.

The PS5 adapter owns aligned RW mappings and forwards exact ranges to native
`sceKernelMprotect`, rejecting W+X requests and propagating actual denial. The
retained native wrapper forwards to the mprotect syscall; the SDK's coarse
executable protection hook is not used. Four focused host groups pass with
ASan/UBSan; the native adapter object has four expected imports. Native executable
permission remains untested.

The 168,440-byte ELF uses genuine SDK payload startup. Its 22 dynamic imports all
have declarations in the pinned SDK libraries. It is a standalone diagnostic
candidate, not a game PRX. It has not been dispatched or executed on the console.
The candidate embeds freshly compiled owned fixtures; the earlier local loader
test separately demonstrated disk-backed acquisition. No downloaded mod DLL has
run on PS5.

The UE4SS successor adds a game-thread check before Lua Tick dispatch. Its focused
regression fails on the original body and passes with the fixed body under
ASan/UBSan. The native composition retains actual activation, controller and C++
loader integration, replaces only one of 359 runtime units and keeps all 197
undefined imports unchanged. Startup/Lua success still does not establish a real
engine call; Item Spawner needs a genuine game context, correct target layouts
and real hooked game-thread execution.

Independent reviews found no remaining material findings in these changes. All
work in this slice was local. Original captures, the frozen console-proven PRX
and unrelated dirty work were preserved. The full mod-manager goal is unfinished.

Evidence: `CHECKPOINT.json`, `MEMORY-RECEIPT.json`, `CANDIDATE-RECEIPT.json`,
`TICK-RED-RECEIPT.json`, `TICK-GREEN-RECEIPT.json` and `TICK-BUILD-SUMMARY.json`.

Next: provide observable results for a bounded owned PS5 DLL execution test,
then establish the permitted genuine engine context/layout/game-thread path for
Townfall. Generic Windows DLL compatibility remains limited by the APIs and
binary assumptions each DLL actually uses; static PE TLS/TEB/FLS, unwind and
unsupported Windows thread APIs are still unavailable.

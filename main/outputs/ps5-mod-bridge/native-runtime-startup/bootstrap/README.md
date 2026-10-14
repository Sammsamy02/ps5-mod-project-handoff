# Native UE4SS bootstrap owner

This source connects an owning `NativeProgramContext` to the actual native
`UE4SSProgram` constructor and `init()`. The adapters compile with the pinned
PS5 SDK and are included in the fresh coherent relocatable runtime. The owner lifecycle is exercised with a
controlled host backend. The actual engine backend has not been executed, and
this is not a complete console module or evidence of UE4SS mod support.

## Loader integration

`PrepareDeferredUE4SSRuntime(context)` constructs the program while its paths,
settings and logging are available. It forces deferred mod startup: it does not
enumerate or run mods. The caller must supply an actual retained native module
snapshot, correct image/build facts and any genuine input provider. The
function validates the existing context contract; it cannot establish whether
the caller's image identity, permissions, addresses or Unreal ABI are true.

Keep the returned owner alive. `start()` launches the real blocking `init()` on
one owned thread and is one-shot. Status moves from `Prepared` to `Starting`;
it reaches `EngineInitialized` only through the real program's callback after
Unreal setup, property setup and Lua-function sharing. That state alone does
not certify working input, a delivered game-thread callback or mod behavior.

From a separate loader control thread, `stop()` requests cooperative shutdown
and joins the init caller before any program/context destruction. It retains
the program after joining. Repeated stop is allowed; self-join is refused;
late readiness callbacks cannot overwrite stopped/failed state. Destroying
the owner from its init thread terminates rather than detaching live code.

**Stop/join does not unhook the engine or prove safe unload.** Keep this owner
and its module loaded for the process lifetime unless the actual hook owner
has separately established callback quiescence before destruction/unload.
Cooperative startup deadlines cannot preempt an opaque engine call already
in progress; a stuck engine call can still prevent joining.

The backend preserves every reported program error. The program handles typed
`StartupCancelled` as shutdown; `StartupDeadlineExceeded` and genuine failures
remain errors, including a real failure concurrent with a stop request.

## Required source changes

The adapter uses the current source port's deferred startup policy,
`on_engine_initialized` callback, error accessor, and cooperative startup wait
control. These are in the canonical `ue4ss-unreal-native-build-1/port` tree;
their hashes are recorded with verification. Do not substitute the historical
loader-factory overlay: its `scanner_source` field is absent from this
canonical context. Do not invent a mapping provider to make init appear ready.

Native context/program class layouts changed. Rebuild **all** C++ consumers of
the changed headers with the same SDK/options before composing another full
module. Replacing only `UE4SSProgram.o` in the previously tested object would
leave old inline field offsets in other consumers. The frozen successful PRX
and its original sources remain unchanged.

## Activation-owned mod files

`MakeActivatedUE4SSModSource(activation, expected_title)` consumes a successful
Claude `mfa_activation` from the pinned backend source. It verifies the caller's
title, the Unreal engine/framework declarations, and that every UE4SS file is
preloaded. Each file view shares the activation owner, so its bytes survive the
original files becoming unavailable and the source object being destroyed.
Non-UE4SS asset files are left to their own handlers. Native binary mod roles
are explicitly refused by this Lua activation route.

`PrepareActivatedUE4SSRuntime(context, activation, expected_title)` supplies
that source to the real program, discovers the buffered Lua inventory before
`init()`, and retains it for the runtime lifetime. The program's settings,
member/vtable layout pair, signature entry scripts, Lua entry scripts and
`require` reads use retained bytes. `mods.txt` controls explicit order and
`enabled.txt` supplies deterministic additional entries. Package IDs are not
guessed to be Lua directory names. No per-mod script rewriting is performed.

An activation containing Lua scripts does not supply missing engine facts.
Both real native layout files and an owning, validated game module/scanner
context are still required. The adapter cannot discover those or make a
Windows C++ plugin runnable. `EngineInitialized` continues to mean only the
actual engine readiness callback; it is not evidence that a Lua mod started
successfully or changed gameplay. Remaining file-API limits are documented in
the current LuaMod integration report.

## Focused verification

The earlier `tests/verify.py` retains the owner lifecycle controls and historical
SDK recipe. Current source must use the reconciled include/source selection in
`.superpowers/native-dependency-spike/native-coherent-runtime-1/`; its report
records the complete fresh native compilation and relocatable composition.
The earlier owner receipts remain under
`.superpowers/native-dependency-spike/native-runtime-bootstrap-owner-1/`.

Run `python3 outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/tests/verify_activated.py`
for the actual C activation reader + owning-source adapter test and native
adapter compilation. It deletes the temporary original files, then verifies
retained bytes and ownership; wrong title/framework, unbuffered files and
native binary roles are refused. It performs no engine initialization or
console contact. Receipts are under
`.superpowers/native-dependency-spike/native-activated-mod-source-1/`.

The remaining integration requirements are a validated native game context,
engine/layout binding, actual game-thread callback delivery, and an admitted
mod-loading plan with script/file lifetimes that survive the game's sandbox.
The console-only composer can feed that plan; archive roles and module wrapper
checks do not themselves establish engine compatibility.

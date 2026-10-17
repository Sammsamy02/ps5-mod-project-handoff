# Runtime results and initializer retry

The owned DLL SDK candidate now reports its actual execution, native protection
errors and cleanup as bounded JSONL records. The native UE4SS initializer now
survives transient publication/verification failures and retires on explicit
cancellation. These are local implementation results; neither establishes a
Townfall engine call or PS5 DLL execution.

## Owned DLL candidate

The real SDK payload ELF embeds freshly compiled owned consumer/helper DLLs.
The host run returned 86 and 93 with three export calls entered and returned,
10 consumer imports, six helper imports and complete cleanup. Separate host
fault controls preserved an injected executable-protection denial and a
release failure through cleanup. The denial is a host injection, not a PS5
observation. All 58 saved commands passed; independent review matched all 178
saved input/artifact hashes without redundant test reruns.

The candidate emits four records, or five when cleanup needs a retry. It
reports actual native return values and keeps output failures separate from
runtime failures. The profile selects the output path and stdout; the default
is `/tmp/modbridge-owned-dll-candidate.jsonl`. The build never opens that target
path. The driver does not rerun a DLL while retrying cleanup.

Artifact: 185,608-byte SDK ELF, SHA-256
`6d9b0ad773faaf77fef4ed1550b65d449f1ea3cee50daddd1329e50c9fda27e5`.
It is not a PRX. PS5 executable-memory permission, DLL execution and report
delivery remain untested. No console was contacted or modified.

## UE4SS initializer

The old native callback was registered once: its executor invalidated it before
calling it, even if identity publication or FName callback retirement failed.
The new registration remains active on those failures and invalidates itself
after success. Before retrying it consults the existing weak-owner cancellation
callable and invalidates itself after shutdown. It does not recheck an expired
startup deadline.

The original callback failed the retry test; the pre-cancellation fix failed the
cancellation test. The corrected production body passed five focused scenarios
with ASan/UBSan: publication cancellation, retirement cancellation, expired weak
owner, transient retry/wrong-thread refusal and ordinary initialization. Engine
publication and FName retirement use explicit host seams; no target hook runs.

The native composition freshly rebuilt only UnrealInitializer in the 359-unit
runtime, retaining activation, controller input, native C++ integration and the
previous Lua Tick affinity guard. All 197 undefined imports are unchanged.
Artifact: 116,577,280-byte relocatable object, SHA-256
`022060284d57fe6854c2f35e839f4cec938bd3649f0697b1f1290c722816c339`.
Six compile warnings come from unchanged code/headers; link and import checks
passed. The frozen console-proven PRX and original sources remain unchanged.

Self-invalidation stops later admission, not an invocation already past the
cancellation check. It does not establish safe module unloading, drain an engine
hook or retire the separately owned FName callback.

## Next engine milestone

Lua execution proves the script runtime runs. Calling Townfall requires genuine
native module facts, validated target object layouts and real game-thread
execution. Those inputs remain unfinished. Item Spawner becomes a gameplay test
only after that connection exists. Successful fixture DLLs cover exercised APIs
and ABI paths; they do not establish arbitrary downloaded DLL compatibility.

See `CHECKPOINT.json` and the focused receipts here. Source reconstruction and
build commands are in the initializer slice README; candidate build and result
format are in `outputs/ps5-mod-bridge/windows-candidate/README.md`.

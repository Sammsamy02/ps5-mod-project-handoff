# Windows DLL module loading — 2026-10-04

A self-built Windows DLL now loads another DLL **from a file**, looks up its
function by name and ordinal, and executes it with the existing heap/TLS service
binder. This is actual **local x86_64 execution**. PS5 evidence remains object
compilation and ABI checks; no DLL or Item Spawner console success is claimed.

The consumer starts with its helper absent. Its imported `LoadLibrary` acquires
the helper's original PE bytes through the real file supplier, maps them and
returns the actual image base. Calls by name and ordinal return **86** and **93**.
The helper calls six genuine replacement heap/TLS imports. Both DLLs unload,
including a deliberately failed final deallocation followed by a successful
retry; source bytes and service sessions stay owned throughout that retry.

| Current evidence | Result |
| --- | --- |
| [DLL execution](EXECUTION-RECEIPT.json) | Eight Windows module APIs executed; helper acquired lazily from disk; six helper heap/TLS imports called; named/ordinal calls and reference counts pass. All 21 commands pass. Eight typed Win64 thunks compile to a 15,032-byte PS5 object. |
| [Native loader](LOADER-OWNER-RECEIPT.json) | Four owner groups pass normally and with ASan/UBSan: recursive dependencies, cached procedures, cycle/failure rollback, retained cleanup, pin/capacity/name admission. Direct PS5 object: 15,000 bytes. |
| [File source](FILE-SOURCE-RECEIPT.json) | Actual bounded regular-file acquisition; immutable snapshots survive later path replacement; empty/oversize/nonregular/symlink paths refused. Host and ASan/UBSan pass; native object: 2,096 bytes. |
| [Import visitor](IMPORT-VISITOR-RECEIPT.json) | Existing PE walker now publishes imports after full supported-image validation. 42 byte-semantic groups pass; malformed later structures produce no callbacks. |
| [Concurrent service sessions](CONCURRENCY-RECEIPT.json) | Real concurrent heap/TLS calls survive 1,000 added-module session lifetimes normally and under ThreadSanitizer. |
| [Heap regression](HEAP-REGRESSION-RECEIPT.json), [TLS regression](TLS-REGRESSION-RECEIPT.json) | Existing owned DLL executions still pass against current service/parser sources; no repeated unchanged broad suite. |

The new exact `KERNEL32.dll` bindings are `LoadLibraryA/W`, `LoadLibraryExA/W`,
`GetModuleHandleA/W`, `GetProcAddress`, and `FreeLibrary`. The native owner keeps
explicit load references separate from provider/import leases. Lookup gains no
load reference. Failed cleanup preserves its phase and handle for retry.
Source and service ownership retire only after the image/link are empty.

Catalog profiles supply file locations; DLL queries never become arbitrary
filesystem paths. Basenames match ASCII case with optional `.dll`; spaces are
admitted. The Windows thunks admit at most 255 ASCII characters or equivalent
UTF-16 ASCII units. Ex accepts only NULL file handles and zero flags. Main-image
lookup, forwarded/data exports, circular dependencies, arbitrary paths and
reentrant DLL entry code remain unsupported.

Independent review found and fixed two concrete defects: rollback diagnostics
could borrow freed source strings, and successful callbacks could leak changes
to Windows last-error. Focused regressions reproduced both defects before the
fixes. The earlier concurrency test likewise caught an installed-config race;
config is now immutable while any service session exists. Final source,
dependency, artifact, Win64 signature and register-preservation review is clean.

Reproduce the actual DLL pair:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_loader_execution.py
```

The existing admitted PE profile still excludes static PE TLS/TEB, FLS, unwind,
unimplemented Windows thread/services and unsupported startup requirements.
Executing these owned fixtures does not make arbitrary downloaded DLLs work.
The target memory/provider implementation, target system-import resolution and
real PS5 execution are unfinished. UE4SS still needs a genuine game-context
supplier, matching Unreal layouts and actual game-thread engine calls.

[CHECKPOINT.json](CHECKPOINT.json) pins receipt origins and current source hashes.
Copies are byte-identical to their receipts. Prior reports remain historical
snapshots; fresh heap/TLS receipts here cover the modified bridge sources. No
console contact, game/system writes or production-account actions occurred.

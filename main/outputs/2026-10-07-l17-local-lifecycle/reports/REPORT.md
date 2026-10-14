# l17 local lifecycle result

Local implementation and verification passed. All work is contained in `outputs/2026-10-07-l17-local-lifecycle/`. No network/console contact, upload, payload, launch, configuration/mod/kernel change, package, commit, push or shared checkpoint write was performed. The 29 recorded frozen l16/source/test inputs still have their original hashes.

The latest l16 console evidence remains a refusal: 64 startup sinks in 338ms, pool admission in 44ms, followed by a fresh 32-byte prologue EFAULT at `0x63d590`, consumed FD25, no constructor call and controlled mod stop while the game remained alive. l17 changes resource lifetime and thread ownership; it does not resolve or bypass that fault, prove native FName on the console, or establish descriptor headroom.

## Changes

`runtime_start_bridge_r14_diag_spanpub.cpp` unlinks each successful exclusively created startup sink immediately after opening it, while retaining its descriptor. Denied unlink is logged with path/raw error/normalized error/startup credentials and the file is retained; unlink is never retried at stop. Acquisition diagnostics now record each sink's ordinal, raw open result, normalized error and valid descriptor ID, so all 64 acquired IDs can be recovered from a future l17 capture. Existing parent-FD diagnostics remain. The bank remains 64 handles and the startup scheduling budget remains 5,000ms, separate from the unchanged 60,000ms admission budget. The startup check runs before each sink-acquisition iteration; an admitted iteration can then finish its open, required unlink attempt and diagnostics after the deadline. Neither budget preempts an in-flight OS call or mandatory cleanup. This establishes no hard wall-clock maximum for startup.

`runtime_owner.cpp` publishes the actual initializer's `std::thread::id` before its first backend operation and revokes it on worker return or exception. `CopyinStartup.hpp` protects that ID with a mutex. Foreign probe, capture, fault-query and completion calls consult the gate before touching the unsynchronized bank, ring, ledger or telemetry: probe returns unavailable, capture returns no bytes, fault query fails closed and completion does nothing. The logged initializer token is a hash of the stored actual C++ thread ID, explicitly not an OS numeric TID. Publication is by the worker, never by the startup thread. The supported startup uses the existing `std::thread` launch happens-before relationship; loader start/stop serialization and joining remain required.

`UnrealInitializer.cpp` installs once-only bank cleanup at seed-consumption entry. On success it explicitly completes immediately after pool admission, fresh signature, native constructor, pool roundtrip and binding, **before Phase5 SCO and the remaining initializer**. Early refusal, cancellation, exception and `seed_refused` unwind through the same scope. Worker-exit cleanup also covers failures before seed-check entry. Retirement closes unused sinks and the retained parent but preserves captures and the fault ledger for the rest of initialization. Fresh probes after retirement are unavailable. A descriptor is removed from ownership before its close call; a close error is logged without retrying a potentially reused number. An in-progress probe also has a local once-only close guard.

The existing bridge stop and selected owner's join ordering are unchanged. Supported stop joins the worker before fallback bank release; exceptional supplier-stop recovery first destroys/joins the owner. Explicit Control D previously read stale shared telemetry after a foreign probe refusal. A focused red test caught that defect, and only its D block now checks initializer ownership before probing or reading the mirror. A/B/C behavior is preserved; these explicit diagnostics are not called at startup.

There are four production-source deltas: bridge, initializer, ownership/cleanup header and selected owner. `CopyinSink.hpp`, all admission/FName/ABI/profile/seed units, shadow units and mod/pad bytes are preserved. Three additional unchanged dependency headers were copied from the frozen l16 overlay to keep quoted includes on one local header topology; native compilation otherwise sees duplicate equivalent `#pragma once` headers from different paths. Their provenance is in `HEADER-PATH-INPUTS.json`.

## Verification

The complete isolated host command is:

```sh
python3 outputs/2026-10-07-l17-local-lifecycle/tests/run_all.py
```

`HOST-ALL.json` records every command, exit and output. It passes all five suites and re-verifies frozen inputs:

| Suite | Fresh result |
| --- | --- |
| Actual extracted bridge plus selected RuntimeOwner lifecycle | 44/44 assertions in normal, fatal ASan/UBSan and TSan builds; no sanitizer stderr |
| Actual extracted initializer check scope, admission, signature, native call boundary, roundtrip and binding | Nine scenarios / 23 assertions each in normal and fatal ASan/UBSan builds |
| Retained l16 probe/cleanup regressions | 34/34 assertions in normal and fatal ASan/UBSan builds |
| Retained actual initializer admission integration | Eight scenarios, ASan/UBSan, all exit0 |
| Retained FName adapter and checks | Four normal/sanitizer runs, all exit0; 13 signature and seven roundtrip cases per check run |

The new cases include unlink success with live same-FD copyin/pread, denied unlink with retained files, startup FD IDs, acquisition failures at positions1/2/31/64 in POSIX/SCE/negative-errno forms, parent failure, consumed/unused FD reuse sentinels following close errors, fresh retries and 64-attempt exhaustion, startup-thread refusal, no foreign telemetry mutation, concurrent foreign callers under TSan, preserved ledger after retirement, owner completion, normal stop, injected supplier-stop exception, negative stop after destruction and actual worker exception. Stop fallback observation checks worker completion at every production fallback-release call.

Initializer cases cover success at the real Phase5 boundary, constructor-prologue EFAULT, signature mismatch, roundtrip refusal, exception at the native test boundary, admission refusal, early seed refusal, cancellation and exhausted-bank admission retries. Completion is called exactly once in every case. EFAULT makes zero native constructor calls and its ledger entry remains after descriptor retirement. The initializer fixture preserves production logic and substitutes only native boundaries: clock, Sony I/O with real host descriptors, CPU-copy source and same-ABI host constructor. Unchanged phases1/2 are excluded; early refusal is injected after the actual function-entry cleanup scope. These are host checks, not game-memory/console ABI proof.

Test-first evidence is retained: `LIFECYCLE-RED.json` has 26 expected failures per normal/sanitizer run against the copied l16 code; `INITIALIZER-LIFECYCLE-RED.json` records missing check-scope cleanup in all nine scenarios; `LIFECYCLE-CONTROL-D-RED.json` records the specific stale-telemetry failure once in each of three modes. Missing old lifecycle hooks were test-only no-op shims so the old code could run; they did not simulate the new cleanup or guard. Immutable per-run receipts are under `tests/runs/`.

## Native evidence and remaining boundary

```sh
python3 outputs/2026-10-07-l17-local-lifecycle/native/focused_compile.py
```

`NATIVE-FOCUS.json` passes six full target-TU compiles: the frozen l16 and new l17 bridge, selected owner and initializer. It records the pinned `LLVM_CONFIG`, complete argv, input/object hashes, relevant relocations and SDK exports. A three-object target relocatable join resolves all three internal hooks: owner enter/leave and initializer completion. This is a focused local join, not a final runtime link or package.

The only new SDK symbol from these TUs is `sceKernelUnlink`, with public prototype `int32_t sceKernelUnlink(const char *)`, local SDK exports in `libkernel.so`/sys/web/weak stubs and candidate NID `AUXVxWeJU-A`. New mutex/runtime-error references are already defined in the retained l16 composed object; `memcpy` and `pthread_self` are existing imports. `RETAINED-IMPORT-EVIDENCE.json` records that symbol evidence. A future final package still needs its new unlink import mapping and decoded relocation/import gates; firmware resolution and unlink policy remain console-unproved.

The first wrapper run lacked `LLVM_CONFIG` and failed rc126. That receipt is preserved in `NATIVE-ENV-FAILURE.json`. After using the exact pinned environment, the bridge found duplicate header paths; the failing receipt is retained in `NATIVE-HEADER-PATH-FAILURE.json`. Copying the unchanged related headers locally fixed topology. No SDK or frozen input was changed.

Source review: `SOURCE.diff`. Pins: `SOURCE-DELTA.json`. Frozen-input verification: `FROZEN-INPUTS-VERIFIED.json`. Final source hashes:

| Unit | SHA-256 |
| --- | --- |
| Bridge | `a695de0b938bec7fc22dabc37adf75039a3a2c64d57d9761f2f614864915b3d9` |
| Initializer | `3d8909a3a852b83f86a3feb22a2f939e852e5a1985a0e2035177011245842e53` |
| CopyinStartup.hpp | `b300cb59f41c7b552c66672439521285edbd6b0c1cbbb5ac04065cb114af7900` |
| Selected owner | `532b899133ba8f4d6d6c815aef7ad3bb7f3fa2efcb0cd5ce2d66a2f919f771c3` |

No local implementation or host/target-compile blocker remains. Final runtime/package integration, new firmware import resolution, target unlink policy and console lifecycle behavior remain unperformed and unproved. No signature/protection/fault/ABI shortcut was added.

# Independent l16 retained-handle review

2026-10-07. Local review only. No console or account contact, deployment, mod-file change, or production-source edit. Reviewed `AGENTS.md`, the recovery postmortem, current mission, approved l16 plan, l15 baseline, l16 bridge and unchanged admission/FName consumers, host fixture/receipts, and available local import evidence.

**Verdict: HOLD for one Important cleanup finding. Mac-only.** The delayed transport change itself preserves fresh kernel-copyin and exact same-descriptor positional readback. The new platform imports and restored-process use remain unproved on PS5.

## Important finding

### P2 — stop exception drops the owner without releasing the retained bank

Location: `units/runtime_start_bridge_r14_diag_spanpub.cpp:1471–1474`.

`modbridge_runtime_stop()` catches an exception from `StopRuntimeWithAck`, calls `slot.reset()`, and returns `MRS_ERR_STOP_EXN` before `bridge_copyin_directory_release()`. If owner destruction completes, the initializer has been stopped and there is no remaining runtime owner, but every unused sink and the parent descriptor remain open. l16 increases this inherited cleanup hole from one descriptor to as many as 65. The module-entry implementation subsequently clears `runtime_active` even when bridge stop returns an error, so a second `module_stop` does not call the bridge's empty-owner release path. Process exit eventually closes the descriptors; module teardown does not.

The cleanup must run after successful owner destruction/quiescence on this exception path. Preserve the rule that descriptors stay open while a live initializer can still use them. Do not simply release them before stop or while a retained owner is still running.

Test recommendation: extract the actual bridge stop function, provide a controlled `StopRuntimeWithAck` that throws while a fake owner's destruction successfully quiesces its worker, and assert that the parent and entire unused bank close once. Also cover normal stop, an already-empty owner, and stop refusal/retained-owner behavior. Current tests call `bridge_copyin_directory_release()` directly for the stop assertion and test `with_startup_cleanup`; they do not exercise `modbridge_runtime_stop()`.

## Verified strengths

- Only the bridge unit differs from the l15 `units` baseline. Admission, FName ABI/profile, fresh signature gate, and output roundtrip files remain byte-identical. The broader Lua I/O report explicitly remains investigation and does not authorize implementation.
- Startup acquires 64 exclusive `O_RDWR` sinks with mode `0600` and no target-byte reads. Partial bank acquisition closes all successfully owned descriptors. Failed/exceptional owner startup uses the existing scope cleanup. The delayed probe does no create/reopen/path lookup, consumes one descriptor, writes fresh source bytes, then reads from that descriptor with `sceKernelPread(..., 0)`.
- Signed and zero-extended SCE error patterns normalize before byte arithmetic. Zero/oversized writes, short readback, recorded EFAULT, absent bank, exhaustion and deadlines refuse. EFAULT remains a hard negative; no transferred prefix is published. All overlapping old captures are invalidated before a fresh admitted-range attempt; publication follows full requested write, exact readback, successful close and the final deadline check.
- The signature consumer explicitly requires a new successful probe immediately before comparing its owned bytes; a disk/startup cache cannot substitute. Native call and final binding keep their shared admission deadline checks.
- Normal stop joins the initializer through the owner before releasing descriptors. This supports the current single initializer caller. The exported probe/capture/control functions use shared unsynchronized state; their safety continues to depend on that serialization and absence of concurrent explicit controls. Adding debugger/console callers would require a synchronization/lifetime design first.
- Sandbox/jail observations preserve unknown/error states and do not claim UID0 proves sandbox removal. `sysctlbyname` output size/value is validated, and errno is saved/restored around credential diagnostics.

## Verification and remaining limits

At review time the bridge SHA-256 was `297fff1ca63a17bf9af497f905d646228fd6e2a5d6d7cadb2886767285957efb`, matching `receipts/HOST-GREEN.json`. Both existing `tests/plain` and `tests/san` executables were rerun successfully: 27 assertions each, exit0, `failures=0`. The sanitizer receipt specifies AddressSanitizer/UndefinedBehaviorSanitizer and `-fno-sanitize-recover=all`. This was a rerun of existing binaries, not a fresh compilation by this reviewer. Those tests establish host descriptor mechanics and refusal behavior, not Sony sandbox policy or native FName execution.

The approved resource plan distinguishes a finite bank from a preemptible wall-clock limit: 64 sink descriptors plus one parent are retained; startup acquisition has no scheduling deadline, and elapsed creation time is emitted only for complete success. The 60-second admission budget begins later. Even the delayed deadline cannot preempt a kernel call, diagnostic call or mandatory close. This matches the documented plan, but reports must not describe total startup or syscall latency as bounded to 60 seconds. A future latency test should inject delayed opens and record partial-acquisition elapsed time as well.

Local SDK symbol evidence includes all seven added names, and the importer receipt assigns the libkernel identity by extending the established `sceKernelOpen` class. The local `unistd.h` and `sys/sysctl.h` declarations support UID/GID and named sysctl types. SDK assembly stubs prove symbol presence, not function prototypes or firmware behavior. The new receipt lists public prototype URLs, but this reviewer did not find a locally saved declaration for `sceKernelPread` or `sceKernelIsInSandbox`; retain a pinned excerpt/hash or actual header and confirm their return/argument types before claiming prototype verification. Check the final package's decoded NID identities and target compilation after any cleanup edit; the full build was still being produced during this review.

Additional useful tests: close-error normalization/refusal, a pread that completes after deadline, same-range failed reprobe invalidating a previous success, bank exhaustion without stale capture reuse, unavailable sandbox result, and a startup diagnostic snapshot assertion. `reset()` currently clears the fault ledger and capture validity outside production, so it does not establish stop/start cache reset semantics. The entry lifecycle currently forbids a new module start after teardown; avoid claiming a supported in-process restart from these tests.

Console readiness remains **Mac-only / upload HOLD**. Startup file creation, delayed write/read policy through retained descriptors, the added import calls on firmware13.60, exact live prologue match, native FName behavior, unmodified mod completion, controller/menu and item spawning remain console evidence requirements under the mission's explicit per-step approval rules.

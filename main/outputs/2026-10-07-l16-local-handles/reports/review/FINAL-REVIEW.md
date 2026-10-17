# Final local l16 review

2026-10-07. This supersedes the cleanup verdict in `REVIEW.md` for the source hash below. Report-only re-review: no production edits, console/account contact, uploads, payload loads or launches.

**Verdict: no remaining Important or Critical defect found in the reviewed retained-handle change. Ready for final local build/package verification. Console status remains Mac-only; upload approval remains a separate gate.**

Reviewed bridge SHA-256: `d05a307728cee0105b721934fdfed0ba739774536b89d4ad76d7fd5d4b358f00`.

## Cleanup correction

`modbridge_runtime_stop()` now releases the bank after `slot.reset()` in its exception branch and on a returned empty owner, including a negative stop result. The original failure paths were reproduced by `HOST-STOP-RED.json`; both failed assertions now pass. The initial report's P2 finding is resolved.

The selected owner source is `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp`, as identified by the composition compile command. Its `stop()` calls the nonthrowing backend cancellation method and joins its initializer. Its destructor calls `stop()` again and terminates if stopping cannot return successfully; it does not return with a live initializer after failed destruction. The supplier calls `owner->stop()` and resets the owner before returning. Thus normal release follows worker join, and exception-path release follows successful owner destruction/join. If destruction terminates, control cannot proceed to an unsafe release.

I independently compiled this actual selected owner source with a real host worker thread and the extracted production bridge stop and supplier stop bodies. A test seam injected an exception before supplier stop, exercising the actual owner's destructor as recovery. The close adapter checked worker completion at every descriptor close. Four cases passed in both plain and fatal ASan/UBSan builds: normal stop, injected stop exception, a negative result after actual owner destruction, and empty-owner cleanup. All had no retained descriptors and zero closes before worker completion. The acknowledgement transport was an unused host shim with a null acknowledgement sink; these tests do not establish target acknowledgement I/O or engine-hook unload safety. Temporary fixtures and binaries were removed; no project test/source/receipt was changed by this reviewer.

## Fresh verification

I freshly extracted the current production probe and stop regions into a temporary directory and compiled the current fixture twice, with the same clock-only substitution used by the checked-in runner. Both plain and AddressSanitizer/UndefinedBehaviorSanitizer builds passed all **34 assertions**, compile/run exit0, `failures=0`, no stderr. The sanitizer build used `-fno-sanitize-recover=all`. The current source hash agrees with `HOST-GREEN.json`.

The additional cases cover changed live bytes at the same address, startup UID0 observation, close-error refusal without capture publication, the startup scheduling cap, and both stop cleanup regressions. `HOST-BUDGET-RED.json` records the failing startup-budget case before the cap was added. Existing partial-write, signed/zero-extended SCE fault, exact readback, deadline, range-overflow, exhaustion, startup acquisition and owner-start cleanup cases remain green.

Startup now refuses further sink acquisition after a **5,000 ms scheduling budget** and closes a partially acquired bank. It retains at most 64 single-use sinks plus one parent. The shared **60,000 ms admission budget** remains unchanged. Neither budget can preempt an in-flight OS/native call, diagnostics or mandatory cleanup; the final sink call can finish after the scheduling deadline. The cap therefore must not be described as a hard five-second wall-clock maximum for `module_start`.

## Platform evidence and retained assumptions

The locally saved `receipts/public-api/OpenOrbis-libkernel.h` hashes to `d15c49347fabdc332e1ba180c2361e7cfe22733be2d06236262210d435ed98ea`, matching `PROTOTYPES.json`. It declares four-argument `sceKernelPread(int32_t, void*, size_t, off_t)` with a 64-bit `size_t` return on the target and `int32_t sceKernelIsInSandbox(void)`. The bridge's signed 64-bit Pread declaration consumes the same x86-64 RAX bits for error normalization; offset0 removes any offset-sign ambiguity. Sandbox results outside0/1 remain unknown/error. The UID/GID and named sysctl declarations remain supported by retained SDK headers. This public PS4 declaration evidence plus PS5 SDK export evidence is ABI evidence, not a PS5 firmware runtime proof.

Single initializer serialization remains required: the handle nonce, descriptor bank, capture ring and fault ledger have no independent locking. Normal stop joins that initializer before release. Concurrent debugger/console/control use would need explicit synchronization and lifetime ownership before being added. The current change adds no such callers.

Fresh late source-copy, exact owned prologue comparison, fault ledger, full write/read checks and refusal behavior remain intact. No startup-only/disk-only capture replaces the late check. The broader Lua I/O options remain report-only; general Lua filesystem compatibility, PC parity, controller/menu and item spawning are not established by this review.

The final two-build pipeline was still running during this re-review. Its final source pins, decoded import identities, target compile/package gates and manifests must refer to the source hash above. Initial prereview artifacts do not satisfy that requirement. PS5 startup creation, post-restoration descriptor use, added import resolution, exact live prologue match and native FName behavior remain console evidence requirements under the current mission and explicit user approval rules.

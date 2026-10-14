# L13 local admission candidate

Local source and host tests only. No console contact, upload, deployment, launch, configuration change, or pad edit occurred in this admission work.

## Changes

`units/UnrealInitializer.cpp` now calls the production policy in `units/PoolAdmissionEngine.hpp` for the anchor and each validated FName entry. A single steady-clock deadline, 60,000 milliseconds from admission setup, covers metadata preparation, settings access, proof operations, retries and entry validation. Retries sleep at most the smaller of 5,000 milliseconds and the remaining budget. The l12 nested per-round budgets are removed.

For each exact needed address/length:

1. Existing source-fault evidence excludes the range immediately.
2. Exact VirtualQuery runs before capture lookup or any sink probe. Every successful query logs needed start/end, returned region start/end, protection and flags. Every byte must belong to a containing region with CPU-read protection and the committed flag. Multiple adjacent regions can cover a range; page rounding is not used to inflate the request.
3. A positive query admits that exact direct read immediately; no optional oracle cross-check is enabled.
4. An unavailable or malformed query falls through to owned capture, then a bounded copyin probe if that exact range has not already been probed. Validation consumes captured bytes; copyin never authorizes a direct heap read.
5. After unavailable VQ and copyin, the shadow fallback policy runs once when the setting permits it. It explicitly declines admission because disk-image identity supplies no live heap-read evidence. The existing whole-image shadow hash/scan has no deadline seam, so it is not started inside admission.

Successful-query protection or commitment failures retain the whole negative region in the admission fault ledger. Copyin EFAULT and prior bridge faults also exclude overlapping ranges. No fallback can override these findings. Generic VQ errors are inconclusive: a firmware-specific exact-unmapped error mapping has not been established, so the adapter does not label every nonzero API return as unmapped.

Pool metadata remains under the zero-span fail-closed rule, and each metadata word / indexed pointer slot additionally requires its entire read range to lie inside a published readable span before access. A span containing only the pool base is insufficient. Structural readiness is checked before any heap query. Pointer values and proof are tied to the exact address used by each read.

All modes preserve the user's VQ-first and memory-proof requirements. `oracle` disables the shadow diagnostic callback. `auto` and `shadow` permit that callback but cannot admit from image identity.

The final `admitted via` log is emitted only after the anchor and both required entries have independent readable-byte evidence and matching content. Mixed VQ/copyin provenance is reported accurately.

## Reproduction and verification

`tests/l12_admission_red.py` compiles source excerpts directly from the preserved l12 initializer, whose SHA256 is verified against `baseline.json`. Only the clock seam is substituted. All four regression assertions fail as expected: generic policy error falsely becomes hard-negative; a returned region that does not contain the queried address falsely proves coverage; capture is consulted before VQ; the 10-second per-round configuration takes **475,000 fake milliseconds** before fallbacks. Receipt: `admission-red.txt`.

`tests/admission_engine_test.cpp` includes the exact production header. **22 scenarios pass with AddressSanitizer and UndefinedBehaviorSanitizer**. They cover immediate VQ admission, exact non-page-aligned windows, malformed VQ responses, unavailable-to-capture fallback, negative VQ, EFAULT exclusion, prior bridge faults, shadow identity never granting a read, deadline accounting and expiry, pointer changes, overflow, later-entry independent proof, uncommitted reservations and full negative-region exclusion. Five multi-region cases additionally prove a straddling read requires both adjacent regions, unknown/non-containing second regions fall through without a direct read, and non-readable/uncommitted second regions block every fallback and exclude the whole second region. Two follow-up red receipts retain the reviewer-found commitment failure and the negative-region persistence failure before their fixes.

`tests/initializer_admission_host.py` extracts the actual native adapter and the entire Phase 3 orchestration from the candidate initializer. The native VQ/probe/capture functions, clock and already-gated metadata readers are faked; the production direct heap-read adapter reads real local fixture bytes. **9 scenarios pass**:

- Positive VQ: 3 exact-range queries, 0 sink calls.
- Oracle capture fallback: VQ first, 2 exact-range sink calls (6-byte anchor and 14-byte ByteProperty entry), shared absolute deadline of 60,000ms.
- All unavailable: 12 VQ attempts, 1 sink call, shadow explicitly declines, terminal at 60,000ms.
- Negative VQ: immediate refusal, 0 sink calls, no shadow callback.
- Previously faulted capture: excluded before VQ/capture.
- Later entry with no read protection: refused independently of the admitted anchor.
- Missing far metadata word coverage: no metadata dereference.
- Zero published spans: no metadata dereference.
- Pool not ready: 60,000ms total, no sink/oracle work.

Both host worst-case fixtures finish at **60,000ms**, below the 300,000ms outer startup deadline. `admission-source.diff` records the initializer changes and new helper; `admission-source-sha256.json` records source identities. Root owns target-object compilation/link inspection and the combined candidate receipts.

Commands from the worktree root:

```sh
python3 outputs/2026-10-07-l13-local-admission/tests/l12_admission_red.py
python3 outputs/2026-10-07-l13-local-admission/tests/initializer_admission_host.py
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Ioutputs/2026-10-07-l13-local-admission/units outputs/2026-10-07-l13-local-admission/tests/admission_engine_test.cpp -o outputs/2026-10-07-l13-local-admission/tests/admission_engine_test_asan
outputs/2026-10-07-l13-local-admission/tests/admission_engine_test_asan
```

## Evidence limits

- This is a scheduling deadline. A synchronous native syscall, log call or mandatory descriptor cleanup already in progress cannot be preempted by this helper. Checks prevent starting subsequent operations after expiry; the host fake-clock results are not a console wall-time guarantee.
- VQ success proves the observed mapping/protection/commitment at query time. It does not remove the usual query-to-read race if another thread unmaps or protects the region.
- Shadow remains identity corroboration only. There is no deadline-aware shadow provider of live heap bytes, so it cannot rescue unresolved heap readability.
- No console behavior or successful in-game admission is claimed.

Committed-flag provenance: retained `outputs/2026-10-04-zcode-team/worker-kernel-deploy/source/src/SharpProspero/Interop/Kernel/KernelMemoryQuery.cs`, lines 55–56, describes `Flags & 0x10` as memory backing rather than a reservation. That source also declares the 72-byte query layout and exact-versus-find-next behavior.

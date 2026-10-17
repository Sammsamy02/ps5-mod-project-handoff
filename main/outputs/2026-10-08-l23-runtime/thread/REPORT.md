# l23 game-thread queue self-test — Mac-only

Implemented in copied LuaMod.cpp and UE4SSProgram.cpp. No Lua mod files changed.

After the first successful Lua main script returns, a protected call to the existing registered ExecuteInGameThread API queues one no-op Lua C callback. The existing action mutex prevents concurrent drain before the queued marker. Normal mod queue ownership, reference retirement and uninstall apply. There is no new dispatcher, thread, game hook, or forced execution from the runtime update thread.

The callback checks the existing IsInGameThreadRaw guard and records scePthreadGetthreadid plus monotonic drain duration. That native import already exists in l22's admitted import set. The existing update loop polls a process-lifetime diagnostic state; once 30 seconds elapse, it logs NOT DRAINED once and continues. This is a scheduling-observed deadline, not a real-time guarantee if the update thread stalls. Late drain is explicitly labelled. Uninstall cancels the owner token before Lua destruction; the token is never dereferenced. Property reader revocation follows event-loop join, mod cleanup and input shutdown, before output teardown.

## Focused evidence

- tests/RED.log: new state test initially refused compilation because the implementation header was absent.
- tests/STATE-RESULTS.json: normal, ASan/UBSan, TSan pass. Covers queue lifecycle, cancellation, wrong-thread result, 30s boundary, late callback, one-shot timeout, backwards clock rejection, and 100 concurrent timeout/drain/cancel trials per mode.
- tests/QUEUE-RESULTS.json: 31 assertions per normal/sanitized configuration (62 total), using real probe wrappers/callback and unchanged production insertion/drain bodies with host Lua. Five scenarios: successful drain, wrong-thread refusal, cancellation, Lua API error and absent API. The full runtime API registration/selection is source-reviewed and target-compiled, not simulated as a console success. Retained host Lua C objects are not sanitizer-instrumented.
- tests/target/RESULTS.json: both actual copied translation units compile under the retained PS5 target recipes. An initial missing LLVM_CONFIG setup and then a main_lua pointer-call typo were corrected before these successful receipts. No full runtime link/package/import check is claimed.
- ../profile/REVIEW.md: independent review, no actionable findings, matching source hashes and repeat focused verification.

## Unproved

Real R3 carrier delivery, actual console native-thread identity, the 30-second console timeout, and mod menu/spawn remain unproved. This diagnostic checks consistency with the existing identity mechanism, not an independent proof of how that mechanism identified the game thread.

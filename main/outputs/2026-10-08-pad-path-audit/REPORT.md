# Selected native pad borrow audit — Mac-only

Exact selected helper/factory body is compiled with controlled class and SDK seams.
The pinned SDL PS5 API declarations and helper/factory source body are preserved;
only project includes are replaced. PAD_BORROW_DELAY_MS=0 accelerates the scheduling
gate in the fixture. This does not test PS5 GetHandle semantics, module unloading,
the full source lifecycle or actual console configuration.

## Verified observations

Five plain cases and the same five fatal ASan/UBSan cases pass. These describe the
current behavior; a PASS does not mean each behavior is desirable.

- The first open refuses borrowing during startup. A later poll starts one helper.
- A positive result returns the borrowed handle and close_owned does not close it.
- A negative result stays deferred forever: ten further retries produce no second
  GetHandle call. There is no recovery/backoff reset for that result.
- A blocked helper stays Running and prevents another helper. Releasing the controlled
  call lets it finish. There is no timeout state transition. This avoids unbounded
  parallel calls, but does not bound a stuck OS call or prove safe module unload.
- A changed user receives the prior user's cached result; the tuple is not rebound.
- After close_owned retires a borrowed handle, a further open returns the old result
  without revalidation. These are conditional lifecycle defects if replacement or
  user change occurs; they are not proof of this launch's missing-menu cause.

The source launches a detached helper without a retained join owner. The fixture
waits for completion before exit; it does not simulate unloading the native module.
Module stop must account for a live helper before any integration changes.

## Controller-to-menu implications

The input source deliberately establishes the first connected read as its baseline.
A held combo on that first sample generates no edge; release and press after the
baseline is required. Duplicate timestamps generate no event. This behavior is
covered by the separate source/handler tests and must not be confused with failure
of the Lua keybind or widget rendering.

The retry intervals count polls (200,400,800,1600,3200), not seconds. Any wall-clock
claim depends on the actual update-thread cadence. The delayed helper can finish
between polls and remain unused until the next retry. No measured console delay
is asserted here.

All discovered local PS5Pad.ini copies are2958/2960 bytes. The saved l18 loadlist
records3718 bytes, so those old files cannot prove the current L3+R3 binding. No
console read was made to resolve that uncertainty during the user's absence.

## Next integration requirement

Keep borrowed versus owned handles distinct and authenticate the user/type/index
identity; do not close a borrowed handle. Any retry/reset design must retain one
in-flight helper, avoid stale results, and define module-stop behavior for a blocked
SDK call. A fix is not selected or deployed by this audit. First combine these
findings with native callback/frame and unmodified-mod menu-path evidence.

Evidence: SOURCE-RECEIPT.json, RESULTS.json, selected_native_borrow_test.cpp.
No console contact, selected runtime/mod/config edits or deployment occurred.

# Windows address wait service

Implemented native pthread-backed WaitOnAddress, WakeByAddressSingle and WakeByAddressAll, with a separate Microsoft x64 bridge. Binding is restricted to exact names in `api-ms-win-core-synch-l1-2-0.dll` (ASCII case-insensitive library matching). Other libraries and ordinal imports remain unresolved.

The owner mutex covers acquire comparison, FIFO queue insertion and the condition wait handoff. Wake marks the oldest matching unsignaled waiter or all matching waiters and broadcasts the native condition; unrelated waiters remain queued. Waiter records live on the waiting call's stack. The owner pins the native thread domain, module sessions pin the owner, queued waits prevent close, and failed native destruction retains ownership for retry. Caller-owned PE code leases and buffers must remain alive through each call.

The admitted concurrency protocol uses naturally aligned unsigned scalar objects of 1, 2, 4 or 8 bytes. Concurrent stores use the same width with release ordering followed by wake; reads use acquire ordering. CompareAddress is a stable byte snapshot and may be unaligned. This is an explicit protocol for admitted code, not a claim that arbitrary Windows machine code obeys the C concurrency model. A successful wake does not prove the predicate changed; callers must recheck. No buffer probing or address discovery is performed.

Finite deadlines use CLOCK_MONOTONIC. The PS5 build selects a native monotonic condition attribute; Darwin host controls use relative native waits against a monotonic deadline. A differing initial value succeeds immediately. Equal zero/expired waits return FALSE and ERROR_TIMEOUT1460. Success preserves Windows LastError; invalid arguments/alignment use87 and native wait/clock failures use31 as adapter policy. Native mutex/broadcast contract failures terminate instead of proceeding without synchronization.

## Evidence

- Expected red: `d9b600c96e384121a95fe356170d5f56/receipt.json`; stub returned FALSE for an unequal 8-byte value. The old stub is preserved beside that receipt.
- Final green: `69ad1d5e568a4560a8d919265aa74232/receipt.json`; 20 recorded commands exited0 without warnings. Four focused groups passed normally, with ASan/UBSan, and with ThreadSanitizer.
- Controls cover exact BOOL/SIZE_T widths, high-bit scalar comparisons, zero/finite/infinite waits, last-error isolation, FIFO single and address-selective all wake, same-value successful wake, a deterministic compare-to-native-sleep race, 300 bidirectional handoffs, original token identity, waiter-only close refusal and native cleanup retry.
- Direct PS5 core object: 6448 bytes, SHA256 `1d3574a05885d0546c831f461237078ff948c715b8e2f9822a2d88f7c0cfd017`.
- Unedited Win64 IR to PS5 backend bridge object: 4632 bytes, SHA256 `9aa4a0ff4ccb65ba2129d69d0f17a2007fa0123bc80ecc07621fce558350d774`. Three typed Win64 definitions and nine matching native/frontend layout metrics.
- Initial `bb15980f...` assert-exit process stalled in native UE state. Its verifier was stopped and the receipt explicitly marked discarded. The bounded-exit red and all final controls completed independently; the stalled run supplies no passing evidence.

Reproduce from the worktree root:

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify_address_wait.py --sanitize --tsan
```

Native provider export declarations are checked against the pinned SDK. This service receipt proves host pthread/Win64 behavior and native object compilation, not PS5 execution or broad DLL compatibility. Parent-owned actual PE integration is separate. No existing binders, candidate profiles, console state or remote publication were changed by this slice.

## Primary contracts

Microsoft [WaitOnAddress](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress) specifies the four widths, API-set library, timeout result and caller recheck. [WakeByAddressSingle](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle) identifies the earliest waiter; [WakeByAddressAll](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddressall) covers all matching waiters. [System error codes](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1300-1699-) identifies ERROR_TIMEOUT as1460. No unverified library alias is inferred.

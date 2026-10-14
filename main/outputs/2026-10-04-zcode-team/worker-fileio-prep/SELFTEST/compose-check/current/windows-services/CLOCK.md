# Independent QPC/QPF service

`mb_win_clock.h` exposes a separate binder for exactly
`KERNEL32.dll!QueryPerformanceCounter` and `QueryPerformanceFrequency`.
Both Windows signatures are `BOOL(LARGE_INTEGER*)`: signed 32-bit return,
pointer to signed 64-bit output, Microsoft x64 calling convention.
Microsoft documents the counter in counts and frequency in counts per second.
[QPC](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter),
[QPF](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency).

The direct native provider calls `clock_gettime(CLOCK_MONOTONIC, &sample)`.
It checks seconds, nanoseconds and signed 64-bit overflow before writing output.
QPF verifies the same provider and reports the fixed representation scale
1,000,000,000 counts/second. That scale does not establish nanosecond hardware
resolution or accuracy. No wall-clock substitution or cached timestamp is used.

Failure returns BOOL 0 and preserves output. The explicit adapter policy is
Windows last-error 31 for native clock failure, 534 for malformed/out-of-range
samples, and 87 for null output. Success returns BOOL 1 and restores the caller's
previous Windows last-error. Windows CRT errno is separate.
These are adapter choices, not assertions about Windows failure behavior.
[Error names](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-),
[arithmetic overflow](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--500-999-).

Each stable session pins an existing thread domain before its EMPTY module can
load, and retains that pin through module CLEANUP until the module is EMPTY.
Calls require a thread already admitted to that domain. Session controls and
binding are serialized; first installation/final retirement require quiescent
calls. See the header for the complete lifetime contract. No existing binder,
candidate, thread owner, or heap owner has been changed to advertise this service.
File-time, process/thread identity, and other clock imports remain unresolved.

Retained import evidence is
`.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/llvm-reference.json`,
with file order documented by the adjacent `inputs.json`. Input 0 (`dwmapi.dll`)
imports QPC at IAT RVA 94312. Input 1 (`UE4SS.dll`) imports QPC at 13693464 and
QPF at 13693472. These files were inspected as data and were not executed.

The pinned SDK at `native-host/build/sdk-v0.43/ps5-payload-sdk/target/include`
declares `clock_gettime(clockid_t, struct timespec*)` in `time.h:172` and
`CLOCK_MONOTONIC=4` in `time.h:105`; `sys/_timespec.h` declares the two fields.
The actual PS5 frontend checks 64-bit `time_t`/`long`, the 16-byte timespec and
8-byte nanosecond offset. `libkernel_web.so` declares `clock_gettime` with SONAME
`libkernel_web.sprx`; the verifier records its hash and exact symbol entry.

Read-only inspection of the frozen
`.superpowers/native-observations/user-return-20261003T140907Z-3bf558aa/libkernel.sprx`
(SHA-256 `f89a698bf7a0082ec0e361cebf77e40c938196cd13d46016531f71d6ca760912`)
finds encoded export `lLMT9vJAck0` at RVA `0xfe0`, a 24-byte native syscall 232
wrapper. This supports the retained provider route; it does not establish current
target resolution, timing quality, or successful target execution.

Run the focused verifier:

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify_clock.py --sanitize
```

It executes real x86_64 Microsoft-ABI calls through the binder, using both the
real host clock and injected failures/boundary samples. Checks include maximum
signed counter values, malformed samples, unchanged outputs, saved last-error,
two-thread error isolation, exact names, copied-session refusal and pin lifetime.
The native provider compiles directly for PS5; the two Windows thunks use the
verified FreeBSD9 frontend → unedited `win64cc` IR → PS5 backend route. Receipts
include source/dependency/object hashes, target layouts and disassembly. Native
objects are compiled and inspected only. No PE DLL instructions, console actions
or game calls are performed by this verifier.

# Executable-prefix capture through the existing bridge

The existing bridge provides a suitable mechanism to attempt the first-call
live constructor prefix check without CPU data reads of executable text.
It does not prove that the attempt will succeed on the next console run.

## Existing mechanism

Source: `outputs/2026-10-07-l14-image-admission/units/runtime_start_bridge_r14_diag_spanpub.cpp`.

1. During module-facts capture, `bridge_copyin_directory_prepare()` is called
   at line 1481. Lines 922–937 retain `/data/plugins` as a directory descriptor
   before ploader restores its process namespace. Preparation opens the
   directory only; it does not create a sink or probe memory.
2. `bridge_kernel_write_copyin_probe_until(address,length,deadline)` begins
   at line 1025. It validates nonzero/overflow-safe bounded length and vetoes
   any recorded copyin fault before starting. It invalidates overlapping
   captures, honors its scheduling deadline, requires the retained parent
   descriptor, and enforces a process-wide 64-file probe budget.
3. It creates a unique game-owned sink via `_openat(parent_fd,basename,...)`
   with `O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_EXCL`, mode 0600. Generation,
   PID and nonce distinguish retained evidence files.
4. At line 1066 it passes the target address as the **kernel** write source
   to `sceKernelWrite`. Userspace does not dereference the source address.
   The loop writes page-sized bounded chunks, recognizes SCE/POSIX error
   shapes before count arithmetic, and advances on legitimate short writes.
   An EFAULT records a veto in the persistent fault ledger and returns 0.
5. Only after all requested bytes land and close succeeds does it reopen the
   same sink read-only. It reads `min(length,0x1000)` bytes into one of the
   bridge's three owned 4 KiB buffers. The exact expected count and no error
   are required before marking that slot valid and returning 1.
6. `bridge_write_copyin_capture()` at line 995 serves an interior address
   only from a valid owned slot, gives its remaining length, and vetoes
   faulted ranges. The output pointer refers to bridge memory, not game text.
7. The retained directory descriptor survives a successfully started runtime
   and is released on stop (lines 1381/1396) or failed-start cleanup (1433).

`CopyinSink.hpp` contains the error normalizer, overflow/range validator and
bounded fail-closed fault ledger. Source hashes are in `SOURCE-PINS.json`.

## Recommended gate order

Before assigning/enabling the adapter or invoking the native constructor:

1. Validate the active per-game profile and its pinned image identity,
   expected base, output shape, UTF16 input and bootstrap self-check mode 0.
   Ordinary retained constructor calls may use Find=0 or Add=1 after binding;
   Replace=2 remains rejected. Compute base+offset
   with checked addition; the expected address at base 0x400000 is 0x63d590.
2. Check the existing bridge fault ledger for the exact prefix range. Any
   existing fault is terminal for this gate.
3. Request a **fresh** `bridge_kernel_write_copyin_probe_until(ctor,32,deadline)`.
   Require return 1; return 0 means fault/refusal; return -1 means unavailable.
   Do not accept a cached capture as a substitute for this fresh successful
   request. The deadline must remain inside the startup's bounded lifecycle.
4. Fetch `bridge_write_copyin_capture(ctor,&owned,&available)`. Require return
   1, nonnull owned pointer and `available>=32`, and recheck the ledger.
5. Immediately copy/compare those 32 **owned** bytes against the profile.
   Keep the check and first call serialized during startup. The three slots
   are bounded reusable storage without a lock; never hold an accessor
   pointer across another probe or allow concurrent probes to overwrite it.
6. Only a byte-for-byte match permits the native call. Missing capture,
   short capture, SCE/POSIX IO error, closed/missing parent descriptor,
   deadline exhaustion, probe budget exhaustion, fault ledger veto or
   mismatch must fail closed before invoking native code. Preserve the
   behavioral ByteProperty-id check after the correctly shaped call.

This can be implemented as a dedicated constructor-prefix identity gate
using existing exports. It needs no new kernel feature, code write, memory
protection change or heap admission change.

## Why the pool read engine cannot serve this check

`PoolAdmissionEngine.hpp::coverage()` intentionally requires CPU READ and
committed protection. `read()` treats no CPU-read protection as a hard
negative and will not reach copyin afterward. `read_image()` additionally
requires a readable PT_LOAD and ends in `read_direct()`.

Those rules correctly govern CPU reads of pool metadata and heap entries.
They should stay unchanged. The constructor's PT_LOAD flags are PF_X only;
using those paths for the prologue would either reject it or create an unsafe
CPU-read temptation. A dedicated code-prefix gate directly asks the existing
copyin mechanism for owned bytes. A successful kernel capture permits
comparison of **the owned copy only**. It does not authorize a later direct
read of text or expand any registered readable span.

At `UnrealInitializer.cpp` lines 1005–1015 the old code classifies the ctor as
`CodeCallOnly`, assigns it, and calls the wrong two-argument wrapper. The new
gate should sit between the code/address/profile validation and first call.
The existing `L13AdmissionOps::capture()`/`probe()` at lines 843–854 show the
exact exported interfaces, but their pool engine policy need not be reused.

## Proof limits and freshness

The saved ELF prefix is the expected reference; a disk hash or a shadow-image
scan alone cannot prove the live executable bytes. A fresh kernel capture
that matches establishes those 32 live bytes at capture time. It does not
hash the entire live routine, prove the entire process image or eliminate
every possible modification between capture and call. Keep image-profile
identity/base checks and serialized startup alongside the prefix check, and
state this scope accurately.

No existing retained console evidence proves successful copyin from this
particular executable-text range. It may be unavailable under the current
kernel policy; the correct resulting behavior is refusal with a clear prefix
capture diagnostic. No fallback to disk-only identity is authorized.

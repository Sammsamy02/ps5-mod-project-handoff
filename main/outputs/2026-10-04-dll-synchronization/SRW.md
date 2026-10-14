# Native SRW locks and condition variables

The source implements eleven KERNEL32.dll synchronization imports for the
owned Windows DLL runtime. Public SRWLOCK and CONDITION_VARIABLE storage is
eight bytes with eight-byte alignment. Native mutexes, conditions, queues and
holder state live in private owner sidecars, never in Windows object storage.

The retained original import census (`windows-image-original-inputs-1/final-1/llvm-reference.json`,
image 1) actually requests AcquireSRWLockShared, AcquireSRWLockExclusive,
ReleaseSRWLockShared, ReleaseSRWLockExclusive, SleepConditionVariableSRW and
WakeAllConditionVariable from KERNEL32.dll. Initialization, try acquisition and
wake-one complete the same bounded service family; no additional DLL aliases
are inferred from those observed pairs.

The owner is explicitly opened with a thread domain and a maximum number of
simultaneously active sidecars. It pins the domain. Module sessions pin the
owner until the consumer image and all of its synchronization use retire.
Shared holders may overlap; exclusive ownership excludes both modes. Blocking
acquisition uses a native condition queue with writer preference; FIFO/fairness,
recursive acquisition and lock upgrades are not promised.

Condition registration and releasing the logical SRW mode happen under the
same owner mutex. A sleeper keeps a sidecar user reference while it owns no
mode, and reacquires its original mode before returning, including timeout.
Wake-one marks one waiter; wake-all marks all current waiters. Callers must
always loop over their protected predicate. Finite waits use CLOCK_MONOTONIC
condition attributes on the target and monotonic remaining relative waits on
the macOS host. Zero and UINT32_MAX/infinite waits are supported.

Only flags 0 (exclusive) and 1 (shared) are admitted. Timeout returns FALSE and
Windows last-error 1460. Unsupported flags produce 87; allocation/capacity
failure produces 8; native sleep setup/wait failure produces 31. The latter
three choices are explicit adapter policy. Failure preserves or reacquires a
previously held lock. Successful calls and try contention preserve last-error.
Void and try APIs fail-stop on resource/native/contract failure; no fake lock
success is supplied. Try functions return Windows BOOLEAN (8 bits), while
SleepConditionVariableSRW returns BOOL (32 bits).

Idle sidecars reset the public word to zero during the final valid call and are
retired immediately. Thus idle public storage can be copied, moved or forgotten
without a delete API, and owner close never dereferences forgotten storage.
Public objects must remain stationary while held, waiting or reacquiring.
Forged/copied active tokens, invalid memory, wrong-thread/mode release,
recursion, cancellation and nonlocal exits are outside the caller contract.
Shared ownership validity is a precondition, not a promised per-thread detector.
Final close is serialized with new API entry and refuses retained holders,
waiters and sessions. No process-shared synchronization is implemented.

## Verification

Production source is under `outputs/ps5-mod-bridge/windows-services/`:
`mb_win_srw.h`, `mb_win_srw.c`, `mb_win_srw_abi.h`, `mb_win_srw_abi.c`.
The focused real pthread/Win64 harness is `tests/test_win_srw.c`; reproduction
is `python3 outputs/ps5-mod-bridge/windows-services/verify_srw.py --sanitize --tsan`.

Final receipt: `12815e3db1b14c0f9f39355679239e91/receipt.json`, SHA-256
`f406d25f6d2a7c6159ca42f20ac9a121489147d01e172cf47728b4849722e577`.
All 17 commands exit zero: normal, ASan/UBSan and TSan executions; direct PS5
owner compilation; native/bridge layout checks; unchanged Win64 LLVM IR through
the native backend; object disassembly/import extraction and SDK declarations.
Six groups cover simultaneous readers, exclusion, three exclusive waiters with
wake-one then wake-all, shared-mode wait, zero/finite timeout and lock
reacquisition, invalid flags, capacity failure with lock retained, original
session ownership, unknown library refusal and forgotten idle storage.

The owner object is 10,976 bytes, SHA-256
`2d6ac722baa90c1da5b82292624f6bb6f52372f409ac581882d1a50bb79d07fa`.
The ABI object is 10,000 bytes, SHA-256
`a776319dd786c634c21983f13d33347233a6721ae9ec0eff7de0eedd079de39f`.
Fourteen layout metrics match the native and bridge frontends. Eleven typed
Win64 definitions include exactly two i8 returns and the ptr/ptr/i32/i32 -> i32
condition-sleep signature. Target native imports are all declared by retained
SDK providers, apart from the three explicitly linked thread-domain helpers.
Declaration and compilation do not establish target resolution or execution.

The bounded negative control in `ab387352029f4f6ea1b6b863a7471c1a/receipt.json`
replaces only owner-open with a BAD_STATE stub and exits 1 at the expected
open assertion. Early incomplete macOS runs using aborting assertions are not
evidence: crash reporting left the translated process in exit state. A separate
test harness ABI error (attribute applying only to the first comma-separated
declarator) was corrected to explicit Win64 typedefs before final verification.
Neither issue is hidden as a successful production test.

The root-owned PE candidate separately exercises these imports from real owned
Windows DLL instructions and native workers. This service receipt itself makes
no PE execution, PS5 execution, original third-party DLL, UE/game-call, static
TLS, exception/unwind or complete Windows-runtime compatibility claim. Frozen
earlier artifacts and the original captured game inputs remain unchanged.

## Primary contract references

- https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializesrwlock
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-tryacquiresrwlockexclusive
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializeconditionvariable
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw
- https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1300-1699-

# Owned DLL SDK candidate

This composes the PE loader, dependency owner, CRT/heap/TLS/module API bridges,
thread domain and native memory adapter into a real SDK payload ELF. It embeds
only freshly compiled owned consumer and helper fixture bytes,
acquired lazily through the loader source callbacks. The separate
`windows-execution/verify_loader_execution.py` check proves real disk intake.

The consumer calls eight Windows module APIs, obtains the same helper export by
name and ordinal, and expects results 86 and 93. The newly loaded helper calls
six heap/TLS imports. The candidate checks 10 consumer imports, six helper
imports, lazy acquisition, image-base handles, reference counts, cached export
leases and disposal. No game interfaces, downloaded mods or target file paths
are part of this candidate.

These counts describe the default no-entry profile. The optional entry profile
uses 16 consumer imports and eight helper imports because their DllMain
functions exercise heap, dynamic TLS and last-error services as well.

Run the local build with:

```sh
python3 outputs/ps5-mod-bridge/windows-candidate/build.py \
  --report-path /tmp/modbridge-owned-dll-candidate.jsonl
```

The builder uses the retained SDK v0.43 and LLVM 21.1.8. Native C is compiled by
the PS5 frontend; the four Windows service translation units and exact owned
export-call adapter use the existing unedited `win64cc` IR route. It checks the
entry-call adapter through that same route when the entry profile is selected.
It also checks native/bridge layouts, preserves command logs and source/dependency hashes,
links genuine SDK `crt1.o`, validates the ELF dynamic extent and maps each
undefined dynamic symbol to the actual SDK provider declarations.

The output is an AMD64 ET_DYN SDK payload ELF, with the SDK's startup and segment
permissions. It is not a PRX or a file renamed to look like one. The retained
aligned PRX and its `0x11000000` SDK-metadata correction are untouched; that
separate package/entry model is not applied to this ELF.

`pe_ps5_memory` uses dedicated RW image allocations followed by exact-range
native `sceKernelMprotect` requests, rejects W+X, and preserves ownership on
release failure. It does not use the SDK's coarse `kernel_mprotect` hook for PE
section protections. Native permission to grant EXEC is untested; failure
returns through ordinary module cleanup. This checkpoint establishes local
compilation/linkage, not PS5 DLL execution or ordinary PRX acceptance.

## Entry and ownership

`owned_runtime.h` exposes aligned native `open`, `run`, `result`, and `cleanup`
functions. One stable diagnostic instance performs one run. The result contains
stage/error codes, import/source counts and arithmetic results, without raw
addresses. Calls must be externally serialized. Fixture bytes, callbacks,
service sessions, module records and memory owner remain alive through cleanup.

The SDK driver exposes `mb_owned_ps5_start`, `mb_owned_ps5_run`,
`mb_owned_ps5_result`, `mb_owned_ps5_cleanup`, and
`mb_owned_ps5_memory_status`. Genuine SDK startup calls `main`, which initializes
and runs once, then cleans up. A persistent cleanup failure leaves the driver
waiting with a one-second sleep between cleanup attempts. It retains ownership,
preserves the first failure and never executes the DLL again. If deregistration
fails, cleanup must run on the original registered thread; another thread is
refused until that obligation retires.

The host controls execute the embedded DLL pair using local x86_64 mappings.
They cover normal completion, retained mapping-release failure with retry, and
failed native thread leave followed by wrong-thread refusal and same-thread
retry. The thread-leave fault is injected only at the host test call site;
production thread code and target objects are unchanged.

No build command dispatches an artifact, contacts a console, performs placement
or modifies target/game state.

## Optional DllMain profile

Build the entry candidate with `build.py --entry`. Its newly compiled owned
fixtures use `/entry:DllMain`; default builds retain `/noentry` and NULL entry
callbacks. `--entry --reject-attach --host-only` supplies the focused rejected
consumer-attach control. The builder verifies each entry RVA and refuses static
TLS, exception/unwind, load-config, bound/delay imports and other unsupported
directories in either profile.

Stable catalog entry callbacks delegate to the production `mb_win_call_entry`
adapter. Its typed Microsoft-x64 indirect call is compiled by the same unedited
IR route as the service bridges. Attach executes after binding/protection with
the module INITIALIZING; detach executes while STOPPING. The callback verifies
the image base, NULL reserved argument, registered calling thread, module phase
and active service session before calling the real DllMain. It records actual
BOOL results without replacing the module owner's lifecycle policy.

Each DllMain attaches once with BOOL 7, allocates a small heap token, acquires a
dynamic TLS index, stores the token there, and checks last-error services. Both
exercise exports refuse to run unless that state exists. Helper unload and
final consumer cleanup detach once, check/free their token and TLS slot, and
return FALSE (ignored for detach). The successful exercise still produces
86/93. The consumer's intentionally rejected variant returns FALSE after its
initialization; production module handling immediately calls detach, and no
export is called. Detach uses only existing imported services, with no nested
loader operation.

The entry profile keeps native thread admission until final loader cleanup has
retired every possible detach. Cleanup must run on that same thread, including
after a successful run. Failed release retains admission and service/source
owners. Wrong-thread cleanup is refused; same-thread retries do not repeat
detach or exports. Thread leave then happens before heap/domain destruction.
The no-entry profile retains its original leave-at-run-return behavior.

The JSON `entry` object adds enabled state, per-module attach/detach call counts,
their actual signed BOOL results and callback contract-error count. Array order
is consumer, helper. First-failure stage 20 means an entry observation violated
the owned fixture's callback/state contract. These are ordinary dynamic
process-attach/process-detach calls, not a Windows loader lock, CRT startup,
static TLS, thread notifications, process-exit teardown, exception support or
general downloaded-DLL admission. Target execution remains untested.

## Observable result schema

The build generates an immutable `owned_profile.h` with the report path, a
32-character build ID, and stdout enablement. The default path shown above is a
generic diagnostic profile, with no game path or shared game mailbox. The
builder records these values and their generated header hash in its receipt;
it never opens the target path. Target directory existence, writability, stdout
connection and native executable-memory permission remain untested.

`--report-path ''` selects stdout alone, and `--no-stdout` selects file alone.
Disabling both is rejected. Use an exclusively controlled regular-file path for
this diagnostic. File records append without truncating prior runs; the build
ID and per-run sequence separate records from different builds. A file grows
across runs, but each run emits only four records, or five if cleanup needs a
retry. Records are smaller than the fixed 2048-byte buffer. No record contains
an image, function or mapping address.

Each JSONL record has marker `modbridge.owned-dll` and schema `1`:

| Field | Meaning |
| --- | --- |
| `build_id`, `sequence` | Source-bound build ID and zero-based record order. |
| `phase` | 1 starting, 2 initialized, 3 run returned/skipped, 4 cleanup pending, 5 retired. |
| `execution` | Runtime stage, started/run-attempted/completed, and export calls started/returned. Three returned calls are exercise, unload and lookup; nested helper calls are not counted separately. |
| `first_failure` | First runtime failure stage and its native/Windows status, preserved through cleanup. Zero means none. |
| `counts` | Bound imports and source acquire/release counts for consumer then helper. |
| `results` | Actual values returned by the exercise export, expected `[86,93]`. `completed` distinguishes a verified run from partial/zero results. |
| `cleanup` | Current pending state and a retired flag set only after owners have closed. |
| `memory` | Memory-owner status, last actual native operation and first failed native operation. |
| `delivery` | Separate first/latest file and stdout errors plus completed record counts. |

Runtime stages are 0 none, 1 initializing, 2 ready, 3 thread admission,
4 consumer load, 5 export lookup, 6 exercise call, 7 unload call, 8 lookup call,
9 run returned, 10 cleanup and 11 retired. First-failure stages retain the
runtime's operation codes: 1 mutex init, 2 thread domain open, 3 heap open,
4 loader open, 5 consumer load, 6 unexpected eager helper, 7 export resolution,
8 exercise failure, 9 result/invariant mismatch, 10 unload failure, 11 lookup
failure, 12 thread enter, 13 thread leave, 14 loader cleanup, 15 heap cleanup,
16 domain cleanup, 17 mutex destroy, 18 memory owner and 19 cleanup-thread
mismatch.

`first_failure.module_cause` additionally retains the module owner's original
cause when a loader operation fails. In particular, 13 is ENTRY_REJECTED even
when a subsequent release failure makes the outer status CLEANUP_REQUIRED.
It is captured with the first failure and survives cleanup retries.

Memory operations are 0 none, 1 allocation, 2 protection and 3 release.
`native_result` preserves the signed 32-bit native protection return exactly;
protection `native_errno` is zero because that API returns its own error code.
POSIX allocation/release failures additionally capture their immediate errno.
`offset`, `length` and `rights` show the requested image-relative protection
range; rights bits are read=1, write=2 and execute=4. The first failed operation
survives later successful unmaps and memory-owner close. A native protection
failure before export entry therefore has zero DLL call counters and a retained
protection error, rather than being labeled a failed DLL computation. Host
tests inject a denial through the native-call test seam; they do not establish
that this error has occurred on PS5.

The driver emits before initialization, after initialization and after the
single run. If cleanup fails, its first pending record is emitted **before**
sleeping. Persistent failure keeps that record available, with no repeated
execution or continuing log growth. Only successful cleanup emits retirement.
There is no heartbeat record and a hung/crashed DLL call cannot produce a
run-returned record.

File opens use append, no-follow and nonblocking flags and admit only regular
files. Each attempt owns its descriptor, closes it once and retries neither an
uncertain close nor a failed file destination. A failed write can leave a partial
last line; complete JSON records and their delivery state are the evidence.
Close errors are reported, with process teardown as fallback if the OS kept the
descriptor open. Successful writes are not a crash-durability/fsync guarantee.

Stdout is a borrowed descriptor: regular files use write, sockets use
nonblocking send with SIGPIPE suppressed. Pipes, terminals and unsupported
descriptor types report `ENOTSUP`; no flags or global signal dispositions are
changed. `--no-stdout` is appropriate when the launcher's stdout is unsupported.
Each record permits at most 32 write calls including short writes and EINTR;
open/fstat each permit at most eight calls. These bound retries, not an
individual native syscall's I/O latency. A zero-progress write is a failure.

Reporting errors never overwrite runtime/memory first failures and never skip
cleanup. File delivery runs first; stdout's same-sequence record includes that
file outcome. A file record describes delivery state before its own write, and
stdout likewise cannot report its own final failed write. The native
`mb_owned_ps5_delivery` getter and the final nonzero return preserve that error.
If every configured destination fails, no external record can be guaranteed.

Focused host checks cover parsed JSON transitions for actual 86/93 execution,
native protection denial and retained cleanup; short writes, EINTR exhaustion,
zero/invalid progress; real append files, symlink/FIFO refusal, socket output and
closed peers; and interrupted close without accidentally closing a reused fd.


`--entry --clock` additionally composes the independently owned clock service
into each consumer/helper module session. `--clock` without `--entry` is refused.
The default and entry-only profiles retain their existing imports and binder.
The clock profile has 18 consumer imports and 10 helper imports, including exact
`KERNEL32.dll!QueryPerformanceCounter` and `QueryPerformanceFrequency` signatures
(`BOOL` int32, `LARGE_INTEGER*` int64 storage, Microsoft AMD64 calling convention).

Freshly compiled PE instructions call both functions during DllMain attach,
exported work, and DllMain detach. Each phase requires positive and nondecreasing
counter samples, the 1,000,000,000 count scale, and preservation of a nonzero
Windows last-error value. Successful attach (7), exported results (86/93), and
zero detach results are conditional on those checks. Counter history remains
inside each image, so the checks also compare across its successive phases.
The frequency describes the representation scale, not hardware resolution.
The provider is the real native `clock_gettime(CLOCK_MONOTONIC)` with checked
int64 conversion; there is no synthetic clock or wall-clock fallback.

Clock sessions begin while modules are EMPTY, before their loader lifecycle,
and end only after modules return to EMPTY. They share the existing admitted
thread domain and remain live through attach rejection and failed unmap retries.
The opt-in `clock` result member reports session begin/end counts; it is omitted
in default/entry-only records. It does not claim raw clock sample telemetry.
The host control checks the still-live consumer clock session during deferred
cleanup, same-thread detach, eventual single retirement, and the rejected-attach
path with no exported calls. Unrelated CRT and service binders are unchanged.

The host controls execute the owned DLLs locally on AMD64 (Rosetta on this host).
The SDK build retains the actual native `clock_gettime` import and validates
clock domain/session layout plus both explicit Win64 thunk definitions. Native
SDK compilation/linkage does not establish console clock execution, executable
mapping permission, game compatibility, or a playable UE4SS build. Building
this profile performs no target launch or console contact.

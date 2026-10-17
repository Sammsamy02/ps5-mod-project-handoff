# Windows x64 services

Fourteen real CRT primitives can be bound into an AMD64 PE image through the existing
`mb_pe_binder` interface. Each callable function uses the Microsoft x64 ABI and
works directly on the caller's memory. The binder itself uses the native C ABI.

| Exact DLL identity, matched without ASCII case | Exact, case-sensitive symbols |
| --- | --- |
| `VCRUNTIME140.dll` | `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `strchr`, `strrchr`, `strstr` |
| `api-ms-win-crt-string-l1-1-0.dll` | `strlen`, `strcmp`, `wcslen`, `wcscmp`, `wcsncmp`, `wcscpy` |

These pairs were selected from the retained original import evidence in
`.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/llvm-reference.json`.
Library paths, similarly named libraries, other API-set versions, different
symbol case, unlisted symbols and ordinal imports remain unresolved. DLL aliases
and API-set forwarding are not implemented. This is a primitive service slice,
not CRT or Windows compatibility coverage.

Use `mb_win_crt_bind` as the callback in `mb_pe_prepare`, with a null context.
It compares counted import views, so they need not be NUL-terminated. Only a
successful binding writes the output address. Invalid callback arguments return
`MB_PE_BIND_ERROR`; unsupported identities return `MB_PE_BIND_UNRESOLVED`.

The fourteen entry points use 64-bit native pointers and `size_t`, and 32-bit `int`.
The header rejects non-AMD64 compilation and incompatible type widths.
`memmove` supports both overlap directions; `memcpy` requires disjoint ranges.
`memcmp` and `strcmp` order unsigned bytes. `strlen` counts bytes before the first
NUL. As with these CRT functions, the caller must supply valid buffers and
terminated strings. There is no buffer validation, allocation, operating-system
state, error-state emulation or hidden service fallback.

The byte-search functions return a pointer into the original buffer or `NULL`
when absent. `memchr` converts its `int` search value to an unsigned byte and
returns the first matching byte within the 64-bit count, including bytes after
embedded NUL; count zero reads nothing. `strchr` and `strrchr` also use the low
byte of the search value and return the first or last occurrence before or at
the string's terminating NUL. `strstr` returns the first complete, case-sensitive
byte-string match, including overlapping candidates. An empty needle returns
the original input pointer, including for an empty input. These functions make
no writes or allocations. Substring search uses a direct scan with worst-case
time proportional to the product of the two string lengths.

The wide-string entry points use explicit `uint16_t` units, independent of host
`wchar_t`. `wcslen` counts unsigned 16-bit units before NUL, so a surrogate pair
counts as two and an isolated surrogate counts as one. `wcscmp` and `wcsncmp`
compare unsigned unit values; only the sign of a nonzero result is specified.
`wcsncmp` stops at its 64-bit count or either NUL and reads nothing for count zero.
`wcscpy` copies through NUL and returns the original destination pointer; the
caller supplies enough space and nonoverlapping ranges. None of these routines
decodes UTF-16, validates Unicode, normalizes text or performs locale collation.

The retained metadata above contains these exact wide imports. Image indexes
are zero-based positions in its JSON array; all five records are named imports
from `api-ms-win-crt-string-l1-1-0.dll`:

| Image index | Symbol | IAT RVA | Hint |
| --- | --- | --- | --- |
| 0 | `wcslen` | `0x17488` | 163 |
| 1 | `wcslen` | `0xd10308` | 163 |
| 1 | `wcsncmp` | `0xd10340` | 166 |
| 1 | `wcscmp` | `0xd103e8` | 158 |
| 1 | `wcscpy` | `0xd103f8` | 160 |

Metadata SHA-256:
`95d365cdb93e057e53fe44180965f207bb55520c65a996f6673702ae4f2f55e0`.
These imports motivate this bounded slice; resolving them alone does not make
either original image executable.

The same retained metadata contains all four byte-search imports as named
imports from `VCRUNTIME140.dll` in image index 1:

| Symbol | IAT RVA | Hint |
| --- | --- | --- |
| `memchr` | `0xd0fe90` | 58 |
| `strstr` | `0xd0fee8` | 66 |
| `strchr` | `0xd0fef0` | 64 |
| `strrchr` | `0xd0ff20` | 65 |

## Building and checking

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify.py
```

The script uses the retained LLVM 21.1.8 and v0.43 payload SDK. It downloads
nothing. `--host-only` runs local checks; `--host-cc`, `--sdk`, `--llvm-bin` and
`--output` select existing local tools or a receipt destination. On macOS it
builds x86_64 and invokes the tests through `/usr/bin/arch -x86_64`; Apple Silicon
therefore needs an existing Rosetta installation.

The host tests resolve and call all fourteen real addresses through explicit
Microsoft-ABI function pointers, checking return pointers, untouched boundary
bytes, zero counts, byte ordering, both overlap directions and exact matching.
A small assembly caller checks preservation of all Microsoft nonvolatile general
registers and all 128 bits of XMM6–XMM15. The tests also exercise larger vectorized
copy/move/fill ranges. Wide-string checks cover unsigned values above `0x7fff`,
surrogates, prefix ordering, a comparison count above 32 bits, copying the NUL,
and guarded memory boundaries. Guard pages detect reads past a terminator or
bound, writes past the copied NUL, and reads with count zero. Byte-search checks
cover low-byte conversion, embedded NUL, first/last matches, original returned
pointers, absent matches, overlapping substring candidates, empty needles and
guarded buffer/string endpoints. An ARM64 compile must fail at the explicit
architecture guard. This is nineteen focused test
groups, without downloading or executing DLLs.

### PS5 frontend restriction and IR build

With the retained compiler, direct `prospero-clang -c mb_win_crt.c` reports
`'ms_abi' calling convention is not supported for this target`. LLVM 21.1.8's
[`PSOSTargetInfo::checkCallingConvention`](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/clang/lib/Basic/Targets/OSTargets.h)
accepts only the native C convention in its PS4/PS5 **C frontend**. Erasing
`ms_abi` would compile functions with the wrong incoming argument registers.

The reproducible target build therefore has two explicit stages:

1. Compile the same freestanding C with the retained compiler and SDK headers,
   selecting `x86_64-unknown-freebsd9`, `-fshort-wchar`, `-march=x86-64`,
   `-fno-builtin`, and `-fPIC`, to LLVM IR. All fourteen service definitions must retain
   `win64cc`; the binder keeps the ordinary C convention.
2. Feed that **unedited IR** into `prospero-clang -x ir` for the
   `x86_64-sie-ps5` backend. The intentional target override is explicit.

Before stage two, the script compares the complete LLVM data-layout string and
18 scalar, alignment and import-structure layout facts against a direct PS5 C
probe. It also compiles a native C binder consumer using `mb_win_crt_bind.h`.
The final object must be little-endian AMD64 ELF `ET_REL` with no undefined
symbols. Receipts include source/dependency hashes, exact commands, the direct
C frontend diagnostic, unedited IR, target disassembly and ELF metadata.

The current object is 5,888 bytes. Disassembly confirms RCX/RDX/R8 argument use;
the move implementation preserves RDI/RSI when using them, and the remaining
service functions do not touch these registers or XMM6–XMM15. The wide routines
use 16-bit loads/stores, unsigned extension and a 64-bit bound; `wcsncmp` checks
the bound before its first load. The byte searches compare bytes using DL or
byte loads, keep the `memchr` bound in R8, and return original pointers in RAX.
All fourteen routines are leaf functions in this
build. The host canary tests validate host code;
the PS5 object has been compiled and inspected, **not executed on a PS5**.

Native PS5 code includes `mb_win_crt_bind.h` and links the generated object. Code
that directly calls the Microsoft-ABI services must use an ABI-capable compiler
path and `mb_win_crt.h`. A native C declaration with its ABI attribute omitted is
not a supported direct caller. `-ffreestanding -fno-builtin` prevents these
implementations from being rewritten into external CRT calls.

Generated artifacts live under ignored `build/`. Ship the source and verifier,
not local test executables, IR or objects. Binding the stateless services alone does not supply
PE entry/TLS sequencing, SEH/unwind support, allocation, handles, thread state,
loader services, C++ runtime behavior or engine bindings. The PE preparer's
execution-readiness boundary remains the caller's responsibility.

## Owned allocation and per-thread errno

`mb_win_heap` implements native-backed, 16-byte-aligned `malloc`, `calloc`,
`realloc` and `free`. Allocation overflow/failure sets Windows `ENOMEM` (12);
failed realloc preserves the original allocation and its bytes. Successful
operations preserve errno. Live allocations and consumer pins prevent owner
cleanup. Native heap access is externally serialized.

`mb_win_threads` supplies a separate Windows errno slot for each explicitly
registered native thread using pthread storage. Registration must succeed before
that thread calls DLL code. Lookup does not allocate; explicit leave or a native
thread-exit destructor releases registration. Close waits for all registrations
to retire, with workers joined and calls quiescent. The same domain now also
provides the dynamic TLS services described below. It does not create Windows
threads or implement FLS or PE static TLS.

`mb_win_heap_bridge` connects those owners to five exact import pairs:

| DLL | Symbols |
| --- | --- |
| `api-ms-win-crt-heap-l1-1-0.dll` | `malloc`, `calloc`, `realloc`, `free` |
| `api-ms-win-crt-runtime-l1-1-0.dll` | `_errno` |

Its public header uses the native C ABI; private import functions use Win64 ABI.
`mb_win_process_bind` combines these with the fourteen stateless services above.
Begin a stable bridge session for the EMPTY consumer module before opening it,
pass that session as binder context, and retain it through module cleanup. End
the session only once the module is EMPTY. The last session cannot end with live
allocations. Multiple sessions share one identical process-wide configuration;
control operations require all imported calls to be quiescent. Imported calls
serialize through the supplied lock. The native domain, heap and callback code
must outlive every session and registered thread.

Focused checks:

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify_heap.py
python3 outputs/ps5-mod-bridge/windows-services/verify_threads.py
python3 outputs/ps5-mod-bridge/windows-execution/verify_heap_execution.py
```

The owned DLL actually calls all five imports plus `strlen` locally, including
failed realloc preservation, independent errno on native threads, and retained
ownership during cleanup failures. Heap and thread owners compile directly for
PS5; the bridge follows the unedited Win64 IR route and checks 29 native layout
facts. These are local execution and target compilation results only. The SDK
declares the thread adapter's twelve imports; their resolution inside a game
has not been tested. See `outputs/2026-10-04-windows-heap-process/` for receipts.

## Dynamic TLS and Windows last-error

`mb_win_threads` keeps a separate unsigned 32-bit Windows last-error value for
each registered native thread. Its dynamic TLS table has 1,088 indices shared
by the domain, with separate pointer values for every registered thread.
Allocating an index initializes its value to NULL on all existing threads;
future threads also start with NULL values. Reallocating a freed index clears
its previous values. Freeing the index never frees caller-owned pointees.

`mb_win_tls_bridge` exposes six exact `KERNEL32.dll` imports: `GetLastError`,
`SetLastError`, `TlsAlloc`, `TlsFree`, `TlsGetValue`, and `TlsSetValue`.
`TlsGetValue` clears last-error on success, including a NULL value. The other
successful TLS operations preserve it. Get/Set support the documented minimum
index range 0–63 even without an allocation; higher indices require allocation.
The adapter reports error 8 on exhaustion and error 87 on invalid-index failures.
These failure codes are explicit adapter policy, not a claim that every Windows
release uses the same failure codes for these APIs. Windows errno and host errno
remain separate from Windows last-error.

Begin a stationary TLS bridge session before opening its consumer module. It
pins the domain even when no thread is currently registered, and retires only
after the module reaches EMPTY. The application must release its allocated TLS
indices; final domain close refuses outstanding indices, sessions or threads.
Every thread must enter the domain before making Windows calls. TLS operations
lock internally, so callers must not hold the domain mutex around them.

`mb_win_runtime_bind_context` pairs original TLS and heap sessions for the same
module. `mb_win_runtime_bind` composes all twenty-five current exact bindings.
It validates session ownership, tries the existing process binder, and falls
through to TLS only for an unresolved import. Errors never become fallback
success. Use the same native domain for the heap's errno/locking callbacks.

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify_tls.py
python3 outputs/ps5-mod-bridge/windows-execution/verify_tls_execution.py
```

This dynamic TLS API does not supply the Windows thread environment block,
static PE TLS data/callbacks, FLS/fibers, thread creation or loader notifications.
Those remain independent requirements. In particular, the retained Windows
UE4SS DLL contains a static TLS directory and is not made runnable by these six
imports alone. See `outputs/2026-10-04-windows-tls-last-error/` for this slice's
scope and evidence.

## DLL entry-call bridge

`mb_win_entry.h` exposes this ordinary native C callback, including to the direct
PS5 C frontend:

```c
int32_t mb_win_call_entry(void *unused, void *entry_address, void *image_base,
    uint32_t reason, void *reserved);
```

It invokes exactly one function with the Microsoft x64 signature
`int32_t (void *image_base, uint32_t reason, void *reserved)` and returns its
32-bit result unchanged. The implementation uses a private `ms_abi` function
pointer; that attribute is not exposed by the public native header. The caller
must provide a valid nonnull executable entry address and keep the image mapped
through the call. There is no exception handling in this bridge.

Microsoft specifies DLL entry as `BOOL WINAPI (HINSTANCE, DWORD, LPVOID)`:
`BOOL` is a 32-bit integer, `DWORD` is an unsigned 32-bit integer and the module
handle is the loaded image base. Dynamic process attach uses reason `1` and a
null reserved pointer; dynamic detach uses reason `0` and a null reserved
pointer. Selecting those values, interpreting attach success as any nonzero
result, ignoring detach's result and sequencing lifecycle calls belong to the
caller. The bridge does not normalize the return or impose a reason policy.
See [Microsoft's DLL entry contract](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)
and [Windows data types](https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types).

```sh
python3 outputs/ps5-mod-bridge/windows-services/verify_entry.py
```

This separate focused check performs six real native-to-Windows ABI calls on the
host, validating all three arguments, single invocation and unchanged results
including zero, negative values, `INT32_MIN` and `INT32_MAX`. Additional reason
values stress transport of all 32 bits; they do not extend DLL lifecycle support.
It also checks ARM64 rejection on macOS and direct PS5 compilation of a consumer
of the native header. It does not rerun the CRT service suite.

The same two-stage target route described above compiles `mb_win_entry.c` to
unedited LLVM IR, then uses the PS5 backend. Full data-layout strings and six
scalar width/alignment facts must agree. The verifier requires the exported
bridge to keep the native C convention and make exactly one indirect `win64cc`
call with the expected three arguments in order. The current target object is
880 bytes with no undefined symbols. Its disassembly reserves 32 bytes of shadow
space, passes base/reason/reserved in RCX/EDX/R8 and returns EAX unchanged. This
is a target compile and inspection, without executing PS5 code. Generated
receipts use the `build/entry-*` prefix.

## Semantics and ABI references

- [Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)
- [Microsoft `memcpy`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/memcpy-wmemcpy)
- [Microsoft `memmove`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/memmove-wmemmove)
- [Microsoft `memset`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/memset-wmemset)
- [Microsoft `memcmp`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/memcmp-wmemcmp)
- [Microsoft `strlen`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strlen-wcslen-mbslen-mbslen-l-mbstrlen-mbstrlen-l)
- [Microsoft `strcmp`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strcmp-wcscmp-mbscmp)

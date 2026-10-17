# Callable PE module adapter

`pe_module.c` adds admission, memory protection, optional entry initialization,
callable export lookup and unload
ownership to the portable `windows-image` preparer. Memory operations belong to
the caller; the production adapter contains no operating-system calls. The
separate local test builds a Windows DLL from `tests/call_fixture.c`, loads it
through this adapter, calls its exports and genuine imported CRT implementations,
then unloads it.

The verified execution result is a local x86_64 Mac process, including Rosetta on
Apple Silicon. The adapter also compiles directly as a PS5-target C object. A
target object compile does not establish PS5 execution. This component supplies
no console dispatcher, process integration or general Windows runtime.

## Supported execution profile

The PE preparer validates AMD64 PE32+ DLL structure. `mb_pe_module_open` additionally
requires a zero entry point. `mb_pe_module_open_with_entry` explicitly permits an
entry point in an executable section. Both reject TLS, exception, load-configuration,
delay-import, bound-import, CLR and other unsupported directories. Its closed
directory allowlist is export, import, resource, certificate, base relocation,
debug and IAT. Resource, certificate and debug content remain inert metadata;
their presence adds no Windows services or signature verification.

Unsupported image characteristics, including driver, AppContainer, forced
integrity and CFG requirements, are refused. Section sharing, cache and paging
requirements are also refused. The optional entry profile delivers dynamic-load
process attach/detach through a caller-supplied ABI bridge. It supplies no separate
CRT startup, constructors, TLS allocation/callbacks, Windows exception registration
or thread notifications. Absence of these headers cannot prove that arbitrary machine code
has no other Windows dependencies; the executed fixture has known source and
deliberate compiler/linker settings.

Every import must resolve to a nonzero address. The caller is responsible for
correct implementations and the Microsoft x64 ABI. The companion
[`windows-services`](../windows-services/) module provides fourteen real byte/string
CRT implementations used by the fixtures, including four explicit 16-bit Windows
wide-string operations and four byte-search functions. Unresolved imports stop loading before
any executable permission is installed. Forwarded exports and exports outside
executable sections cannot be acquired as functions; no dependency search or
forwarder resolution occurs.

## Memory and call lifetime

Include `pe_module.h` and compile `pe_module.c` together with
`windows-image/pe_image.c`. Supply `mb_pe_memory_ops` with a protection granule and
four callbacks:

- `allocate_rw(context, size, alignment)` returns dedicated writable,
  non-executable memory of the requested size and alignment.
- `protect(context, base, offset, length, rights)` applies read/write/execute
  rights to exact aligned spans. Zero rights means inaccessible.
- `flush_instruction_cache(context, base, length)` makes the prepared
  instructions visible before an export is published.
- `release(context, base, length)` releases the allocation. All integer callbacks
  return zero on success. Failure must retain the entire allocation for retry.

The adapter inspects and plans before allocating. It prepares at the actual
allocation address, then finalizes permissions and synchronizes the instruction
cache. It combines all section rights sharing each caller granule and rejects
any resulting writable/executable page. Headers are readable; gaps occupying
whole granules are inaccessible. Partial pages inherit the combined rights of
their contents. Allocation size is rounded to the protection granule and
alignment is the larger of that granule and PE section alignment.

Zero-initialize a `mb_pe_module` and each `mb_pe_export_lease`. These objects must
remain at stable addresses and must not be copied. The normal sequence is:

1. `mb_pe_module_open` admits, prepares and protects the image.
2. `mb_pe_module_acquire_export` or `mb_pe_module_acquire_ordinal` returns a lease
   whose `address` belongs to an executable section.
3. Call that address through the exact Microsoft x64 function-pointer type.
4. Keep the lease until the call and all retained callbacks/work have finished,
   then call `mb_pe_module_release`.
5. `mb_pe_module_close` releases the image after all leases are released.

The adapter does not infer export function signatures. On
non-Windows x86_64 Clang, the callable typedef needs `__attribute__((ms_abi))`.
Ordinary host function pointers must not be installed as Windows imports unless
their convention already matches. The actual fixture exercises both imported
functions and an export mixing integer and floating arguments beyond the four
register argument positions.

The original PE file stays borrowed, immutable and alive until close succeeds;
export lookup uses it. Imported code, data and dependencies must also outlive
the loaded module. Access must be single-threaded or externally synchronized.
Callbacks may not reenter the adapter, mutate its objects, or escape through an
exception or `longjmp`; neither may exceptions unwind across a PE call boundary.

Close returns `MB_PE_MODULE_BUSY` while any lease remains. Lease identity checks
prevent a copied lease from releasing the original pin. Any failed load after
allocation attempts cleanup. If release fails, the module retains its allocation
and memory callbacks in `MB_PE_MODULE_CLEANUP` and returns
`MB_PE_MODULE_CLEANUP_REQUIRED`; the error preserves the original cause. No export
can be acquired in that state. Retry `mb_pe_module_close` to finish cleanup.

## Optional process attach/detach

`mb_pe_module_open_with_entry(module, file, size, memory_ops, entry_ops, binder,
binder_context, error)` accepts a nonnull `mb_pe_entry_ops` containing a native C
callback and its context. The callback receives the entry address, image base,
reason and reserved pointer. It calls the entry using the Microsoft x64
`BOOL(HINSTANCE, DWORD, LPVOID)` convention and returns the actual signed 32-bit
Windows BOOL. The caller retains the callback, its context and its dependencies
until close succeeds; the ops struct itself is copied.

After all imports, protections and instruction-cache synchronization succeed,
the module enters `MB_PE_MODULE_INITIALIZING`. It invokes process attach with
reason `1`, image base and `NULL`. A nonzero BOOL publishes the module as ACTIVE.
A zero BOOL invokes process detach with reason `0`, image base and `NULL`, then
cleans up and reports `MB_PE_MODULE_ENTRY_REJECTED`. Detach return values are
ignored. These are the documented dynamic-load entry return semantics.
[Microsoft DllMain reference](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)

Close first checks that no leases remain. It enters `MB_PE_MODULE_STOPPING`,
invokes detach once, then attempts to release the allocation. If release fails,
the module retains cleanup ownership; later close attempts only retry release.
Rejected attachment and its diagnostic likewise survive repeated cleanup
failures. A zero-entry image is accepted by the optional API but invokes nothing.
The original `mb_pe_module_open` still rejects every nonzero entry point.

Entry code and its bridge must return synchronously and normally. They may not
reenter the loader, create asynchronous image work, or leave escaping callbacks
or image pointers; no lease is available during initialization. Exceptions and
`longjmp` may not cross the entry boundary. The caller must serialize entry calls
across every module in its loader domain. This provides no Windows loader lock,
thread attach/detach, static-load or process-termination behavior.

## Imported functions from provider DLLs

`pe_dependencies.h` provides `mb_pe_link`, which embeds a consumer module and
retains caller-owned export leases for its provider imports. Register already
ACTIVE providers with counted DLL names and pass zeroed pin storage large enough
for the inspected import count. The registry's DLL keys match ASCII case
insensitively; exported names remain case-sensitive, and imports by ordinal use
the existing ordinal lookup. Duplicate keys are rejected.

`mb_pe_link_open` prepares the consumer and acquires a provider lease before
publishing each imported address. A registered library's missing, forwarded or
non-executable export remains unresolved. The system binder only handles
libraries absent from that registry. The registry itself is needed during open;
provider objects, files, images and pin storage must remain stable until close
finishes. The wrapper has no game-specific rules.

Acquire consumer exports through `&link.module`. Close the consumer through
`mb_pe_link_close`, never by closing its embedded module directly. BUSY close and
failed consumer deallocation retain all provider pins. Consumer attach and detach
likewise execute while those pins remain held. After consumer cleanup succeeds,
the pins retire in reverse order. A pin-retirement failure keeps wrapper
ownership for retry even though the consumer image is already gone.

This owner does no file discovery or provider loading, circular dependency
resolution, data/forwarded export handling, or Windows module-lookup API emulation.
The caller serializes registry/module access and follows the stable, disjoint
storage contracts in the header. Existing entry/ABI/image restrictions apply.

## Lazy catalog loader and file sources

`pe_loader.h` adds lazy loading over the module and dependency owners. Supply up
to 64 declared catalog entries, stable zeroed runtime records, per-entry import
and procedure lease arrays, shared memory callbacks, and source callbacks. Each
entry can select the existing no-entry or explicit entry profile. Catalog identity
uses case-insensitive ASCII basenames, including spaces; omitted extensions gain
`.dll`, and one trailing dot suppresses that addition. Normalized duplicates are
rejected. Paths, executable/NULL-name lookup and filesystem search are unsupported.

`mb_pe_loader_load` acquires immutable source bytes only when first needed, uses
the validated import visitor to open declared providers recursively, then opens
the consumer through `mb_pe_link`. Cycles are rejected. Registered providers own
their imports; missing or unsupported exports cannot fall through to a system
binder. Service hooks begin with an EMPTY module and can install a composed
binder and stable service session before module open. Designated loader-pin
operations are allowed from those hooks; other loader reentry, including from
entry code, remains forbidden.

A successful load gains one explicit reference and returns the actual image base.
`mb_pe_loader_find` gains none. Name/ordinal procedure lookup returns only an
executable, nonforwarded export and caches a stable lease without gaining another
load reference; repeated addresses share one cache entry. Callers retain a load
reference or dependency lifetime through every call and borrowed address. Handles
are compared with owned records before use, so arbitrary pointers are never
dereferenced as modules.

Final free retires cached procedure leases, closes the module/link, ends services,
then releases the source token. Actual dependency leases keep ref-zero providers
mapped until their consumers retire. Failed cleanup retains its phase and tokens
for `mb_pe_loader_free` or `mb_pe_loader_collect` retry, without consuming the same
reference twice or repeating completed callbacks. Failed-load rollback likewise
preserves providers while consumer cleanup remains pending. New loads are refused
while any record needs cleanup, preventing a retained cleanup handle from aliasing
a reused allocation. Returned diagnostics retain scalar import/IAT information
and clear borrowed string views that could outlive released source bytes.

`pe_file_source.h` supplies real native file acquisition for these source callbacks.
Set an entry's `source_context` to a stable `mb_pe_file_source_profile` containing
a trusted path and positive byte limit, at most `MB_PE_MAX_IMAGE_SIZE`. Acquisition
admits only a nonempty regular file, refuses a symlink in the final path component,
and returns an independent immutable allocation. The caller keeps the source
unchanged during acquisition; later replacement does not change the snapshot.
The descriptor closes during acquisition, while the snapshot remains owned until
release. This adapter does not search DLL names or establish PE readiness.

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_loader.py --sanitize
python3 outputs/ps5-mod-bridge/windows-execution/verify_file_source.py --sanitize
```

The loader verifier uses actual PE parsing and ownership with deterministic native
callbacks: it covers lazy provider loading, reference/procedure counts, cycles,
rollback, and retained memory/service/source cleanup failures. It executes no PE
instructions. File-source checks use real host files and sanitizer runs. Both
components compile directly as PS5 C objects; this does not resolve their native
imports in a game or prove target execution. Separate owned-DLL fixtures provide
local x86_64 execution evidence. PE static TLS, unwind/SEH, forwarded/data exports,
loader entry reentrancy and native executable-memory integration remain outside
this slice.

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_loader_execution.py
```

This separate local x86_64 fixture executes eight Windows module APIs from an
owned consumer, lazily acquires a helper DLL from a real file, calls named and
ordinal exports (results 86 and 93), and exercises six heap/TLS imports inside
the newly loaded helper. It retains all service sessions and source ownership
through a failed final release, then retries and unloads everything. Its PS5
bridge artifact remains compile/ABI evidence only.

## Process heap services

`../windows-services/mb_win_heap_bridge.h` provides a pinned service session for
a consumer, with allocation and per-thread Windows errno imports. Its combined
`mb_win_process_bind` also supplies the existing stateless CRT functions. Start
the session before module open and retire it after module cleanup; preserve it
across BUSY and failed release. The thread domain and heap must outlive sessions,
and each native thread must register its errno slot before calling DLL code.
See the services README and bridge header for synchronization and ownership.

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_heap_execution.py
```

This compiles and executes an owned DLL using the real allocation services,
checks failed realloc preserves the old allocation, tests distinct native
thread errno through the DLL, and exercises session retention through cleanup.
The corresponding PS5 bridge object is compiled and inspected, not executed on
the console. No downloaded mod DLL or game is exercised by this check.

To add `KERNEL32.dll` last-error and dynamic TLS imports, begin a TLS bridge
session for the same module and use `mb_win_runtime_bind` with both sessions.
The native thread domain is pinned through cleanup. The application owns its
TLS indices and pointed-to objects, and each native caller must enter that
domain before invoking the DLL. Static PE TLS and Windows thread creation
remain outside the admitted execution profile.

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_tls_execution.py
```

## Reproduce checks

Run portable admission/protection/lease tests, sanitizers and the direct target
C compilation:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_module.py --sanitize
```

Run the local x86_64 execution test of the freshly compiled fixture:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_execution.py
```

Run actual process-entry success, rejection, unload and cleanup-retry cases:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_entry_execution.py
```

The entry verifier builds two owned Windows DLLs with the same source and an
explicit `Entry` routine: one accepts attach, one rejects it. It uses the real
`mb_win_call_entry` bridge, calls a leased `Status` export while active, and
reads the fixture's exported trace from its own allocation before release.
Four cases cover both DLLs with normal release and a single failed release.
They check image-base relocation, attach/detach arguments and order, rejected
startup cleanup, refusal to close with a live lease, and exactly one detach even
when release needs retry. These DLLs use no CRT startup, TLS or Windows threads.
The trace is data, never acquired as an executable export.

Execute the four observed wide-string imports through another owned DLL:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_wide_execution.py
```

This fixture tests unsigned code-unit ordering, surrogate pairs, copy terminators,
prefixes, first/last code units and bounded comparisons against an inaccessible
page. All four imports resolve through the production binder; the result reports
the exact copied units and unload. It does not use the host's `wchar_t` functions.

Run actual byte-search imports, then provider/consumer dependency tests:

```sh
python3 outputs/ps5-mod-bridge/windows-execution/verify_search_execution.py
python3 outputs/ps5-mod-bridge/windows-execution/verify_dependency_owner.py
python3 outputs/ps5-mod-bridge/windows-execution/verify_dependencies.py
```

The dependency execution test compiles an owned provider and two consumer DLLs.
One consumer calls the provider through a real IAT binding; another binds that
provider and then encounters a missing system function. The harness checks
provider pinning during calls, BUSY close, successful and failed deallocation,
and immediate/deferred failed-load rollback. These remain local x86_64 tests.

The first command covers unsupported requirements, unresolved/zero imports,
incompatible page sharing, failed protection/cache operations, retained cleanup
ownership, copied leases and busy unload. Its caller memory implementation uses
ordinary data storage and executes no PE instructions. It also checks attach
success/rejection, initialization ordering, busy close before detach, ignored
detach return values and exactly-once detach through cleanup retries. The same production
adapter translation unit is compiled by the retained `prospero-clang`; the
result must be AMD64 ELF `ET_REL` with only the PE preparation API and memory
comparison/copy/clear dependencies.

The second command builds a `/noentry /nodefaultlib` PE with 16 KiB section
alignment, disabled builtins, stack protector and unwind tables. Its x86_64 host
runner uses `mmap`/`mprotect`, actual CRT bindings and Microsoft-ABI function
types. Success reports `DLL->native:PortBridge`, mixed-argument result `103.5`,
six imported services called and a completed unload. It never runs a downloaded
DLL or contacts a console.

The verification scripts create unique receipt directories under
`.superpowers/native-dependency-spike/`, recording commands, outputs and source
hashes. `verify_module.py` also records target compiler, dependencies and object
hashes. Default compiler paths refer to existing retained tools; command-line
options can select other existing paths. `--host-only` skips the target compile
in the adapter verifier. No tool is downloaded by these scripts.

Format and ABI references:
[Microsoft PE/COFF](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format),
[Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention),
and [Clang `ms_abi`](https://clang.llvm.org/docs/AttributeReference.html#ms-abi).

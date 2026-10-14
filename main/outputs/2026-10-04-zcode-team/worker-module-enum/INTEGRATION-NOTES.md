# Module enumeration family — integration notes (10-point wiring)

Worker: worker-module-enum, 2026-10-04. New family members:
`mb_win_module_enum.{h,c}` (owner: module table, handle/name lookup,
GetProcAddress, LoadLibrary/FreeLibrary refcount, enumeration, MODULEINFO) and
`mb_win_module_enum_abi.{h,c}` (14 Win64 thunks + kernel32 binder). All
proposal-only until a controller integrates.

1. **Promote the four sources + test** into
   `outputs/ps5-mod-bridge/windows-services/` (pattern-identical residents,
   same as the crt-startup/mutex integration). My copies under
   `outputs/2026-10-04-zcode-team/worker-module-enum/sources|tests` exist so
   history stays intact. Add the two `-c` units to build.py source lists:
   `native_c.append(SERV/'mb_win_module_enum.c')`;
   `abi_c.append(SERV/'mb_win_module_enum_abi.c')`; add `--module-enum` guard
   beside `--crt-startup` and `#define MB_OWNED_MODULE_ENUM` in the generated
   `owned_profile.h`.

2. **Bind order.** The composed runtime bind chain is:
   TLS/runtime bind → **mb_win_module_enum_bind** → mb_win_loader_bridge_bind.
   The module-enum binder consumes all census KERNEL32 module names
   (GetModuleHandle*, GetModuleFileName*, GetProcAddress, LoadLibrary*,
   FreeLibrary, K32*), so it must sit BEFORE the loader bridge, which still
   binds the same names for modules the enum owner has not registered. Keep
   the loader-bridge bind as the fallback for un-registered loads; the enum
   binder returns UNRESOLVED for ordinal imports and non-kernel32 libraries.

3. **Session wiring.** `mb_win_module_enum_abi_begin/end` mirror the
   mutex/init-once session family: begin requires a zeroed session, a single
   installed owner and an EMPTY `mb_pe_module`; end requires the module back at
   EMPTY; the bind context is the session pointer (module must be LOADING or
   ACTIVE when the binder runs). One owner instance per process; a second
   owner is refused with BUSY while any ABI session lives.

4. **Table population.** Register the executable image first
   (`executable_image=1`, base = main image base) — GetModuleHandle(NULL),
   GetModuleFileName(NULL), GetProcAddress(NULL,·) and
   K32GetModuleInformation(·,NULL,·) all resolve through it. Then register one
   record per pe_loader record after each successful load: `name` = normalized
   catalog key, `file_name` = full path string, `base` = loader handle,
   `image_size` from the PE header, `entry_point` optional, exports borrowed
   from the loader's export cache when available.

5. **Acquisition seam.** `mb_win_module_enum_ops.acquire/release` is the only
   sanctioned path from LoadLibrary to a real image: the hook calls the
   pe_loader (via the loader-bridge owner) and returns base/size/token. Without
   ops, LoadLibrary resolves only already-registered table entries and unknown
   names report NOT_FOUND (126) — this is the host-test mode. The hook must
   apply the same key policy (no dot gains ".dll"; one trailing dot
   suppresses); the owner passes the key verbatim.

6. **Reference-counting contract.** Load increments one reference per call;
   Free consumes one. Static (registered) records stay in the table at zero
   references — Windows-observable because our runtime never unmaps. Records
   registered through the acquire hook are disposed via `ops.release` when the
   last reference retires; a failed release retains the record for free-retry
   (OWNER_ERROR), matching the pe_loader cleanup-retry discipline.

7. **Last-error surface.** The ABI unit records the mapped Win32 error of the
   most recent thunk call in `mb_win_module_enum_abi_last_error()` (single-
   threaded runtime, no TLS dependency). If the controller prefers the real
   thread-domain last-error, replace the `fail()` helper body with
   `*mb_win_threads_last_error(domain)=code` once a domain is installed — the
   call sites are already separated for exactly this swap.

8. **Semantics kept Windows-exact:** module-name lookup folds ASCII case with
   the .dll key policy; symbol lookup is exact ASCII; proc pointers 1..65535
   are ordinals (1-based, bounded by export_count); GetModuleFileNameA returns
   nSize on truncation with error 122; GetModuleHandleEx supports
   PIN/UNCHANGED_REFCOUNT (no unmap model) and FROM_ADDRESS; LoadLibraryEx
   accepts only hFile=NULL/flags=0 (else 50); K32* process handles accept only
   NULL and the current-process pseudo-handle -1 (else 6); MODULEINFO is the
   exact x64 shape (24 bytes, static-asserted).

9. **Non-goals retained from the census contract:** no filesystem search, no
   ANSI codepage conversion (W names fold ASCII), no foreign-handle
   dereference, no DONT_RESOLVE_DLL_REFERENCES/LOAD_LIBRARY_AS_* flags, no
   LOAD_LIBRARY_SEARCH_* flags, no EnumProcessModulesEx/psapi aliases beyond
   the three K32 names observed.

10. **Verification loop.** Host: `clang -arch x86_64 -std=c11 -O2 -Wall
    -Wextra -Werror -I sources -I windows-execution sources/*.c
    tests/test_win_module_enum.c` then `arch -x86_64` (thunk typedefs carry
    `__attribute__((ms_abi))` — do not remove them, they are what keeps the
    Rosetta host route honest). Native receipts (SDK v0.43 prospero-clang +
    pinned LLVM 21.1.8 via `LLVM_CONFIG`): owner object with **zero undefined
    symbols**, FreeBSD9-frontend IR with exactly **14 win64cc definitions**,
    PS5-backend object referencing **only** the 12 owner core symbols. See
    `receipts/receipt.json`.

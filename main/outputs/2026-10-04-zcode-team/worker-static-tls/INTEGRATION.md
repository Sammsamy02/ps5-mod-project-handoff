# Static TLS family — integration notes (10-point wiring list)

Family: `mb_win_static_tls.{h,c}` (owner) + `mb_win_static_tls_abi.{h,c}` (PE binder),
windows-services pattern. All paths relative to `outputs/ps5-mod-bridge/` unless noted.
No existing file was edited by this worker; every wiring point below is controller work.

1. **build.py wiring** (same recipe as the CRT startup family):
   add `outputs/2026-10-04-zcode-team/worker-static-tls/sources/mb_win_static_tls.{h,c}`
   to the native owner set and `mb_win_static_tls_abi.{h,c}` to the win64cc bridge set.
   Includes needed: windows-services, windows-execution, windows-image (pe_module.h,
   pe_image.h). Evidence recipe: `worker-static-tls/verify_static_tls.py`
   (host / --sanitize / --tsan / --red-stub / full native+bridge).

2. **Binder composition order**: append `mb_win_static_tls_bind` to the runtime bind
   chain (process → crt → heap → dynamic-TLS bridge → init-once → mutex/srw/critical
   → **static TLS last**). It answers only the exact names `__tls_index`, `_tls_index`,
   `__tls_start`, `_tls_start`, `__tls_end`, `_tls_end`, `__tls_import`,
   `_imp___tls_import` (any library spelling) and returns UNRESOLVED otherwise, so
   late placement never shadows the KERNEL32 dynamic-TLS bridge
   (`TlsAlloc/TlsFree/TlsGetValue/TlsSetValue`).

3. **Lifecycle order per TLS-bearing module** (mirrors the Windows loader):
   `mb_win_static_tls_begin_session` while `mb_pe_module` is EMPTY (before
   `mb_pe_module_open`) → `mb_win_static_tls_register` during LOADING with
   `host_base = mb_pe_module.image` and `preferred_base = mb_pe_info.preferred_base`
   (register writes the live index into the image's AddressOfIndex DWORD immediately,
   matching load-time TLS index assignment) → `mb_win_static_tls_process_attach`
   (callbacks ascending, reason 1) **before** the entry adapter invokes DllMain via
   `mb_win_call_entry` → after DllMain DLL_PROCESS_DETACH, `process_detach`
   (callbacks descending, reason 0) → `unregister` → `end_session` after the module
   is EMPTY again. Attach returns 0/-1; a failed attach must abort the load like a
   failed DllMain.

4. **TLS directory source**: the retained image's directory (PE data directory 9)
   must be parsed with `mb_win_static_tls_parse` (40 bytes → 6 fields). Today
   `mb_pe_module` refuses TLS-bearing images (`pe_module.h`: "Only AMD64 images
   without runtime directories beyond exports/imports/resource/security/reloc/
   debug/IAT are admitted", and `mb_pe_info.requirements` already flags
   `MB_PE_REQUIRES_TLS`). Controller decision required: either extend pe_module to
   admit TLS when a static-TLS session is bound, or extract the 40 directory bytes
   + callback array before open. Census facts to encode in fixtures: UE4SS.dll
   directory present (728 raw bytes, zero fill 0, characteristics 0x500000, 2
   callbacks at VAs 0x180c956f0 / 0x180c95a10); dwmapi.dll has NO TLS directory
   (`baseline-dll-census.json`, `tls.directory_present=false`) — the family stays
   unused for dwmapi.

5. **Thread notification gap**: `mb_pe_module` supplies no DLL_THREAD_ATTACH/DETACH
   notifications (documented there as unsupported). The composition must call
   `mb_win_static_tls_thread_attach` / `thread_detach` around every admitted
   thread's entry/exit (worker spawn and join paths), or threads get blocks only
   lazily via `mb_win_static_tls_block` with no callbacks (documented divergence).

6. **Callback ABI boundary**: dispatch reads the relocation-updated callback array
   from mapped memory and invokes plain native-C function pointers. Real retained
   image callbacks (both UE4SS entries) are win64cc code: route the dispatch through
   an entry-bridge-style trampoline (pattern of
   `windows-services/mb_win_entry.c` `mb_win_call_entry`) or compile this owner into
   the bridge IR where ms_abi pointers are call-compatible. Until wired, callbacks on
   target remain component evidence, not execution.

7. **Domain coupling and capacity**: each registered module consumes ONE dynamic
   index from the same `mb_win_threads` pool as `TlsAlloc` (capacity 1088,
   minimum 64). UE4SS additionally allocates dynamic TLS + FLS at runtime, so budget
   indices accordingly. Register/unregister and all attach/detach calls must run on
   a domain-admitted thread and must NOT hold `mb_win_threads_lock` (the underlying
   `tls_*` calls lock internally). Unregister frees the index (`tls_free` resets the
   slot on live threads); close refuses while any module or session remains.

8. **Access-path wiring for retained code**: retained modules with static TLS read
   `__tls_index` and the TEB's ThreadLocalStoragePointer inline (`gs:[0x58]`). This
   family cannot bind `__tls_array` (register-relative; stays UNRESOLVED by design).
   `__tls_import` is this family's adapter thunk `ptr win64cc(i32 index, i64 offset)`
   = `tls_array[index] + offset` for composition shims that cannot inline a TEB
   read; it is NOT a Microsoft declaration and no census import uses it. New shim
   code compiled by lld-link/clang for the composition should prefer the
   `__tls_index` + block-offset shape this owner publishes.

9. **Explicit non-goals preserved**: no FLS (`FlsAlloc/FlsFree/FlsSetValue` observed
   in UE4SS census — separate surface), no TEB emulation, no loader lock, no
   exception trapping around callbacks, no eager blocks for pre-existing threads
   (lazy allocation divergence documented in the header contract). Callbacks run
   outside the domain mutex; attach dispatch is ascending, detach descending — the
   entry adapter owns the DllMain ordering, this family owns only callback order.

10. **Verification contract**: run `verify_static_tls.py` per change; expected
    statuses: `STATIC_TLS_HOST_TESTS_PASSED` (host-only),
    `EXPECTED_OPEN_STUB_REJECTION` (red control: only owner open stubbed to
    BAD_STATE must fail the suite), `STATIC_TLS_DIRECTORY_BLOCKS_CALLBACKS_AND_
    NATIVE_OBJECTS_PASSED` (full: pthread host suite + ASan/UBSan + TSan +
    prospero-clang direct PS5 owner object + FreeBSD9-frontend Win64 bridge IR with
    exactly one win64cc definition `@tls_import_thunk(i32, i64)` + PS5 backend
    objects + llvm-nm/objdump + SDK export-coverage). Never claim target execution
    from these receipts (`target_executed=false`, `console_contact=false`).

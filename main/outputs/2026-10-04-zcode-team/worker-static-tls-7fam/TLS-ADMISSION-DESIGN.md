# TLS-ADMISSION-DESIGN — pe_module admission of TLS-bearing modules (worker-static-tls-7fam)

Worker: worker-static-tls-7fam, 2026-10-04. All citations are into this
worker's vendored tree (`vendor/` here = byte copy of
`worker-combined-dll-allfam/vendor` plus the vendored static-TLS family
sources of `worker-static-tls/sources`); line numbers refer to the vendored
files after this worker's documented edits. No console contact, no target
execution.

## 1. Why TLS-bearing images are refused today

`vendor/windows-execution/pe_module.c` `admission()` (lines 39–63) enforces a
closed allowlist in two independent gates:

- Requirements gate (lines 45–48): `requirements = MB_PE_REQUIRES_IMPORTS |
  (with_entry ? MB_PE_REQUIRES_ENTRY : 0)` and
  `if (v->requirements & ~requirements) return UNSUPPORTED`. `mb_pe_inspect`
  sets `MB_PE_REQUIRES_TLS` (`vendor/windows-image/pe_image.h:27`) in
  `mb_pe_info.requirements` when data directory 9 is present, so a TLS image
  is refused before the directory loop is even reached.
- Directory allowlist (lines 43–51): bits {0 exports, 1 imports, 2 resource,
  4 security, 5 basereloc, 6 debug, 12 IAT}; any nonzero directory outside
  the set returns `MB_PE_MODULE_UNSUPPORTED` with `detail = index` — so a
  nonzero directory 9 is refused even if the requirement bit were allowed.
- The contract text (`vendor/windows-execution/pe_module.h:76–80`) states the
  same: "Only AMD64 images without runtime directories beyond
  exports/imports/resource/security/reloc/debug/IAT are admitted … No TLS/
  unwind/CFG/CLR support". `worker-dll-console-stage-prep` correctly flagged
  this as the blocking decision for the static-TLS family.

Both gates must move together: allowing only one would still refuse (the
other check fires first).

## 2. Decision: explicit opt-in admission ticket (no loader API change)

Options considered (from `worker-static-tls/INTEGRATION.md` point 4): (a)
extend `pe_module` to admit TLS when a static-TLS session is bound, or (b)
extract the 40 directory bytes before `open`. Option (b) alone cannot work —
`mb_pe_module_open*` itself re-runs `admission()` on the image, so extraction
without admission still ends in `UNSUPPORTED`. Option (a) is therefore
mandatory; it is implemented as an **opt-in admission ticket** so the closed
allowlist stays closed by default:

- `mb_pe_module` gains one field, `const void *tls_admission;`
  (`vendor/windows-execution/pe_module.h`). The struct contract (pe_module.h:
  61–62) already requires callers to zero-initialize the module before first
  use, so default behavior is byte-identical with the pre-patch tree.
- `admission()` gains the ticket as an input: when `m->tls_admission` is
  non-NULL, `requirements` additionally allows `MB_PE_REQUIRES_TLS` and the
  directory allowlist additionally allows bit 9. The ticket is opaque to
  pe_module — it is never dereferenced or retained; `open_impl` reads it at
  line 156 (admission) strictly before the `memset(m, 0, sizeof(*m))` at line
  160 wipes it, so a consumed ticket cannot leak into later state.
- Who sets it: the composition, not the loader. `mb_pe_loader`'s `ensure()`
  calls `services.begin` **before** `open_link` while the module record is
  still `MB_PE_MODULE_EMPTY` (`vendor/windows-execution/pe_loader.c:274–282`),
  and nothing clears the module in between. So
  `owned_runtime.c` `service_begin` sets `module->tls_admission` for the
  consumer record only, under `#if MB_OWNED_STATIC_TLS`. The helper record
  never gets a ticket and keeps the closed allowlist.
- Fail-closed: with the ticket, a TLS directory is *admitted for parsing and
  lifecycle wiring by the static-TLS family*, not silently tolerated. The
  composition independently verifies the directory exists
  (`mb_pe_inspect` on the embedded bytes, `info.directories[9]`) and fails
  the build stage if `--static-tls` is on but the fixture carries no
  directory — the inverse (directory without ticket) keeps the exact legacy
  refusal.

## 3. What the TLS directory is on x64 (fields, index, __tls_array)

The family's view of `IMAGE_TLS_DIRECTORY64` is
`mb_win_static_tls_values` (`vendor/windows-services/mb_win_static_tls.h:
16–23`): six raw fields (start/end template VAs, AddressOfIndex VA,
AddressOfCallbacks VA, SizeOfZeroFill, Characteristics), parsed by
`mb_win_static_tls_parse` (mb_win_static_tls.c:114–127, little-endian, 40
bytes, refuses `end < start`).

- **Index assignment.** Windows assigns each TLS-bearing image one slot in
  the thread's TLS array (TEB `ThreadLocalStoragePointer` at gs:[0x58]) and
  writes the slot number into the image's `AddressOfIndex` DWORD at load
  time, *before any callback runs*, so inlined `__tls_index` reads observe
  it. The family mirrors this exactly: `mb_win_static_tls_register`
  (mb_win_static_tls.c:148–200) consumes ONE slot from the same domain pool
  as `TlsAlloc` (`mb_win_threads_tls_alloc`, line 179) and writes the live
  index into the mapped image at line 198 — "load-time write, before any
  attach". Index capacity and slot sharing with dynamic TLS are the family's
  documented model (INTEGRATION.md point 7; worker-static-tls REPORT
  divergence 4: no separate TEB static-array tier).
- **`__tls_array`.** This is a gs-relative TEB offset, not bindable data; the
  ABI binder deliberately leaves it UNRESOLVED
  (`vendor/windows-services/mb_win_static_tls_abi.c:55` and the header
  contract). There is no TEB emulation (REPORT divergence 3).
- **Access path.** Real retained code reads `__tls_index` and
  `ThreadLocalStoragePointer[index]` inline, which cannot work without a TEB.
  The family therefore publishes the adapter thunk `__tls_import(index,
  offset)` = `tls_array[index] + offset` (mb_win_static_tls_abi.c:31–39,
  header lines 30–44): the calling thread's block via
  `mb_win_threads_tls_get`, **never allocating** (documented; lazily created
  blocks would have no detach notification path and would leak — see §5).

## 4. What each stage needs: load vs first access

Order verified against the actual sources:

1. **Pre-open (module EMPTY).** The composition inspects the embedded
   consumer bytes itself (`mb_pe_inspect` is public, pe_image.h:80):
   `directories[9]` gives the 40-byte directory's RVA/size, `info.sections`
   maps RVA→file offset, `info.preferred_base` is remembered for VA
   translation. The immutable file bytes remain valid until `close`
   succeeds (pe_module.h:62), and the parsed values + callback array are
   re-read from the *mapped* image after open, so nothing is copied out of
   band. `mb_win_static_tls_begin_session` runs here — it requires the
   module EMPTY (mb_win_static_tls_abi.c:13).
2. **LOADING — first binder call.** `open_impl` allocates the image
   (pe_module.c:164), then `mb_pe_prepare` copies headers and sections
   (pe_image.c:360–365) and applies relocations (line 372) **before** the
   import walk (line 374). Registration therefore happens on the first
   `service_bind` call: the module is LOADING (required by
   `mb_win_static_tls_bind`, abi.c:44), `module->image` is valid,
   `raw_data`/`index_slot`/`callbacks` resolve into already-copied,
   already-relocated image bytes, and the index write at
   mb_win_static_tls.c:198 cannot be overwritten by a later section copy.
   The binder answers only the eight exact ASCII names
   (`__tls_index`/`_tls_index`, `__tls_start`/`_tls_start`,
   `__tls_end`/`_tls_end`, `__tls_import`/`_imp___tls_import`) and returns
   the AddressOfIndex address, template bounds, and the thunk address
   (abi.c:47–55); `bind_nonzero` (pe_module.c:120–124) then publishes them
   into the IAT.
3. **INITIALIZING — process attach.** `open_impl` invokes the entry callback
   with reason 1 (pe_module.c:189–196). The composition's `entry_invoke`
   wrapper calls `mb_win_static_tls_process_attach` *before*
   `mb_win_call_entry` — callbacks ascending (reason 1), per INTEGRATION.md
   point 3 — and treats a failed attach exactly like a failed DllMain
   (return FALSE → `ENTRY_REJECTED`, which open converts into the standard
   attach-rejection cleanup with its detach notification).
4. **First access / other threads.** Fixture threads use the bound
   `__tls_index` (data) and `__tls_import` (thunk). A thread that never got
   a thread-attach notification observes `NULL` for its slot — the exact
   documented behavior ("never allocates"), and the fixture asserts it as
   the per-thread-isolation evidence. Worker-call threads get explicit
   `mb_win_static_tls_thread_attach` in the composition's
   `worker_call_begin` window (INTEGRATION.md point 5's first branch),
   which dispatches reason 2 callbacks ascending and is idempotent for the
   already-attached entered thread (mb_win_static_tls.h attach contract).
5. **STOPPING — detach.** `mb_pe_module_close` invokes the entry callback
   with reason 0 before releasing the image (pe_module.c:308–315). The
   wrapper runs, in order: `mb_win_call_entry(0)` (DllMain DLL_PROCESS_
   DETACH, canonical order per INTEGRATION.md point 3), then
   `mb_win_static_tls_process_detach` (callbacks descending, reason 0,
   releasing the current thread's block), then `mb_win_static_tls_unregister`
   — still inside the callback, i.e. before the image is unmapped. Unregister
   refuses while any live block remains (mb_win_static_tls.c:211–214), which
   is what keeps the no-alloc-for-unnotified-threads rule honest.
6. **EMPTY — session end.** `service_end` runs after `mb_pe_link_close`
   (pe_loader.c:144–153), so `mb_win_static_tls_end_session` sees the module
   EMPTY (abi.c:22). Owner `close` then refuses while sessions, pins,
   registered modules or live blocks remain (mb_win_static_tls.c:45–49) —
   the unload-cleanup proof.

## 5. The win64cc boundary: dispatch trampoline

TLS callbacks in a mapped image are win64cc code (census: UE4SS's two
entries); the owner's dispatch (`dispatch`, mb_win_static_tls.c) must call
them with the Windows x64 convention on both the host route (ms_abi under
Rosetta) and the bridge IR. The pattern is `mb_win_entry`'s
`mb_win_call_entry`: a plain-native-convention function that calls through an
`ms_abi` **function-pointer type**, producing exactly one `call win64cc` in
the bridge IR and no extra win64cc *definition* (build.py enforces
`mb_win_entry`: 0 defs / 1 call at vendor/windows-candidate/build.py:267,
274).

Adaptation (the only semantic edit to the vendored family owner): dispatch
goes through a settable dispatcher hook —
`mb_win_static_tls_set_dispatch(fn)` with
`void fn(void *callback, void *host_base, uint32_t reason)`; the default
dispatcher preserves the original direct plain-C call (standalone family
tests unchanged). The composition installs

```c
static void static_tls_dispatch_adapter(void *callback, void *base, uint32_t reason) {
    typedef void (__attribute__((ms_abi)) *cb_t)(void *, uint32_t, void *);
    ((cb_t)callback)(base, reason, 0);
}
```

in `owned_runtime.c` (compiled with the FreeBSD9 frontend for the bridge, so
`ms_abi` is accepted — the same reason `owned_runtime.c` gets `bflags` at
build.py:265). Result: across all abi units the ELF gains exactly ONE new
win64cc definition — `@tls_import_thunk(i32, i64)` in `mb_win_static_tls_abi`
— matching the original assessment ("zero new ELF imports, +1 win64cc IR").

## 6. The fixture's TLS directory (linker facts, verified by experiment)

lld-link does **not** synthesize a TLS directory from `__tls_used` (verified
with both the rust-shipped lld-link and LLVM 21.1.8 `lld -flavor link`: both
produce `TLSTableRVA/Size = 0`, and `$`-subsections are grouped/dropped
under `/opt:ref`). The payload therefore makes the fixture self-describing
and lets build.py patch data directory 9 post-link:

- One `.tls` region object (`worker-statictls.h`): 24 bytes = 16-byte
  template signature + 4-byte index cell + 4-byte end marker, in a single
  object so declaration order cannot scramble the offsets; referenced from
  code so `/opt:ref` keeps it; section `READ|WRITE` (no shared/cache/paging
  bits, which pe_module refuses at line 57–59).
- Callback array (two fixture callbacks + NULL) in `.rdataC`; the 40-byte
  directory struct (preferred-base VAs, filled by base relocation like every
  real image) in its own `.rdataT` section.
- build.py, after the consumer link: locates `.rdataT` in the section table
  and patches the optional header's data directory 9 to `{rva, 40}` — the
  same directory `mb_pe_inspect` then reports and `admission()` (with
  ticket) accepts. The patch is inside the embedded-fixture flow, so the
  bytes hashed and embedded are exactly the patched image. The pre-existing
  unsupported-directory guard (build.py:156–157) is updated to allow 9 for
  the consumer only under `--static-tls`, and to *require* it there.

## 7. Evidence invariants (7-family composition)

- Prior six families' evidence must not shift: `results` stays [86,93],
  worker calls 4/4, dll_calls 3, attach/detach [7,7]/[0,0], contract_errors
  0, cleanup retired {pending:0,retired:1}, all prior family keys
  `{"enabled":1}`.
- New console-results deltas: `static_tls` key `{"enabled":1}`;
  `consumer_imports` 78 → 80 (exactly `__tls_index` + `__tls_import` bound);
  SDK ELF undefined-import set unchanged (delta ZERO); total win64cc IR
  definitions +1 (`@tls_import_thunk`); fixture callback log = [(1,1),(1,2)]
  + [(2,1),(2,2)]×5 + [(0,2),(0,1)], validated in-fixture (codes -120..-127)
  and via the fixture's last-error audit channel after unload.

## 8. Explicit non-claims

No TEB emulation (`__tls_array` stays unresolvable, gs:[0x58] never
emulated), no eager blocks for pre-existing threads (lazy path documented;
the fixture's unattached thread observes NULL), no loader lock, no FLS, no
exception trapping around callbacks, and no target/console execution claim.
Divergences inherited unchanged from `worker-static-tls/REPORT.md`.

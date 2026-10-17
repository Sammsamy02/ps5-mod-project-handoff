# worker-fileio report — KERNEL32 file I/O and metadata family (backlog rank 3)

## Scope: exact observed set, per-name evidence

Source of truth: the two frozen readobj files (the ONLY real-mod DLL imports in this
workspace; both from `nexus-real-mods-2026-10-01/ue4ss-v1.zip`).

Implemented this pass (16 names; evidence `file:line`):

| Name | dwmapi.dll | UE4SS.dll | Notes |
|---|---|---|---|
| CreateFileW | — | 342 | core open/create |
| CreateFile2 | 230 | 348 | thin wrapper per docs |
| ReadFile | — | 268 | synchronous only |
| WriteFile | — | 269 | synchronous only |
| FindFirstFileW | 232 | 346 | wildcard + exact-name forms |
| FindFirstFileExW | — | 328 | Standard/Basic levels only |
| FindNextFileW | — | 341 | |
| FindClose | 233 | 329 | |
| AreFileApisANSI | 229 | 349 | fixed TRUE (policy) |
| GetFileAttributesExW | 231 | 347 | Standard level only |
| GetFileInformationByHandle | — | 267 | |
| GetFileInformationByHandleEx | 227 | 323 | FileBasicInfo/FileStandardInfo only |
| CreateDirectoryW | — | 324 | |
| DeleteFileW | — | 266 | |
| GetCurrentDirectoryW | — | 326 | fixed virtual root (policy value) |
| GetFinalPathNameByHandleW | — | 367 | VOLUME_NAME_DOS only |

Census-observed but explicitly unsupported this pass (fail fast, never fake success):
`SetFileInformationByHandle` (UE4SS 315), `FindFirstChangeNotificationW` (UE4SS 350),
`FindNextChangeNotification` (UE4SS 246), `FindCloseChangeNotification` (UE4SS 245).
Each fails with `FALSE`/`INVALID_HANDLE_VALUE` + last error 120
`ERROR_CALL_NOT_IMPLEMENTED` (policy code; loud-failure requirement).

Observed and correctly left to other families: `CloseHandle` (both DLLs; mutex family
rank 2 — interaction contract below), `CreateFileMappingW` (UE4SS 270; rank 7
mappings), `NtWriteFile` (UE4SS 223; ntdll, not KERNEL32). `SetFilePointerEx`,
`GetFileSizeEx`, `FlushFileBuffers` are NOT in either retained import table and were
NOT implemented (trimmed per assignment). ERROR_BROKEN_PIPE (109) is documented only
for pipes; this pass has no pipe objects, so it is never raised.

## Owned files (all new, only under this output area)

- `sources/mb_win_fileio.h` / `sources/mb_win_fileio.c` — owner: virtual namespace,
  handle table, all 20 documented KERNEL32 behaviors (16 real + 4 fail-fast stubs)
  plus `mb_win_fileio_close_handle` (CloseHandle delegation service).
- `sources/mb_win_fileio_abi.h` / `sources/mb_win_fileio_abi.c` — ms_abi bridge:
  session pinning + `mb_pe_binder` for the exact 20 kernel32.dll names.
- `sources/tests/test_win_fileio.c` — host typed-ABI test + layout probe.
- `verify_fileio.py` — verifier (host/ASan+UBSan/TSan, red-stub control, native
  PS5 object route, SDK declaration check). Imports unchanged
  `windows-services/verify.py` helpers read-only.
- `receipts/` — receipt trees; authoritative: `7e9da974c36e4417be9bd82daebea652`
  (full pass) and `afb21f36465f4e9c8e86fca372a412f6` (red-stub). Three earlier
  iteration receipts are marked FAILED and kept for provenance.

No existing file was touched; `windows-candidate/build.py` was read only.

## Win64 semantics decisions (widths are contracts)

- Widths asserted in owner, bridge and layout probe: HANDLE 8/8, BOOL 4, DWORD 4,
  FILETIME 8/**align 4** (two DWORDs — this makes WIN32_FILE_ATTRIBUTE_DATA 36/4,
  BY_HANDLE_FILE_INFORMATION 52/4, WIN32_FIND_DATAW 592/4),
  CREATEFILE2_EXTENDED_PARAMETERS 32/8, FILE_BASIC_INFO 40/8, FILE_STANDARD_INFO 24/8.
  Layout probe equality native vs bridge:
  `[8,8,4,4,8,36,52,592,32,40,24,4,4,4,4,8,8,8]`.
- Both DLL-visible returns are 32-bit (`BOOL`/`DWORD`); handles are pointer-sized
  opaque tokens (generation-tagged table indices; 0 and -1 never issued; no host fd
  ever leaks across the ABI).
- Documented behaviors implemented: dispositions 1-5 incl. CREATE_NEW->183
  ERROR_FILE_EXISTS; OPEN_EXISTING missing->2, missing parent->3; OPEN_ALWAYS/
  CREATE_ALWAYS on existing succeed AND set last error 183 (documented quirk; every
  other success preserves LastError — marker-verified); share-mode conflicts->32;
  readonly write/delete->5; delete on directory->5; FindNext exhaustion->18;
  invalid/closed handles->6; GetCurrentDirectoryW return semantics.
- Adapter-policy codes (documented in header + here): ':'-prefix and '..' ->87
  (containment refusal); unknown access bits/flags, non-NULL SECURITY_ATTRIBUTES or
  template ->87 (refuse instead of fake); FILE_FLAG_OVERLAPPED or non-NULL
  lpOverlapped ->50 ERROR_NOT_SUPPORTED (overlapped out of scope this pass);
  transfers >1 GiB ->87 (bounded sizes); host symlink anywhere under root ->5
  (containment; never reported by enumeration); component >255 ->123, total >32767
  ->206; GetFileInformationByHandleEx unsupported classes ->87, short buffers ->122;
  GetFinalPathNameByHandleW non-DOS volume names/FILE_NAME flags other than
  OPENED ->87; unsupported family ->120.
- Virtual namespace (exact rule): controller supplies ONE real host directory at
  owner open. Mod paths are UTF-16, root-relative; '/' and '\' both separate; leading
  separators stripped; drive prefixes and '..' rejected (87); '.' and empty
  components dropped; ASCII case-insensitive matching (documented NTFS default;
  FIND_FIRST_EX_CASE_SENSITIVE fails fast instead of faking). Per-component
  resolution reads the host directory and fold-matches, so semantics do not depend
  on the host filesystem's case behavior. Current directory fixed at '\' since
  SetCurrentDirectory is not census-observed. Final-path format policy:
  `\\?\vfs\` + canonical path.
- Handle semantics: synchronous pread/pwrite on per-handle offsets (independent file
  pointers per handle, as on Windows); owner-wide operation mutex serializes calls
  (correctness over throughput; documented in header). Directory handles require
  FILE_FLAG_BACKUP_SEMANTICS and refuse read/write (5). Enumeration returns '.'/'..'
  when matched, never recurses, and does NOT model the 8.3 '*.*' short-name quirk
  (documented divergence). BY_HANDLE volume serial/index are opaque host-derived
  values; negative host times clamp to 0 (policy).
- CloseHandle interaction: rank 2 owns the CloseHandle import. This family binds
  only the 20 file names and exposes `mb_win_fileio_close_handle(owner, HANDLE)`
  for that future owner to delegate file-handle closing; CloseHandle on a find
  handle must fail (6) — Windows requires FindClose.

## Exact commands and results

```
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-fileio/verify_fileio.py --sanitize --tsan
  -> status FILEIO_HOST_WIN64_AND_NATIVE_OBJECTS_PASSED
     receipt receipts/7e9da974c36e4417be9bd82daebea652/receipt.json
python3 outputs/2026-10-04-zcode-team/worker-fileio/verify_fileio.py --red-stub --host-only
  -> status EXPECTED_OPEN_STUB_REJECTION
     receipt receipts/afb21f36465f4e9c8e86fca372a412f6/receipt.json
```

Host evidence (host + ASan/UBSan + TSan runs all exit 0, ten PASS lines each): typed
ms_abi calls through binder-resolved function pointers in a fresh temp namespace root
under this output area: create/read/write/size roundtrip; per-handle EOF; metadata by
path/handle/handle-Ex incl. directory attributes; close-then-use refusal (6);
OPEN_ALWAYS/CREATE_ALWAYS 183 quirk and CREATE_NEW 183; 2/3/5/6/32/50/87 fidelity;
share enforcement; two-thread concurrent distinct handles with per-thread LastError
markers; independent file pointers; enumeration incl. '.', '..', no-more-files 18,
repeat-18, closed-handle refusals, non-recursion; CreateDirectory 183/3;
DeleteFile 2/5/32; CreateFile2 wrapper incl. parameter refusals; final-path length
semantics and policy format; unsupported stubs fail 120; binder refusals (wrong
library, unknown name, ordinal), session lifecycle, owner close BUSY on pins/handles.

Native route (unchanged ABI rule): `prospero-clang` direct PS5 owner object plus
FreeBSD9-frontend -> **unedited win64cc LLVM IR** -> PS5 backend for the ABI unit;
data layout equality native vs bridge asserted; exactly **20 win64cc definitions**,
one per exact documented signature (receipt `win64cc_signature_census`: e.g.
`ptr @create_file_w(ptr,i32,i32,ptr,i32,i32,ptr)`, `i32 @read_file(ptr,ptr,i32,ptr,ptr)`,
`i32 @are_file_apis_ansi()`, `i32 @get_current_directory_w(i32,ptr)` — all 20 unique);
the binder stays native-C calling convention. Red-stub control: only
`mb_win_fileio_open` mutated to BAD_STATE; the real test rejects it and sources are
byte-stable across the run.

SDK v0.43 declaration check: owner object's undefined imports (pthread_mutex_*,
opendir/readdir/closedir, open/pread/pwrite/close, lstat/fstat, mkdir/unlink,
malloc/calloc/free/strdup, memcpy/memset/strcmp/strlen/strrchr, abort, __error —
13 non-threads symbols) are all declared by `libkernel_web.so` /
`libSceLibcInternal.so` exports. `target_resolution_proven: false` — **no
target(PS5) support claim**; PS5 is controller-only.

## build.py wiring points for the controller (NOT edited by this worker)

Same 10-point pattern as the InitOnce report, adjusted counts:
1. Promote the four C sources + test into `windows-services/`.
2. argparse flag (e.g. `--fileio`) gating on the appropriate base profile.
3. `owned_profile.h`: `#define MB_OWNED_FILEIO`.
4. Source lists: `native_c += [SERV/'mb_win_fileio.c']`,
   `abi_c += [SERV/'mb_win_fileio_abi.c']`; the owner needs a controller-chosen
   namespace root passed at owner open (install-scope containment).
5. Receipt `sources` list additions.
6. `tests/worker-fileio-kernel.def` exporting the 20 names; `lld-link /lib /def:`.
7. Consumer fixture exercising the imports (CreateFileW/ReadFile/WriteFile roundtrip
   + FindFirst/Next/Close would exercise 8 of them).
8. Receipt profile: `fileio: {enabled, win64cc_defs: 20, pe_imports: N_used}`.
9. win64cc census (`build.py` expected map): `'mb_win_fileio_abi': 20`.
10. Host driver result assertions for the new key.

## Open questions / limits

- Overlapped I/O, change notifications, SetFileInformationByHandle (rename/
  disposition/end-of-file), GetFileAttributesExW levels beyond Standard,
  FindExSearch ops and case-sensitive find are unimplemented and fail fast — they
  need a later pass with their own evidence, not silent stubs.
- Owner-wide mutex serializes I/O (documented policy); Windows allows truly
  concurrent I/O on distinct handles. Correct now, throughput later.
- Waiter ordering, 8.3 short-name matching, short-name alternate field, hidden/
  system attributes, and the change-notification wake model are unmodeled
  (documented divergences above).
- Host FS case semantics: handled by per-component fold-matched resolution, but
  non-ASCII case folding is not implemented (ASCII-only, documented).
- Target (PS5) execution: NOT attempted and NOT claimed.

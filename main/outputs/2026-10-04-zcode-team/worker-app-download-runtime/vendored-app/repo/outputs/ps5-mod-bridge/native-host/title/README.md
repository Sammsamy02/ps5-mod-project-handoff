# Captured title metadata decoder

`metadata.h` exposes `mb_title_metadata_decode_v1()` as a pure C ABI over complete immutable PS5 root-field `param.json` bytes. It performs no filesystem, title enumeration, process, account or network operation. The application's actual native title provider remains unavailable.

Initialize `mb_title_metadata_input_v1` with exact `struct_size`, ABI1, `MB_TITLE_PARAM_JSON_ROOT_V1`, zero reserved and the captured byte span. Supply valid, mutually disjoint full input/output backing; output needs no prior initialization. The result owns fixed NUL-terminated `title_id[65]`, `name[257]`, `version[129]`, `locale[65]` and `metadata_sha256[65]`, plus availability/source-size/scope/status scalars. No input pointer is retained.

Required root `titleId` follows ASCII `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`. There is no commercial prefix, game or mod branch. Optional root `contentVersion` preserves up to128 UTF-8 bytes exactly; `masterVersion`, filenames or catalog names cannot substitute. Optional `localizedParameters` must be an object. Its optional string `defaultLanguage` (up to64 UTF-8 bytes) selects only the exact corresponding locale object and its optional `titleName` (up to256 UTF-8 bytes). JSON field order is irrelevant. Missing declaration/object/name remains unavailable; malformed present selected fields fail. No configured-locale, English or first-match fallback is implemented. Present optional empty strings remain exactly observed/available; embedded NUL is rejected because fixed C strings cannot preserve it.

The full input limit is65536 bytes, container depth16, values+keys4096 and each decoded string/key4096 UTF-8 bytes. Root containers, nested containers, scalar values and object keys each count once. Valid UTF-8, complete strict JSON and decoded duplicate-key rejection apply even to ignored auxiliary fields. Duplicate escape spellings are checked as their decoded key. Whole-input SAX validation precedes root-field extraction; a second pass captures identity/version/default locale, and a third pass selects that locale's name when declared. No JSON DOM is created. Pinned nlohmann's DOM destructor can allocate beneath `noexcept`; the retained late-allocation crash led to the supported SAX approach. Tokenizer/key/hash storage still allocates, within fixed input bounds; this decoder is not allocation-free.

`MB_TITLE_METADATA_OK` commits a whole owning metadata-only result. `MB_TITLE_SOURCE_METADATA_CAPTURE` and `MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256` identify only the captured JSON observation. The SHA256 covers all captured bytes, including whitespace and ignored fields; it is never an executable/build fingerprint or authenticity claim. Installed/running/build-fingerprint/engine/runtime/install/authenticity/game-acceptance flags stay zero. A later source owner must establish its own candidate-ID binding, observation scope, stability and read permissions. AppInfoJson database rows and PS4 SFO are separate profiles outside this decoder.

Invalid pointer/span/alignment, input ABI/profile/reserved or known overlap is no-write `BAD_ARGUMENT`. A non-success preflight leaves any old result untouched and unpublished; callers must check the return and never reuse it as this call's positive output. Every operational parse/schema/limit/allocation/internal failure clears the entire result and publishes only exact result headers plus safe constant status/reason. Error output contains no raw JSON values. Allocation/other C++ exceptions are contained by the ABI; ordinary C++ scratch destruction does not claim secure erasure.

## Focused checks

Run from the repository root. The SDK path is read-only; host behavior uses only its publicly authored fixture JSON:

```sh
python3 outputs/ps5-mod-bridge/native-host/title/verify.py \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --output-dir outputs/ps5-mod-bridge/native-host/title/tests/build/green
```

The driver freshly compiles the C++ decoder with existing pinned nlohmann3.11.3 and PicoSHA2v1.0.1, five C++ behavior groups, a failing-allocator fixture and a separate actual C caller. It independently compares the authored SDK fixture's metadata digest with Python SHA256. `--red` compiles the missing implementation stub and requires actual behavioral exit1. Generated artifacts/logs/command receipts belong only to the supplied output scope; no old broad matrices or sanitizer suite are run.

For compile/link-only native evidence using existing verified tools, add:

```sh
python3 outputs/ps5-mod-bridge/native-host/title/verify.py --native \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --sdk-archive outputs/ps5-mod-bridge/native-probe/build/deps/sdk.zip \
  --llvm-config outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
  --resource-dir .superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21 \
  --output-dir outputs/ps5-mod-bridge/native-host/title/tests/build/native
```

This mode compiles the current implementation and C caller, links ordinary SDK C++/ABI/unwind libraries with BIND_NOW, inspects ELF/imports and never executes its ELF. Actual SDK header bytes and the four C/C++ runtime archives are compared against the verified SDK ZIP. Tools are checked against fixed hashes, including the invoked `llvm-readobj`. Used resource headers are matched by bytes/hash to root's approved `native-host/llvm_selected_manifest.json` (SHA256 `7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896`, `lib/clang/21` member keys). Header paths outside owned/vendor/SDK/verified-resource roots fail. SDK library imports and successful linking are not native runtime or resolver evidence. Source capture/provider/paging and Make/HTTP integration remain separate owners.

## One-candidate read-only source capture

`source.h` adds `mb_title_source_capture_v1(request, budget, workspace, 65536, result)`.
`request` has exact size/ABI1, zero reserved, enum `MB_TITLE_CACHE_SYSTEM_PRIVATE`
or `MB_TITLE_CACHE_USER`, and a borrowed generic candidate segment matching the
decoder's `[A-Za-z0-9][A-Za-z0-9._-]{0,63}` policy. The production paths are exactly
`/system_data/priv/appmeta/<candidate>/param.json` and
`/user/appmeta/<candidate>/param.json`. There is no caller-supplied root/filename,
title prefix, enumeration, mounted-game profile, source write, permission change
or provider enablement. App native inventory remains unavailable.

The ordinary OS adapter opens `/` and then each directory component separately
with SDK-defined `O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC`. It checks
`lstat`/`fstatat(AT_SYMLINK_NOFOLLOW)` and opened `fstat` identities. `param.json`
must be regular, nonnegative-sized and at most 65536 bytes; its open also uses
`O_NONBLOCK` so a FIFO substituted after the initial check cannot block awaiting
a writer before the second regular-file check. Every ancestor FD stays owned
until cleanup. After exact reads of at most 4096 bytes and a one-byte EOF probe,
opened-file and named-file identity checks compare device/inode/mode/link count,
owner/group, size and modification/change time with nanosecond fields. Every
ancestor name is then rebound under its pinned parent and compared by
device/inode/mode; `/` is rechecked. Directory mtime/size and file atime are not
used. These observations detect the tested replacement/mutation cases; they
neither freeze a hostile filesystem nor establish native permissions or a hard
I/O deadline. Short reads and interrupted/failed calls fail without retries.

Use one caller-owned `mb_title_source_budget_v1` across composed captures.
Remaining/reserved operation and requested-read totals are checked `uint64_t`
values; their sum must not overflow. The global finite ledger may fund multiple
calls and must never be replenished per file. Each call additionally caps
reservations at 128 operation tokens and 65537 requested read bytes. All attempts
are charged before the callback, including failed reads/opens. Each open reserves
two operation tokens: its attempt and one close escrow. A failed open does not
refund the unused close token. Diagnostic `operations_reserved` can therefore
exceed `operations_attempted`; read reservations never refund short/failed
attempts. Held descriptors receive one close attempt each even if the work
budget is exhausted. Production holds at most six FDs; the trusted host-only
fixture profile can hold nineteen. No close retry is made; a close error makes
`cleanup_complete` false and cannot publish success. An error such as `EINTR`
leaves actual OS close disposition unproved.

Caller backing for request, candidate, budget, exact 64KiB workspace and output
must be valid, exclusive and disjoint. Invalid ABI/profile/reserved/span/known
alias preflight changes nothing; callers must check the return and not reuse a
prior positive output. After valid preflight, every return clears the whole
workspace. Only whole `MB_TITLE_SOURCE_OK` publishes owning nested decoded
metadata with `source_complete`, `identity_checked`, `candidate_matches` and
successful cleanup. Its `title_id` must match the candidate byte for byte.
Operational errors keep bounded status/counters/errno diagnostics and clear all
owning metadata and positive capture fields. The child's eight authority flags
stay zero; metadata SHA256/version never become installation, running, engine,
asset or executable-build evidence. The C capture has no heap allocation;
borrowed existing decoder allocation failures are contained and still trigger
capture cleanup. No OS/input/test callback pointer is retained.

`source_test.h` and `mb_title_source_capture_fixture_v1()` exist only under
`MB_TITLE_SOURCE_HOST_TESTING`; the header explicitly rejects the SDK's
`__FreeBSD__` target. Their arbitrary owned fixture root/trusted fault callbacks
do not enter the production signature. Native object inspection requires that
the fixture symbol is absent.

Focused checks create only fresh evidence/fixtures beneath
`title/source_tests/build` (default paths use a unique suffix):

```sh
python3 outputs/ps5-mod-bridge/native-host/title/source_verify.py host
python3 outputs/ps5-mod-bridge/native-host/title/source_verify.py native
```

`red` compiles the retained missing-source stub and requires actual behavioral
exit1. Host checks use ordinary POSIX calls against owned local fixtures with
narrow fault injection; they cover candidate binding, no symlinks/special files,
full/excess/short/changed sources, whole-result cleanup, exact/shared budgets,
invalid preflight, allocation failure and a separate C caller. Native mode
freshly compiles C capture, unchanged C++ decoder and C caller with pinned SDK/
LLVM/resource inputs, links BIND_NOW, verifies used headers/libraries and inspects
actual source imports plus static/dynamic resolution. Pinned SDK libc supplies
`openat`/`fstatat` as static syscall wrappers, with remaining ordinary file APIs
dynamically imported. It never runs the ELF. Filesystem behavior, permission,
native symbol resolution and installed/running completeness remain unverified.

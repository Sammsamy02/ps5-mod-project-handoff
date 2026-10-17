# Native read-at feasibility review — 2026-09-30

**Verdict:** The report supports a narrow local feasibility finding: the pinned SDK headers and link stubs accept the ordinary `open`/`fstat`/`pread`/`close` route, and the desktop algorithm passes the stated fixture. It does not establish PS5 runtime behavior or a production-ready provider.

## Evidence checked

- The recorded SDK archive is 8,814,217 bytes with SHA-256 `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`. I found no mismatch across the 17 recorded SDK provenance paths, 17 linker inputs, 22 pinned SDK headers in the target dependency closure, or the five cached LLVM tools. The exact target declarations, flags, 64-bit signed `off_t`, and stat layout agree with the pinned headers and compiler-produced layout receipt.
- The 120,904-byte ELF matches SHA-256 `2c1f6a329de1d4218804e2a62f8045d4919b3970b8a00d1bc4683285557f7c72`. Its six dynamic undefined symbols are `__error`, `_exit`, `close`, `fstat`, `open`, and `pread`; the cached `libkernel_web.so` exports the four file calls and `__error` as SDK link stubs. This is link evidence only.
- I reran the original desktop fixture from copied sources in an isolated ignored scratch directory. All 66 checks passed with ASan and UBSan enabled, empty sanitizer output, no remaining owned tempfile, and an actual sparse-file read above 4 GiB. This independently supports the desktop algorithm and range/retry tests; it does not validate PS5 filesystem calls.
- The SDK link invocation selects `--build-id=uuid`; this ELF’s captured build ID is `843e008ac32c90ba7c8c679ed08c2306`. The SHA pins this particular artifact. Re-linking with the SDK’s UUID build-ID mode is not a byte-reproducibility check.

Evidence: [feasibility report](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-09-30-native-read-at-feasibility.md), [native receipt](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/native-receipt.json), and [independent desktop rerun receipt](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-review-scratch/desktop-receipt.json).

## Blockers to a PS5 or product claim

- The ELF was not executed on a PS5. Actual import resolution, startup, `open` permissions, `fstat`/`pread` semantics, filesystem offset limits, and descriptor lifecycle remain unobserved. The exports are SDK stubs, not runtime implementations. No current evidence supports “ready to install” or product behavior.
- The throwaway provider is not an integrated, versioned container ABI. File registration and path resolution are outside it; the argv-open fixture does not provide an approved product resolver. There is no production session registry or status integration.

## Unverified engineering limits

- Stability is correctly described as `checked_only`: the device/inode/size/mode/mtime/ctime checks can detect observed changes, but do not provide an immutable snapshot or catch every same-size write/revert, timestamp-granularity race, or pathname replacement. The descriptor and provider state are caller-owned, with no lifetime/refcount or synchronization mechanism in this API.
- The finite per-operation `fstat`/`pread` attempt count is useful but does not impose a hard deadline: any synchronous call can block. The spike has no cancellation, global callback/byte/work budget, cumulative page limit, or production open/close policy. A 96-byte provider and 64 KiB request cap do not prove a whole-inspection 8 MiB bound.
- `llvm-readelf` reports `invalid PT_DYNAMIC size (0x1048)` and falls back to `SHT_DYNAMIC` for the spike ELF. The same warning appears on an existing SDK-built loopback ELF; the pinned SDK linker script maps `.dynamic` into both `ph_rodata` and `ph_dyn`. The dynamic entries are inspectable, but target-loader acceptance of this layout remains unverified and should be separated from the file-API feasibility result.

## Research scope

The range subtraction, signed-offset rejection, 64 KiB cap, short-read/EINTR retry accounting, before/after stat comparison, and caller-owned descriptor cleanup claims match the source and fixture. The >4 GiB result is desktop-only. This is research evidence for a possible generic physical-file provider; it is not a game-specific patch or an installation recommendation.

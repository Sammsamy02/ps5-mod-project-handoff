# L18 local XOM candidate — frozen for parent review

All nine local host suites pass. The changed full target translation units and the three-object relocatable join pass. `bridge_code_cache_context` and `bridge_code_cache_signature` resolve strongly; the bridge/owner add no undefined imports versus l17, and the initializer adds only these two local providers. This is local evidence; no package, upload, console contact or console success is claimed.

The candidate copies frozen l17 inputs. Only the bridge, initializer and NativeFNameChecks changed in production, plus three standalone headers: CodeImageCache, CodeImageSha256 and CodeSignatureProfile. All 42 frozen l17 source/test/report/receipt pins remain unchanged, as do the retained l16 input pins. The l17 initializer ownership, immediately unlinked startup sinks, once-only descriptor retirement before Phase5, retained fault ledger and join-before-fallback cleanup are retained.

## Authenticated startup cache and routing

After loaded-image capture and successful native-facts conversion, before context construction or worker start, the bridge opens the configured profile path `/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin` with `O_RDONLY|O_NOFOLLOW`. Its bounded streaming cache computes the full SHA-256 and extracts both prefixes from that same opened stream. It rejects malformed or oversized ELF program-header tables, overflowing or truncated PT_LOAD bounds, overlapping virtual LOADs, missing file-backed executable coverage, zero-fill/BSS substitutes, SHA/prefix mismatch, wrong actual base/extent, nonprogress/trailing data, read errors and every nonzero close result. The image descriptor is consumed once before Close, never retried, and closed before worker construction. Open/Pread/Close reset errno first. Failures retain raw results, normalized errors, exact configured path/flags and reason diagnostics.

The profile inventories every selected known function pin: FName32 at runtime0x63d590/file0x241590 and staged SCO25 at runtime0x890aa0/file0x494aa0. Exactly57B are cached from PT_LOAD0. Caching SCO does not activate another signature check, scan or native call. FName ABI, constructor argument/output layout and behavioral pool roundtrip stay unchanged.

Both signature routes require this fresh whole-file identity and nonzero actual observed base == resolved module base == retained MainExe view base == profile0x400000. A named complete prefix must be in an authenticated immutable executable file-backed PT_LOAD; any obtained real module-info segment must contain the complete prefix and agree that it is executable and nonwritable. The selected `lmf_to_native_loader_facts.cpp:245-254` leaves the mapped scan ABI image_base0 while planting the resolved extent into ModuleImage MainExe.base. The selected `PlatformModuleSnapshot.hpp:82-105` validates and publishes that module.base into Views; no replacement base is invented.

A positive VQ Available result must fully contain the prefix, mark committed0x10 and EXEC4. Available noREAD1 and noWRITE2 then permits the authenticated static prefix. READ continues through fresh owned live copyin; Unavailable has only that existing live route. Unmapped, malformed/partial, nonexec/uncommitted/writable XOM, missing cache, base mismatch, recorded faults and readable EFAULT refuse. The log labels `authenticated-disk-XOM-signature` versus `fresh-live-copyin` and reports VQ fields, authenticated PT_LOAD/file mapping, observed/module bases, SHA and retained module segment. Logging reads only owned cache metadata, never unowned target text. A disk signature is not described as live-byte proof.

The immutable cache is owner-gated and survives initializer bank retirement. Failed start and stop after owner destruction/join clear it; no image FD persists. Foreign-thread cache/probe/capture/fault calls refuse before accessing unsynchronized bank/cache/ledger/nonce/telemetry state.

## Scheduling and cleanup

The absolute shared60000ms deadline begins before the first image open. Reads/hash and prefix extraction check it between operations; context preparation, worker scheduling, settings IO, admission and FName verification consume its remaining time. The initializer imports the exact original start/deadline and checks expiry before its data-slot read group and immediately before settings IO. No added duration or extension is introduced. OS calls and cleanup are not preemptible.

The independent l17 startup sink5000ms budget remains separate: it is checked before each acquisition iteration; the iteration can perform open, required unlink and diagnostics, and cleanup completes without preemption. Neither budget is a hard wall-clock interruption guarantee.

Malformed kernel module-info is rejected before cache preparation on the selected supported supplier route: `launch10-build/final-units/live_module_facts.geometryfix.cpp:84-103` validates count/address/size/ends before obtained=true; rejection preserves ModuleInfoRejected at415-421. `lmf_to_native_loader_facts.cpp:124-145` refuses that status. Contradictory successfully obtained segments also refuse the common initializer identity gate.

## Verification

Commands were run from `/WORKSPACE/.codex/worktrees/translation-core/what`:

```
python3 outputs/2026-10-07-l18-local-xom/tests/run_all.py
python3 outputs/2026-10-07-l18-local-xom/native/focused_compile.py
```

Individual commands and compiler arguments are recorded in the pinned receipts. Host suites include the unchanged retained bank/ABI/pool/data-admission tests,44 lifecycle assertions per plain/ASanUBSan/TSan mode using the actual selected RuntimeOwner,15 signature policy cases per plain/ASanUBSan mode,30 cache scenarios per plain/ASanUBSan mode,16 actual bridge/cache assertions per plain/ASanUBSan/TSan mode, and19 actual initializer scenarios per plain/ASanUBSan mode. Both sanitizer families use fatal recovery settings where applicable. TSan exercises supported ownership publication, foreign refusal and joined cleanup; the same owner protocol excludes concurrent unsupported cache mutation.

The actual source-extracted initializer tests retain the old9 success/refusal/exception cases. Added cases prove XOM succeeds with zero target probes before its same-ABI host constructor seam, readable EFAULT retains evidence and never calls the constructor, Unavailable still requires fresh live copyin, SHA/cache/view/deadline refusal precedes data admission, and contradictory module/LOAD facts precede either signature route. Advancing time during excluded Phase2 work proves the actual pre-settings guard makes zero settings-reader calls and therefore zero settings opens. Its RED receipt records the prior invocation after expiry. The source extraction excludes only the old data-slot Phase2 and replaces the OS/clock/direct CPU-read/native-call boundaries with disclosed fixture seams; it does not run game code on the Mac.

A production C++ Cache::prepare also authenticated the retained194241424-byte image, independently SHA-checked as `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, with one open/one close,11857 reads, exact offsets and57B accounting. The final Mac runs took641ms normally and722ms with fatal ASan/UBSan. These timings are not PS5 predictions. Synthetic cases cover wrong SHA/base/offset/EOF, bounded expiry, acquisition/read errors and exception cleanup. Actual bridge FD-reuse sentinels cover -1/errno0 and positive Close results; repeat cleanup never closes the unrelated reused descriptor.

The initial focused target run compiled l17/l18 bridge, owner and initializer and joined the l18 objects. After the final deadline guard, the refreshed run reuses5 source/header/object hash-verified artifacts, recompiles only the changed initializer, and repeats join/import/relocation proof. Exact LLVM_CONFIG and commands are in NATIVE-FOCUS.json. No new OS IO/crypto import is required; only the existing Sony Open/Pread/Close are used.

## Limits and next gate

The configured disk path is pinned configuration, not a new console observation. The l16 harvest only proves a fresh32B FName copyin EFAULT; it does not prove VQ XOM protection. The future console run must log an available matching committed executable noREAD region for disk routing. Readable EFAULT never becomes disk success. Full-image PS5 hashing may consume the60s budget and refuse; no timing relaxation is hidden. Static authenticated disk bytes corroborate the pinned pristine image and observed placement; they do not claim live text-byte observation. Full composition, packaging and console dispatch remain the parent's reviewed pipeline step.

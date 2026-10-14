# Independent l15 FName adapter review

Disposition: **local source review PASS, no remaining serious ABI, memory-read or refusal blocker identified in the final reviewed source. Mac-only. Upload HOLD.** This is not console success or a certification of the still-running full deterministic pipeline.

Reviewed `NativeFNameAdapter.hpp`, `NativeFNameChecks.hpp`, `FNameAbiProfile.hpp` and their `UnrealInitializer.cpp` integration against l14 and the saved native constructor disassembly. Read worktree AGENTS, recovery POSTMORTEM, PS5 mission, and l15 plan. Reviewer made no shared source edits and contacted no console/network surface. Final exact source hashes are in `FINAL-SOURCE-PINS.json`.

## Correctness findings

- Native `Ctor` is `void(Name*, const char16_t*, uint32_t)`: explicit output first, UTF16 second, mode third. Saved instructions and real game callers establish RDI/RSI/EDX, NUL termination with no length argument, and one packed eight-byte output write. Return registers are ignored by the adapter.
- `Name` is eight-byte aligned, eight bytes long, and zero initialized before the native call. The retained `Function<FName(const CharType*, EFindName)>` binds to a correctly typed local thunk. Retained FName headers and their four-byte object alignment remain unchanged; the native buffer's alignment is asserted independently.
- The first call requires a **fresh successful** owned-copyin prefix probe (`==1`), a complete owned capture, exact byte comparison and fault-ledger veto. CPU text reads and disk-only/cache-only fallback are absent. Available VirtualQuery replies must describe full committed executable extent; an unavailable query may proceed only through fresh owned copyin proof.
- `ByteProperty` is passed with explicit Find=0. The returned number, pool entry bounds, block pointer, header and complete text are checked using the existing admitted exact-range read engine, then compared as UTF16. The independently admitted ByteProperty id must also match. Binding occurs only after these checks; there is no second native engine call in the bootstrap resolver.
- Existing pool image/heap admission policy is preserved. The roundtrip never grants a direct read based on a capture; every direct read still follows exact image-segment or VirtualQuery proof and respects prior faults.
- The shared 60,000ms budget is reused by prefix and pool resolution. Cancellation and expiry are checked immediately before first native invocation and final binding. A separate l15 binding elapsed/outcome receipt now covers the added work. Kernel/native calls already entered are not preemptible; this is a scheduling deadline.

## Findings corrected during review

1. A static assertion initially required public `alignof(FName)==8`, while pinned target compilation established public alignment 4. It now asserts the owned native output buffer's alignment and preserves the public ABI.
2. Signature verification initially accepted cached bytes and could bypass a fresh successful probe. It now always probes and requires result 1 before fetching bytes. The independent regression confirms cached matching bytes plus an unavailable fresh probe produce `admitted=0 probes=1 ctor_calls=0`.
3. Initial success logging could consume the remaining budget before native invocation/binding. Explicit final deadline/cancellation seams now guard both actions. A separate binding log avoids treating the earlier pool receipt as proof of constructor binding.

## Independent validation

- Final host adapter/checks: plain and AddressSanitizer/UndefinedBehaviorSanitizer builds compile and run successfully. Function sanitizer is not globally disabled in final checks. The deliberately invalid legacy ABI call is exercised only in the plain isolated child; final sanitizer adapter run defines `SKIP_LEGACY_NEGATIVE=1`. See `HOST-RERUN-FINAL.json`.
- Expanded checks cover 13 signature cases and seven roundtrip cases, including fresh probe, stale-cache/unavailable refusal, partial/missing/mismatch/fault/deadline, mapping extent/protection, ASCII and UTF16 pool entries, wrong id/number/text/length, and fault-vetoed reads. See the current checked source and final source pins.
- Independent pinned Prospero compilation of the final initializer exits 0, with only inherited warnings. See `TARGET-COMPILE-FINAL.json` and stderr. Object SHA-256: `6762ed4fd99b8c254c08a217c911ba8a9d946f3bc6ae19e8b4ac309f2142993d`. Initial independent attempt lacked LLVM_CONFIG; the final attempt uses the exact retained compiler configuration, without changing source.
- Saved ELF rehashed to `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`; extracted constructor prefix equals the profile. See `SAVED-ABI-RECHECK.json`. This verifies saved bytes only.

## Coverage and proof limits

The adapter retains ordinary Add=1 after the Find-only bootstrap. Real saved caller 0xf824 demonstrates that same output/UTF16/mode ABI with EDX=1. The insertion implementation and lock side effects are not exhaustively proven here. The initial ctor audit's Find-only recommendation/profile facts should distinguish bootstrap policy from retained ordinary Add support; host fake-ctor tests currently focus on mode 0. These are documentation/coverage follow-ups, not a newly demonstrated wrong ABI.

Host gate tests mostly test the pure helpers; cancellation and startup integration order are additionally source-reviewed and target-compiled. Target hardware execution, executable-text copyin availability, engine concurrency/side effects, and all other game call families remain unproved by this review. A 32-byte prefix does not establish the entire live function or eliminate modifications after capture.

The main executor owns the two fresh launch11-derived builds, import census/delta, packaging, deterministic hashes and unchanged gate receipts. Review this report alongside those final receipts before treating the local build work as complete. **No upload, payload, launch, pad edit, protection change, or console validation was performed. Upload remains HOLD.**

# Stage 42: verified GS segment backend candidate

**Status:** isolated backend code and PS5 SDK binding compile and link offline. Runtime GS switching on a console remains unverified, so the default unavailable backend is still the safe production path.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ps5-gs-backend-20261013-42`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ps5-loader-build-20261013-41`

`VerifiedGsSegmentSwitch` implements the existing `GuestSegmentSwitch` interface with injected GS read/write operations. Before guest entry, it saves the current thread's GS base, writes the guest TEB address, and reads the base back. On refusal after a partial write it restores and verifies the original. On leave it restores and verifies again. A failed restoration stops the process, since continuing with an unknown GS view would be unsafe. Same-TEB nested entry still uses the existing `ReentrantGuestSegmentSwitch`; direct nested physical entry refuses. The new `ps5_sysarch_gs_operations()` binds to the pinned SDK's `sysarch(AMD64_GET_GSBASE / AMD64_SET_GSBASE, ...)` on `__PROSPERO__`. A caller must explicitly select this candidate; no original DLL is admitted by default.

The focused Mac fake-GS suite passed **14/14**, including failed reads, a write that changed GS but reported failure, wrong readback, nested entry, a synthetic Win64 DLL lifecycle call that saw the guest view, and a child-process check that unrecoverable restoration aborts. The same **14/14** passed under AddressSanitizer. All **15** previous regression runners exited 0, and the original synthetic PE fixture passed **11/11**. The four-file patch passed exact-stage-41 `git apply --check`, scratch application, byte-for-byte replay, and ZIP CRC verification.

All **13** loader C++/assembly translation units compiled with `-Werror` for `x86_64-sie-ps5` and linked in an offline ELF that references the new binding. The target ELF imports `sysarch`; the pinned SDK also exposes its constants and symbol. This proves toolchain compatibility, not that the console permits or correctly preserves GS changes. The target build bundle includes source/header hashes, object files, ELF, compiler/link logs, and symbol inspection.

- [Four-file source and test patch](ps5-gs-backend-increment-20261013.patch)
- [Exact base and stage manifest](ps5-gs-backend-manifest-20261013.json)
- [Host regressions, ASan, and replay evidence](ps5-gs-backend-host-evidence-20261013.zip)
- [PS5 target build manifest](ps5-gs-backend-target-build-manifest-20261013.json)
- [Target objects, ELF, and logs](ps5-gs-backend-target-build-evidence-20261013.zip)

The [prepared no-game GS round-trip probe and console card](GS-ROUNDTRIP-CONSOLE-CARD-20261013.md) make the first console gate concrete: record host GS, set a local guest TEB, verify `GS:[0x30]` and `GS:[0x58]`, restore host GS, and verify the original value. A second gate must exercise the loader lifecycle with a synthetic PE before trying unchanged gameplay DLLs. Console permission/cache behavior, Windows CRT/SEH and framework imports, target matching, and gameplay effects remain separate blockers. No console, shared candidate, kernel code, Claude-owned capability, or original DLL was changed.

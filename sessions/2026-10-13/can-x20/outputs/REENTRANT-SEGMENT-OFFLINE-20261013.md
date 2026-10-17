# Stage 40: reentrant guest segment boundary

**Status:** isolated Mac host work; no PS5 segment backend or console-only readiness.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-reentrant-segment-20261013-40`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-nested-tls-view-20261013-39`

`ReentrantGuestSegmentSwitch` wraps a caller-supplied backend. On one thread, its first entry publishes the guest TEB; nested entries with the same TEB increase a depth count without invoking the backend again. A different nested TEB refuses. The final leave restores the backend once. If a backend throws after changing the segment, the wrapper asks it to restore the host view before refusing. The caller must share one wrapper across a root PE and its dynamically loaded dependencies, and must balance every entry and leave.

The focused TEB suite passed **21/21**, including AddressSanitizer. A copied synthetic PE imported `LoadLibraryA` and called it from DllMain using a name literal in its mapped image. That call loaded a neighboring unchanged synthetic DLL while a strict backend, which rejects a second entry, observed only one segment transition for the nested attach. Both DLLs detached with balanced restoration. The focused batch suite passed **52/52**. These are Mac host checks with a mock backend, not PS5 GS evidence or real gameplay DLL execution.

All **15** offline regression runners exited 0; the original synthetic fixture passed **11/11**. The four-file patch passed exact-stage-39 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](reentrant-segment-increment-20261013.patch)
- [Base/stage hash manifest](reentrant-segment-manifest-20261013.json)
- [Regression, ASan, and replay evidence](reentrant-segment-evidence-20261013.zip)

The production segment backend still refuses entry. Real CRT/SEH behavior, wider import coverage, `FreeLibrary`, target correspondence, and console gameplay evidence remain open. No console, shared candidate, kernel file, or original mod binary was changed.

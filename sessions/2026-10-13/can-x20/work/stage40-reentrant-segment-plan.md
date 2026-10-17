# Stage 40 reentrant segment boundary

Goal: make nested guarded PE lifecycle entry use one host-to-guest segment transition, while rejecting a different TEB at the nested boundary.

Add `ReentrantGuestSegmentSwitch` around an existing `GuestSegmentSwitch`. Per-thread state tracks depth and the active TEB. First entry invokes the backend; same-TEB nested entries only increase depth; the final leave restores the backend. A failed backend entry publishes no active frame. The caller must share one wrapper across the root PE and dynamically loaded dependencies. This does not implement the PS5 GS backend.

Test first: a strict backend that rejects nested entry should see one enter/leave pair through the wrapper, and a different nested TEB should refuse. Then use a copied synthetic PE whose DllMain imports and calls `LoadLibraryA` with a literal in its mapped image. Load its neighboring unchanged synthetic DLL through the stage-38 source using the same wrapper; prove both modules attach, and detach in safe order. Run focused and full regressions, original fixture, and exact-stage-39 patch replay.

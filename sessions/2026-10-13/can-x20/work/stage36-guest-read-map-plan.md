# Stage 36: bounded guest-memory reads for mapped PE image strings

Base: isolated stage 35. Goal alignment: automatically adapt unchanged DLLs without exposing arbitrary host memory or per-game addresses. Add a generic reader whose allowlist follows attached mapped PE images; use it for stage 34's GetModuleHandleA/GetProcAddress. This is a foundation, not full heap/stack guest-memory coverage.

Create GuestReadMap with bounded, non-overlapping owner ranges and validated copy. WinApiShim can install it as its guest reader before PE preparation. Successful mapped-module registration adds the complete mapped PE image range before DllMain; failed registration/attachment and detach remove it. All mapped pages are PROT_READ in the current loader. Preserve existing external reader hook behavior and reviewed namespace rules.

Test first: range bounds, integer wrap, overlap, owner cleanup; copied synthetic PE imports a dynamic lookup API and can pass a string literal in its image during DllMain without an external test reader; detached provider's image no longer readable. Keep original fixture hashes unchanged. Run 15 regressions, synthetic E2E, exact-base patch replay, no PS5 claim.

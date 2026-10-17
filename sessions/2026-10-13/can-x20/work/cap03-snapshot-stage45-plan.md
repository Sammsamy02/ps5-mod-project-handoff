# Stage 45 expected-original patch guard

Base: exact stage 44. The detour planner reads an original instruction
snapshot; CodePatch currently checks only that the target is not already
equal to the replacement. A stale plan could overwrite different code.

Add a separate `patch_code_if_original_ex` entry that requires a caller
snapshot and preserves the existing `patch_code_ex` signature and results.
Compare after preflight, before mprotect, and re-check after verified OPEN
immediately before WRITE. A mismatch must not write; preserve/restore the
page protection and return a distinct result. Copy the caller's expected
bytes early so an aliased expected pointer cannot track target mutations.

Use mock-injected host tests for stale-before-open, stale-after-open, and
successful checked patch/restore. Then run stage 44 focused detour checks,
the full host protocol, PS5 compile/link, and exact-base replay. No game or
console run.

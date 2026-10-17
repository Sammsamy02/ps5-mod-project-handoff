# Stage 60: guarded VirtualAlloc / VirtualFree

Base: exact stage 59. Scope: publish Win64 imports only after both guest maps bind. Own anonymous page mappings; support common reserve/commit, reserve then whole-region commit, and base-address release. Admit read-only and read/write non-executable protection; refuse unsupported fixed-address, partial-region, decommit, executable and advanced modes. Track ownership and revoke map ranges before release. Keep source isolated.

Verification: focused host and ASan tests; prior heap and thread checks; 15 prior runners; unchanged fixture; pinned PS5 compile/link; exact-base patch replay and evidence CRC. Do not claim console execution or universal DLL compatibility.

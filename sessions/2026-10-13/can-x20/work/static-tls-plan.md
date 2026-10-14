# Static PE TLS state implementation plan

**Goal:** Prepare the platform-neutral state required by unchanged Windows x64 DLLs with static TLS, without claiming their `GS` instructions run yet.

**Design basis:** Microsoft PE format defines an initialized template plus zero fill, a loader-assigned module index, and a per-thread array of module block pointers. The current PE loader refuses real TLS before mapping. This component models the template/index/thread-array part behind an explicit attach call; a later guest TEB backend must publish the array at Windows `GS:[0x58]` and a Win64 callback invoker must handle the callback lifecycle.

**Files:** Add `loader/src/static_tls_bank.h`, `loader/src/static_tls_bank.cpp`, and `loader/tests/static_tls_bank_test.cpp` in isolated `cg-static-tls-core-20261013-09`. Do not edit the shared candidate or source DLL binaries.

**Interfaces:** `register_module(initial_bytes, zero_fill)` assigns a monotonic index; `attach_current_thread()` returns a snapshot of per-module pointers and an epoch; `current_block(index)` returns a pointer only after attachment. Retained modules are never unregistered, so indices never reuse. An empty template still has a non-null distinct block. Existing blocks retain mutations when the same thread reattaches after a late module registration.

**Tests:** Start with failing tests for template/zero-fill, thread isolation, late registration, bank-instance isolation, and overflow refusal. Implement the smallest code to pass. Build with host x86-64 clang and `-Wall -Wextra -Werror`; run tests twice, then package an exact-base incremental patch and evidence.

**Integration limits:** No mapped-image TLS index write, no `GS` base change or instruction rewriting, no callback execution, no Windows exception or CRT behavior. Keep the loader's refusal until all prerequisites are measured and verified.

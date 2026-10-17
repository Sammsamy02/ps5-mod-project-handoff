# x64 S-list head initialization

Stage `cg-slist-init-20261013-21`, copied from isolated stage 20. Shared candidate and unchanged DLL binaries remain untouched.

All five unchanged DLL samples statically import `InitializeSListHead`. Microsoft's x64 SDK `SLIST_HEADER` is 16 bytes and 16-byte aligned. Add its exact one-pointer Win64 row; require non-null 16-byte alignment, initialize exactly 16 bytes to zero, and fail closed on an invalid pointer/alignment. Do not publish push/pop/flush APIs without separate semantics. Test through the sealed lazy bridge with canaries on both sides, reinitialization, and alignment refusal. Run prior regressions and package exact-base patch/evidence.

This does not validate guest code that manipulates the opaque header directly, nor does it remove the loader's TLS/GS or unrelated-import refusals.

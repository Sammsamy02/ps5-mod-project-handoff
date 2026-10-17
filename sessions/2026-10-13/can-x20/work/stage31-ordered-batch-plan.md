# Stage 31: dependency-ordered guarded DLL batch

Base: isolated stage 28. Work only in `cg-ordered-dll-batch-20261013-31`.

The caller supplies a bounded batch of unchanged PE byte buffers, their original source paths, and segment backends. The batch route may retry a DLL only while every unresolved import names another still-pending batch member. It attaches ready providers before consumers, then publishes existing guarded modules in attach order. An unsupported external import, duplicate basename, failed DllMain, or no-progress cycle is a named refusal. On failure, detach earlier modules in reverse order; preserve any module whose detach itself fails for explicit caller cleanup.

Verification: test consumer-first success with a copied synthetic PE, no-progress cycle, missing external import, duplicate basename, and rollback after a later DllMain refusal. Re-run the full offline regression set and exact-base patch replay. Original binaries and the shared candidate remain untouched.

# Stage 38 dynamic directory load adapter

Goal: connect stage 37's `LoadLibraryA` hook to the existing bounded dependency source and closure, so a guest DLL can request a nearby unchanged PE by basename without per-game names.

Add `pe_load_guest_from_source` and a caller-owned context holding the source callback, shim, load options, TLS bank, guest TEB, and a pre-reserved vector of retained batch results. The callback fetches the requested root, verifies its basename, attaches its static dependency closure, retains all attached modules, and returns the root mapped HMODULE. On failure it reports a reason and retains any rollback-failed modules for explicit cleanup. The caller must quiesce guest threads and detach retained batches before destruction. A guarded `GuestPeModule::mapped_handle` accessor supplies the root HMODULE only after attachment.

Test first with a real temporary directory containing an unchanged synthetic fixture. Call the sealed Win64 `LoadLibraryA` wrapper with a registered guest-readable string, prove the resulting handle and export are live, then detach and prove lookup/read range revocation. Test wrong-basename source refusal before entry. Run focused and full regressions, original fixture, and exact-stage-37 patch replay.

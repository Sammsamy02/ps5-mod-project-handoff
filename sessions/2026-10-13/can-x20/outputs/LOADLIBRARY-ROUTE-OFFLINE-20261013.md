# Stage 37: guarded LoadLibraryA entry route

**Status:** isolated Mac host work; a dynamic loader hook must still be supplied. This is not a console-only wait.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-loadlibrary-route-20261013-37`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-read-map-20261013-36`

The shim now offers a sealed Win64 `LoadLibraryA` wrapper when a validating guest reader is installed. It copies a bounded guest DLL basename, reuses already attached or reviewed built-in handles, and asks the caller's `Hooks.load_pe` callback to load an unknown basename. The callback result is accepted only when the same normalized basename appears in the module registry with the returned handle. Paths, invalid pointers, absent callbacks, and mismatched handles refuse. The callback runs outside the registry lock so it can attach a PE and register it.

The focused dynamic-lookup suite passed **26/26** checks after an initial test reproduced the missing-wrapper refusal. It exercised existing handles, a callback registering a new module, repeat lookup without reloading, callable export lookup, mismatched callback handle, path traversal, and invalid guest pointers. This was a host callback test; it did not load a real CRT or gameplay DLL dynamically.

All **15** offline regression runners exited 0. The original unchanged synthetic end-to-end fixture passed **11/11**. The three-file patch passed `git apply --check`, exact-stage-36 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](loadlibrary-route-increment-20261013.patch)
- [Base/stage hash manifest](loadlibrary-route-manifest-20261013.json)
- [Regression and replay evidence](loadlibrary-route-evidence-20261013.zip)

The shim does not provide a directory-backed callback, `FreeLibrary` reference counting, guest heap/stack reads, a console segment backend, or Windows CRT behavior. The callback must complete and retain a successful attachment before returning its handle. No console, shared candidate, kernel file, or original mod binary was changed.

# Stage 37 LoadLibraryA route

Goal: let unchanged DLL code request a dynamically named local module through the existing guarded loader hook, with exact registry and pointer checks.

The shim publishes a sealed Win64 `LoadLibraryA` wrapper only after a validating guest reader is configured. It copies a bounded basename, normalizes an optional `.dll` suffix, and returns an already registered handle immediately. For an unknown basename, it invokes `Hooks.load_pe` without holding the registry mutex. The result is accepted only if the same normalized basename is registered with that exact handle. Missing loaders, failed callbacks, paths, invalid guest pointers, and mismatched handles refuse with a Win32 error. This stage does not claim a directory-backed hook implementation or `FreeLibrary` semantics.

Test first in `loader/tests/dynamic_module_lookup_test.cpp`: no-reader refusal; attached module reuse; missing hook; callback-driven registration and repeat reuse; mismatched returned handle; path and pointer refusal; call a dynamically loaded module export. Then implement only the shim row and rerun all regression runners, original fixture, and exact stage-36 patch replay.

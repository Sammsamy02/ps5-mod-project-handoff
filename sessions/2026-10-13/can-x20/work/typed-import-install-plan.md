# Typed, sealed PE import installation

**Goal:** Connect the context-bound bridge to the Windows shim without exposing raw host handlers, mismatched signatures, or writable/unsealed trampolines to a PE IAT. Keep this reusable across games and DLLs.

**Stage:** `cg-typed-import-install-20261013-15`, copied from isolated bridge stage `14`. No shared candidate or Claude edits.

**Design:** Replace the public arbitrary-pointer publication function with `install_builtin_integer_wrapper(dll, symbol, writable_slot, slot_size, seal_callback, user)`. The shim's internal catalog records integer guest arity and an explicit ready flag. Only reviewed `Sleep` and `GetTickCount` rows start ready. The installer checks the name and memory bounds, stamps a context-bound trampoline using the catalog's arity and target, asks the caller to seal the memory read/execute, and publishes it under the registry lock only after sealing succeeds. Unsupported, unknown, already published, and seal-failure cases refuse. Caller retains executable memory for the process lifetime. The loader itself continues to refuse real TLS/GS.

**Tests:** First reproduce current raw-pointer publication and missing typed installer as a failing host check. Then test raw refusal, unsupported-row refusal, unknown/short/null inputs, failed seal with no publication, successful Mac RW->RX installation, actual `Sleep` and `GetTickCount` calls through the Win64 bridge, duplicate publication, and registered-module reentrant lookup. Run existing bridge, TLS, preflight, and synthetic fixture regressions. Package exact-base patch and logs.

**Limits:** A successful Mac seal callback proves only that the host test used executable memory. PS5 permission and cache behavior remain a console gate. Other Windows/CRT/UE4SS APIs and mixed signatures remain unresolved.

# Windows DLL entry lifecycle

Continue the saved restricted PE runtime with known-signature startup/shutdown.
The user authorized local autonomous development with focused checks. Console
tests and genuine UE4SS game integration remain pending; no previous stopped
probing or placement action is retried.

Keep the existing no-entry open API unchanged. Add explicit entry-enabled open
with a native C callback invoking the fixed Windows entry signature. The concrete
bridge compiles through the already-verified typed LLVM IR path, preserving an
indirect `win64cc` call while exposing a native C API to the module owner.

After all imports bind and memory is sealed, call dynamic process attach with
the image base, reason 1 and NULL reserved. Publish exports only after a nonzero
BOOL return. A zero return causes exactly one process-detach call before cleanup.
On explicit unload, drain leases, call detach once, then release memory. Repeated
release failures preserve both ownership and original diagnostics and never call
detach again. Thread notifications, process-exit/static-load behavior, exceptions,
TLS, CRT initialization and process-wide Windows loader locking are outside this
slice. Caller serialization covers the complete loader domain.

The primary semantic reference is Microsoft's DllMain documentation:
https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain .

- [x] Implement typed entry bridge and actual cross-ABI tests/PS5 IR object build.
- [x] Extend module ownership with attach/detach lifecycle and focused tests.
- [x] Execute owned compiler-built success/rejection DLLs, observing startup and
  detach state before release, including failed-release retries.
- [ ] Review, preserve source-bound receipts/checkpoint, and publish privately.

## Next observed Windows imports

Retained import metadata identifies `wcslen`, `wcscmp`, `wcsncmp` and `wcscpy`
from `api-ms-win-crt-string-l1-1-0.dll`. Extend the real service binder with those
four code-unit operations using explicit 16-bit values, never native `wchar_t`.
Use known Microsoft x64 signatures with 64-bit counts and 32-bit comparisons.
Preserve exact library/symbol binding and unresolved results for unknown pairs.

- [x] Implement the four primitives with focused ABI/edge tests and a PS5 object.
- [x] Compile and execute an owned Windows DLL that imports all four through
  the actual module owner and service binder; preserve source-bound evidence.
- [ ] Review semantics and include the coherent source in the private backup.

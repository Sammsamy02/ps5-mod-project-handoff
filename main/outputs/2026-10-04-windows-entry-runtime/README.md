# Windows DLL entry lifecycle

The restricted PE module owner now initializes a Windows DLL through its actual
entry point and shuts it down before releasing memory. The entry-call bridge
preserves the Microsoft x64 calling convention while exposing a native C API.
This is shared runtime code: no game title, mod name or private installation
recipe occurs in the production implementation.

## Verified results

- Two compiler-built Windows DLLs executed locally through the production
  module owner and entry bridge, across four lifecycle cases.
- Successful attach publishes callable exports; rejected attach receives
  detach before cleanup. The image base and reserved argument are checked by
  actual Windows-compiled code, including a relocated image-base pointer.
- A live export lease prevents unload. Detach runs exactly once even when the
  memory release operation fails and must be retried.
- The original six-import, no-entry DLL still executes and returns its expected
  string and mixed integer/floating-point result after the lifecycle extension.
- Six direct cross-ABI entry calls pass. The entry bridge compiles into an
  880-byte PS5 object with no undefined symbols. The module owner compiles
  directly into a 13,008-byte PS5 C object. Focused owner tests also pass under
  ASan/UBSan. Both code and fixture evidence received independent review.

The actual DLL execution here occurred in a local x86_64 Mac process. PS5 object
compilation establishes build compatibility only. No downloaded DLL was run;
no console or game/system files were contacted or changed.

## Integration and limits

Use `mb_pe_module_open_with_entry` with `mb_win_call_entry` as its entry callback.
The existing `mb_pe_module_open` remains restricted to zero-entry images.
Memory operations and import binding stay caller-owned. Entry functions must
return normally and synchronously; this profile does not support entry-created
threads, escaping callbacks, thread notifications, TLS, exception/unwind
registration, CRT initialization, loader locking or additional Windows APIs.

Entry behavior follows Microsoft's dynamic-load
[DllMain contract](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
Calling arbitrary DLLs or calling game functions is not established by these
fixtures. UE4SS still requires a genuine native game-context provider, validated
Unreal layouts and actual game-thread execution before Item Spawner can work.

Receipts preserve commands, hashes and the distinction between host execution
and target compilation. The source manifest lists only the coherent code and
evidence selected for the private backup; original captures and frozen console
artifacts remain unchanged.

## Observed wide-string import coverage

Added `wcslen`, `wcscmp`, `wcsncmp` and `wcscpy` from the exact library
`api-ms-win-crt-string-l1-1-0.dll`. These five import slots are present in retained
original mod metadata; the source README records their file names, IAT RVAs and
the metadata hash. Implementations use explicit unsigned 16-bit code units,
64-bit sizes and Microsoft x64 calls. No locale or Unicode decoding is implied.

Fourteen service test groups pass, including the original six operations,
wide-string boundaries and nonvolatile-register preservation. The service object
now contains ten Windows-ABI definitions, is 4,872 bytes and has no undefined
symbols. A separate owned Windows DLL actually calls all four new imports
through the production owner/binder, copies the expected surrogate/high-unit
string, observes guard-page bounds and unloads successfully.

The next bounded service family is byte search (`memchr`, `strchr`, `strrchr`,
`strstr`) from the observed `VCRUNTIME140.dll` imports. Beyond those individual
primitives, Windows thread/TLS, allocation ownership, exceptions and game access
remain separate work. This does not make the original UE4SS DLL executable yet.

# Native C++ mod source route

`lifecycle_mod.cpp` derives from the real pinned `RC::CppUserModBase` header and
exports the exact lifecycle functions consumed by `CppMod`: `start_mod()`
returns a base pointer and `uninstall_mod(base*)` destroys it. Its callbacks
only increment counters. It uses no game addresses, hooks, files, or input.

Run `python3 outputs/ps5-mod-bridge/native-cppmod-source/build.py` from this
workspace to reuse the recorded native compiler profile. The recipe compiles
the mod and genuine host base implementation, audits their symbols against the
supplied runtime object, and creates a local source/object audit ZIP. It records
tool arguments and dependency hashes. `--compile-receipt`, `--host-runtime`, and
`--output` can select newer coherent builds explicitly.

The base constructor/destructor are out-of-line definitions in pinned upstream
`UE4SS/src/Mod/CppUserModBase.cpp`; they were absent from the earlier runtime
closure. The canonical copy preserves them and refuses unavailable native GUI
registration before retaining a tab. Input behavior is unchanged.

`child_runtime.cpp` supplies reusable child entry ownership. The genuine plugin
acquires a lifetime count before constructing an object and releases it only after
construction failure or successful destruction. Entry runs its own init array once;
stop requires no live objects, traverses the fini array in reverse, finalizes only
its own DSO, and retires its registration with the genuine host unwinder. Partial
finalization is not retried. Parent quiescence remains mandatory, and no kernel
stop-result or cross-module exception behavior has been proved on target.

The source recipe emits native relocatable objects, not a runnable module.
`native-cppmod-contract/build.py` composes the child runtime with this real plugin
and supplies the corresponding host export/plugin import packaging, preserving
function/data types and selecting one owner for constructor traversal. See that
recipe for local package checks and remaining runtime requirements. Wrapping an
object or renaming a Windows DLL cannot supply these source/ABI dependencies.

Actual `CppMod` discovery now prefers `main.prx`, `main.sprx`, `<mod>.prx`, then
`<mod>.sprx`; legacy `main.dll` and `<mod>.dll` names follow. The real native
module owner continues to refuse PE content. Runtime activation/inventory has separate source and receipts; this source
recipe performs no module loading.

Focused tests live under `tests/`: filename selection and actual module-owner
PE refusal run only in owned temporary directories; GUI refusal runs the actual
method body with host-only GUI test doubles. No test doubles enter the artifact.
The recipe does not load native objects, contact a console, or place game files.

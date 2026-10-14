# Native C++ host/plugin contract

This local recipe compiles the genuine `native-cppmod-source/lifecycle_mod.cpp`
and its reusable `child_runtime.cpp`, then packages them against explicit exports
from the real coherent UE4SS runtime object. The plugin still uses the exact
`start_mod()` / `uninstall_mod(CppUserModBase*)` ABI consumed by `CppMod`.
No canonical runtime source, Claude activation reader, game provider or binary
DLL translator is changed by this recipe.

The child owns constructor and destructor traversal. Its real `module_start`
and `module_stop` functions are selected as `DT_INIT` and `DT_FINI`. The narrow
`child-writer.patch` preserves actual array sections and assigns their linker
boundaries, but publishes empty loader-array spans so those constructors cannot
also be traversed through the dynamic array tags. Existing default library
emission is unchanged for the same host inputs. The patch applies to a copied
writer source tree in each new output directory; its retained base is untouched.

Before constructors, the child registers its own complete exception-frame section
through the actual host's `__unw_add_dynamic_eh_frame_section`. It retires that
registration after reverse fini traversal and `__cxa_finalize(&__dso_handle)`.
An interrupted finalization remains failed and is never replayed. The genuine
plugin acquires a child lifetime count before constructing its object and releases
it after failed construction or successful deletion. Stop refuses while this
count is nonzero. The parent still must retire callbacks and serialize all calls
against unload: the counter does not track arbitrary executing code. Kernel
honoring a nonzero child stop result is unproved, and the current direct module
owner passes a null optional result pointer. Neither this guard nor local metadata
checks establish safe target unload.

The current child imports 17 actual host definitions, including the genuine
`CppUserModBase` constructor/destructor/typeinfo, matching C++ runtime and the two
unwind section functions. It imports `__cxa_atexit` and `__cxa_finalize` using
retained original-provider identities and exact SDK FUNC type evidence. Both RTTI
imports remain OBJECT. No system library identity is guessed. The host export
module must already be initialized and remain resident with this same ABI until
all children have finished; actual host entry/context integration is separate.

```sh
python3 outputs/ps5-mod-bridge/native-cppmod-source/tests/verify_child.py --output .superpowers/native-dependency-spike/native-cppmod-child-lifecycle-1/host
python3 outputs/ps5-mod-bridge/native-cppmod-contract/build.py
python3 outputs/ps5-mod-bridge/native-cppmod-contract/verify.py PATH_FROM_BUILD/receipt.json
```

`build.py` defaults to the direct-loader successor
`native-coherent-cpp-loader-1/build-1/coherent-cpp-loader-runtime.o`.
`--host-runtime` and `--compile-receipt` select an explicit newer genuine host and
its recorded compiler profile. Output directories have fresh UUIDs beneath
`native-cppmod-child-lifecycle-1/packages`. The compiler flags are retained.
The existing SDK, LLVM, .NET 10 macOS ARM64 SDK/cache, typed SharpProspero source,
prior provider declarations and genuine host object are prerequisites. No network,
installation or target action occurs.

`check_child_package.py` checks the emitted lifecycle exports and dynamic entry
addresses, empty loader arrays, exact exception-frame/index FDE set and count,
and the terminating zero after `__eh_frame_end`. `verify.py` checks all emitted
import identities/types, rejects malformed export/RTTI/child-entry selections,
compares default writer bytes with the retained predecessor, then uses the existing
SDK fake-SELF wrapper and retained extraction path to compare loadable segments.
Its `--baseline-tool` option can select a rebuilt predecessor ContractHost DLL;
the default names the retained prior successful package. The predecessor source
was saved under `native-cppmod-child-lifecycle-1/baseline/outputs/ps5-mod-bridge/`.
It builds using the same offline .NET commands and original typed writer DLLs
used by the previous source handoff.

The host fixture runs the production child TU using owned callback arrays and
finalizer/unwind stand-ins; it does not replace the genuine native plugin artifact
or execute its native C++ base. Native compilation and emitted metadata establish
source and packaging progress only. Native constructor invocation, cross-module
throw/catch, module acceptance, callback quiescence and loader result semantics
remain unproved. Runtime activation/inventory has separate source and receipts.
Existing weak host imports and previously documented provider qualifications remain.

Source-only handoffs exclude all generated modules, objects, DLLs and captured
provider binaries. The prior runtime handoff rebuilds the host object; this recipe
rebuilds the child and copied packager successor. Preserved failed/earlier run
receipts distinguish the original missing lifecycle exports and writer array
boundary failure from the subsequent successful package.

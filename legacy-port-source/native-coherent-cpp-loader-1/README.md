# Direct native C++ loader in the coherent runtime

The actual PS5 CppModModule now calls sceKernelLoadStartModule, sceKernelDlsym and sceKernelStopUnloadModule. This composition preserves the previously built full runtime, controller input, bootstrap and Claude activation. It recompiles both recorded consumers of the changed private header: CppMod.cpp and CppModModule.cpp. All other 357 runtime units retain their checked per-unit outputs. Constructor/link input order is preserved; the old composed runtime object is never linked as an input.

The result is a 116,576,216-byte relocatable object, SHA-256 `0ea73d04e95dbf23eb573ef0bdff4b9d114c9b2ad7331b4349e84e80838738bc`. Its unresolved symbols change from 199 to 197: the three native API imports replace __dladdr, __dlclose, __dlerror, __dlopen and __dlsym. Original retained libkernel export metadata independently identifies all three native names and their exact provider declaration; that is serialized export evidence, not a new console test.

The production loader change passed 14 focused groups using native-API stand-ins, the existing real host POSIX shared-library control, and native compilation. The component source review found no material issues. See ../native-cpp-module-direct-1/README.md for the API, handle-zero and failure-state behavior.

## Rebuild order

First reconstruct the original coherent-runtime source snapshot, compile it with the existing source-only recipe, and apply/build the retained pad overlay. Then apply the two canonical CppModModule source changes and run this recipe against the resulting pad receipt:

```sh
python3 .superpowers/native-dependency-spike/native-coherent-cpp-loader-1/build_runtime.py --base-receipt /path/to/rebuilt-pad/receipt.json --output /path/to/new-cpp-loader-build
```

Use a new output directory. The recipe accepts relocated fresh base outputs with the same pinned source/order contract; the historical composed runtime object is not required. Keep per-unit objects, their .d files, bootstrap objects and link input-hashes from the base build.

If all source overlays were applied before constructing the original source snapshot, the old two canonical files are retained verbatim under ../native-cpp-module-direct-1/baseline. Reconstruct the pinned original snapshot from those baseline bytes before applying the new files again. Do not change the original snapshot hashes to accept new source silently. The old handoff ZIPs also retain those source versions.

The output has no module_start entry and has not loaded a native mod or initialized Unreal on the console. Child PRX constructor/entry packaging, activation timing and file access, actual engine binding and runtime lifecycle still require integration. Arbitrary Windows DLLs remain unsupported. This change fixes the native source-plugin loader implementation; it does not change Claude's archive, installer or SDK-stamping components.

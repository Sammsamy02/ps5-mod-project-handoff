# Real matching Mono host obtained locally

The official Mono6.12.0.206 MDK was downloaded once after root explicitly raised its resource bound:370007450 bytes, SHA25680b0dbfa59ba9ed76dbf1393998e6a2ed2d1ccc8f5850c7a46fbe31a2aea88d8. PACKAGE-PIN.json binds the URL and verified content. XAR TOC shows one gzip/cpio Payload; selective bsdtar extraction retained runtime/header/config/corlib files, not the full developer distribution. No installer, pre/postinstall script, home/global installation or target build ran. Do not include this large dependency in the final handoff archive.

Root of extracted host:

`runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0/`

Executable `bin/mono-sgen64` is x86_64. Embedding library `lib/libmonosgen-2.0.1.dylib` is i386/x86_64 universal, not arm64. On this Darwin arm64 host, Rosetta executes x86_64 programs. Actual `arch -x86_64 <root>/bin/mono-sgen64 --version` returned6.12.0.206/0cbf0e290c3, amd64, interpreter enabled, SGen/hybrid. HOST-MONO-VERSION.log preserves output. Core dylib dependencies inspected with otool are macOS system libraries/frameworks. Its install name is the original absolute framework path, so a local embedding process needs an explicit process-local DYLD_LIBRARY_PATH=<root>/lib or suitable owner-controlled linking. Native arm64 cannot dlopen this library.

Headers: `<root>/include/mono-2.0`, with installed JIT header `mono/jit/jit.h` (source-tree owner currently uses `mono/mini/jit.h`). Runtime BCL: `<root>/lib/mono/4.5/mscorlib.dll`, distinct from compile-only v4.7.2 reference assemblies. Config: `<root>/etc/mono`. Probe.dll has no Main, so a CLI execution of that library is not a Run/throw/recovery test; an actual x86_64 embedding host must invoke its methods. Paths were sent to the cap10 owner. No managed probe was run by this researcher.

The official tag resolves to commit0cbf0e290c31adb476f9de0fa44b1d8829affa40, exactly the cap10 owner's selected Mono source commit. Public jit.h exposes INTERP, init/version/cleanup; mono-threads-api.h exposes unsafe-region enter/exit. Actual dylib exports for unsafe enter/exit, runtime_invoke, set_aot_mode, dllmap, fallback registration and image-open were checked with nm (HOST-C-API-EXPORTS.log). This provides a real compatible C API host candidate. It does not prove the production owner: stock MDK lacks its reserved-owned-library loader patch, and INTERP/AOT-wrapper behavior must be observed in the real host run. Reference corlib alone is not a runtime substitute. Optional GTK/LLVM/developer libraries were removed from the extracted subset; retained helper libraries include MonoPosix/IKVM/mono-native.

## Public FreeBSD portability is real, but not a target port

Pinned public mono-threads-freebsd.c supplies stack bounds via pthread_attr_get_np/pthread_attr_getstack and OS thread id via thr_self. Pinned mono-threads-posix.c uses actual pthread creation, signal-based suspend paths and process-wide memory-barrier/protection operations. Official platform documentation lists x86_64 FreeBSD JIT/interpreter; that is generic platform support, not an assertion about Prospero.

A PS5 port must still establish actual native thread attachment/stack bounds, TLS, GC suspend/root scanning, signal/context/EH layouts, synchronization/memory mapping permissions, interpreter/AOT native wrapper support, matching runtime corlib/config and symbol/provider lifetime. Matching x86_64 ISA/FreeBSD ancestry does not establish these contracts. No Sony/SDK/platform-extension material was used or sought.

Runtime/class libraries are generally MIT with third-party BSD/other exceptions in the pinned LICENSE; the MDK contains additional build/developer components with their own terms. LICENSE and package Resources/License.rtf were retained. No blanket license claim for all MDK contents.

Sources (snapshots/hashes in PUBLIC-SOURCES.json):

- https://www.mono-project.com/download/stable/
- https://formulae.brew.sh/api/cask/mono-mdk.json
- https://api.github.com/repos/mono/mono/git/ref/tags/mono-6.12.0.206
- https://github.com/mono/mono/issues/21825 (architecture report corroborated by actual extracted file inspection)
- https://www.mono-project.com/docs/about-mono/supported-platforms/
- https://raw.githubusercontent.com/mono/mono/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/jit.h
- https://raw.githubusercontent.com/mono/mono/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/utils/mono-threads-freebsd.c
- https://raw.githubusercontent.com/mono/mono/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/utils/mono-threads-posix.c
- https://raw.githubusercontent.com/mono/mono/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/utils/mono-threads-api.h

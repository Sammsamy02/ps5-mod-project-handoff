# Runtime support

The link step needs the ahead-of-time runtime — the garbage collector, exception handling, and the
bootstrap that runs before the managed entry. Nothing here is assembled by hand: the runtime is the
.NET SDK's own, and the toolchain meets its operating-system surface itself.

## Where the runtime comes from

The compile step (`dotnet publish -c Release -r linux-x64`) restores the standard NativeAOT runtime
archives into the .NET SDK's package cache as its own runtime pack
(`microsoft.netcore.app.runtime.nativeaot.linux-x64`). `build/build-app.ps1` gathers those archives
from the cache and hands them to the linker. On Windows the cache lives in WSL, so the archives are
copied out to the project's `obj` folder along the way.

## How the runtime's operating-system surface is met

The runtime archives call a set of C-library and operating-system functions. They are satisfied without
a platform layer of our own:

- **The device's C and kernel modules already publish most of them** — the whole `pthread` family, the
  memory-mapping and protection calls, the file and directory calls, timing, and the C library. The
  linker resolves these as ordinary imports against the module stubs it generates.
- **A compat object supplies the rest** — the small set the runtime asks for by a name the device does
  not publish (chiefly the large-file variants of the file calls, and a few glibc-only helpers). The
  linker emits this object itself: each entry is a thin forwarder to the name the device does publish,
  a fixed result an application module can accept, or a weak no-op the toolchain overrides. The set is
  recorded in `imports/compat.txt`; the emitter is `tools/SharpProspero.Link/CompatEmitter.cs`.

The linker also defines the section-boundary symbols the runtime reads to walk its own managed-code and
module tables (`__start_<section>` / `__stop_<section>`), the way the system linker does.

## The import lists

`imports/` records which names the runtime pulls in and where each resolves, as a reference:

| File | Names resolved against |
|---|---|
| `imports/libc.txt` | the device C module |
| `imports/libkernel.txt` | the device kernel module |
| `imports/compat.txt` | the compat object the linker emits |

The authoritative lists live in the toolchain: the C and kernel names are in
`tools/SharpProspero.Prx/StubCatalog.cs`, and the compat names in `CompatEmitter.cs`.

## Building

There is nothing to build here and no `PROSPERO_RUNTIME_PACK` to set. `build/build-app.ps1` does the
gathering and linking; a plain `dotnet build` of the solution and the tests need none of it.

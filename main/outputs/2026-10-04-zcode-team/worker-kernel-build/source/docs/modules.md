---
title: Modules and libraries
parent: Application Modules
nav_order: 11
---

# Modules and libraries

An application interacts with a library you supply as a `.prx`. You need only the module itself: the
SDK reads its exports, generates a wrapper for it, loads it at run time, and packs it with the
application. No other library or development kit is involved.

A library also comes signed, as a `.sprx`. The reader and the inspector take either form: a signed
module is unwrapped to its ELF first, so a `.sprx` reads the same way as a `.prx`. See
[Signed and unsigned](signed-and-unsigned.md) for what the forms are and how to convert them.

<details open markdown="block">
  <summary>On this page</summary>
  {: .text-delta }
- TOC
{:toc}
</details>

## Load a module at run time

Drop the `.prx` in the package's `sce_module` folder and load it by name. Resolve the exports you
call, then dispose the handle to unload.

```csharp
using SharpProspero.Modules;

using PrxModule lib = PrxModule.LoadFromPackage("mylib.prx");

// Resolve an export to an unmanaged function pointer and call it.
var doThing = (delegate* unmanaged<int, int>)lib.GetFunctionPointer("sceMyLibDoThing");
int result = doThing(42);
```

`LoadFromPackage("mylib.prx")` loads `/app0/sce_module/mylib.prx`. `GetExport` returns an address, or
`IntPtr.Zero` when the symbol is absent; `GetFunctionPointer` throws when it is missing.

## Inspect a module

List the exports of a module with the generator's `prx` command:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- prx --module mylib.prx --inspect
```

Each line reports the export's identifier and its library and module numbers. The identifier is
derived from the plain name, so a name you know maps to an export you can confirm.

For the module's structure, the `elf` command prints the header, the program headers, the modules it
depends on, and its export count; add `--exports` to list every export:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- elf --file mylib.prx --exports
```

This reads the module directly and needs no external inspector.

A module this toolchain writes also names its regions in a section table, so any tool that reads a
built module can read one of these — listing sections, disassembling a range, or dumping a table —
without the module having to be built any other way first.

## Generate a wrapper for a module

Write the exports you use, one per line, with an optional call signature, then generate a wrapper:

`names.txt`:

```
# name = returnType(parameterTypes)

sceMyLibDoThing = int(int)
sceMyLibReset = void()
sceMyLibTable
```

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- prx \
  --module mylib.prx --names names.txt --class MyLib --namespace My.App --out MyLib.g.cs
```

The generated `MyLib` loads the module and exposes each named export: a signed entry becomes a
callable function pointer, an unsigned one exposes its address. The generator verifies each name is
present in the module and warns about any that are not; add `--strict` to refuse to emit instead, so a
build script catches a wrapper that would bind a symbol the module cannot resolve. `--module-name` sets
the file name the wrapper loads at run time when it differs from the file being read.

```csharp
using My.App;

using MyLib lib = MyLib.Load();
int result = lib.sceMyLibDoThing(42);
```

## Build your own library as a PRX

An application project builds an `eboot.bin`. To build a library module instead, set the module kind:

```xml
<PropertyGroup>
  <ProsperoModuleKind>Prx</ProsperoModuleKind>
</PropertyGroup>
```

The link step then produces `<name>.prx` as a shared module rather than an executable. Place the
result in another application's `sce_module` folder and load it as above.

The module publishes itself, and the library its exports sit under, under the output file name without
its extension: `MyLib.prx` publishes `MyLib` and puts its exports in a library also called `MyLib`. The
build names the file after the assembly, so the assembly name is what a consumer resolves against. A
module publishes one export library.

When a consumer expects a different name, set either one on the link command:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- link --kind prx \
  --self-contained --obj mylib.o --export myLibDoThing \
  --publish-name libSceMyLib --export-library libSceMyLibCore --out mylib.prx
```

`--publish-name` sets the name the module publishes itself under. `--export-library` sets the library
the exports sit in, and takes the published name when left out. The project properties above do not
carry either; a build that needs them runs the link command itself.

For the module to expose functions, name the symbols it exports through `ProsperoExportSymbol`. These
are the unmanaged entry points (methods marked `[UnmanagedCallersOnly]`) another module imports:

```xml
<ItemGroup>
  <ProsperoExportSymbol Include="myLibDoThing" />
  <ProsperoExportSymbol Include="myLibReset" />
</ItemGroup>
```

The linker records each as an export, so a consumer resolves it by name. Confirm the exports on the
built module with `elf --file <name>.prx --exports`.

### What a library cannot carry

A library's thread-local block is placed after the blocks already loaded, so the distance from the
thread pointer is not settled when the module is linked. A library reaches a thread-local through a
pair of table slots the loader fills, which covers the general-dynamic and local-dynamic sequences.
The two forms that ask for a fixed distance from the thread pointer, initial-exec and local-exec, have
no such indirection, so the link refuses them rather than writing an offset that would read and write
another module's storage:

```
libfoo.o: a thread-local reference of the form this object uses cannot be written into a library.
```

Compile any object you add to the link yourself as position-independent code, so the compiler emits a
general-dynamic sequence rather than a fixed distance.

## Link against a module at build time

Run-time loading needs only the `.prx`. When you would rather bind a library at link time, generate
a stub for it and add it to the link. Point the stub at the module itself so it matches what the
module publishes:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- \
  stub --module mylib.prx --names names.txt --out libs/libMyLib_stub.a
```

```xml
<ItemGroup>
  <ProsperoUserStubLibrary Include="libs\libMyLib_stub.a" />
</ItemGroup>
```

The stub is a small object that carries the plain names and their identifiers, so the linker resolves
calls to the module that provides them.

### Versions have to match

A module records the module and library **version** of everything it imports, and the loader binds an
import only when that version matches the one the providing module publishes. Get it wrong and the
symbol does not bind.

`--module` reads the library name, the module name, the file the loader loads and both versions out of
the module, so the stub and the imports the linker writes agree with it by construction. `--lib`
instead assumes the usual versions (module 1.1, library 1) and that all three names are the same,
which is right for most libraries but is an assumption; use `--module` when you have the file. Either
way, `--module-version` and `--library-version` set the versions explicitly (hexadecimal, with or
without a leading `0x`: `--library-version 0003` and `--library-version 0x0003` are the same), and
`--module-name` and `--soname` set the other two names when they differ from the library.

This only applies to linking against a library. Loading a `.prx` at run time resolves each export by
its identifier and records no versions, so it binds whatever the library declares.

## The system version a module needs

A module records the system it was built against. An application that ships the module has to require
at least that much, or the system installs the application and then fails to load the module: the
package looks fine and the application breaks at run time.

The build settles this for you. After it gathers `sce_module`, it reads what every module needs, and
raises the application's requirement to match the highest. Ship a library built against 11.20 in an
application that asked for 02.00 and the requirement becomes 11.20 on its own.

Read what a module needs at any time:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- sysver --folder out/module
```

```
  libSceMyLib.prx                  11.20
  eboot.bin                        02.00

Current:  02.00
Modules:  11.20
Result:   11.20  0x1120000000000000
  Raised to 11.20 for libSceMyLib.prx.
```

Without `--apply` it only reports. With `--apply` it writes the result to `sce_sys/param.json`.

### Choosing the version yourself

`-SystemVersionPolicy` on `build-app.ps1`, or `--policy` on the command, picks how it is settled:

| Policy | What it does |
|---|---|
| `match` | Require what the modules need. Never lowers. The default. |
| `upgrade` | Raise the requirement to `--version`. Refuses a version below the current one. |
| `downgrade` | Lower the requirement to `--version`, and name every module that stops loading. |
| `keep` | Leave the requirement alone, and still name a module that needs more. |

Require a newer system than any module asks for, for example because your own code calls something
newer:

```powershell
.\build\build-app.ps1 -ProjectPath .\MyApp.csproj -SystemVersionPolicy Upgrade -SystemVersion 11.20
```

Target an older system than a module was built against:

```powershell
.\build\build-app.ps1 -ProjectPath .\MyApp.csproj -SystemVersionPolicy Downgrade -SystemVersion 02.00
```

Lowering the requirement does not change what a module needs. Anything built against something newer
is named:

```
  libSceMyLib.prx needs 11.20 and will not load under 02.00.
```

The build continues, because you asked for it, but that module will not load on a 02.00 system.
Whenever a module is left in that state the command reports it, whichever policy put it there.

A file in `sce_module` that cannot be read is reported rather than passed over, because its
requirement is unknown rather than absent:

```
  vendor.prx                       unreadable
```

## Packaging

Any `.prx` under the application's `sce_module` folder is packed with the application. The packager
copies the folder into the build alongside `eboot.bin` and `sce_sys`.

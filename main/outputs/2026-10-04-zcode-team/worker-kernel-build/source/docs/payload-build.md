---
title: Building a payload
parent: Payloads
nav_order: 2
---

# Building a payload

A payload is built by the same `build-app.ps1` pipeline that produces an application module, with
the `-Payload` switch. This page covers the command, the output layout, the project options a
payload uses, and how the payload build differs from an application-module build.

## The command

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-unjail/SampleApp.csproj -Payload -Output Folder
```

- `-Payload` switches the link to the payload kind. Without it, the build stops with an error asking
  for `--kind eboot` (the default for an application module).
- `-Output Folder` is the only supported output for a payload, and the default when `-Payload` is
  given. A payload does not carry `sce_sys` metadata and is not packed into a `*.pkg`.
- The output is a single `SampleApp.elf` (or `<AssemblyName>.elf`) under the project's `out/`
  folder.

Pass `-DiagnosticBreadcrumbs` to include the CRT's `sp:*` log checkpoints in the built ELF. Each
checkpoint issues `SYS_kexec` directly, so the line reaches the kernel log even when nothing else
is set up. Useful during device-log debugging, and safe to leave off for production.

## The project

A payload project is a C# library project with a small set of extra properties:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <PublishAot>true</PublishAot>
    <NativeLib>Static</NativeLib>
    <AssemblyName>SampleApp</AssemblyName>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="$(SharpProsperoRoot)/src/SharpProspero/SharpProspero.csproj" />
  </ItemGroup>
</Project>
```

- **`PublishAot=true`** and **`NativeLib=Static`** are what let the compiler produce a `.o` the SDK
  linker can turn into a payload. A payload is not an executable — it is a shared object the loader
  maps and jumps to — so the `NativeLib=Static` setting produces a static library the compile step
  emits.
- **`AllowUnsafeBlocks=true`** is required because every payload interop path uses raw pointers.
- **`AssemblyName`** decides the output filename.

Two payload-only project properties add per-payload dependencies. See
[SPRX declarations](payload-sprx.md).

| Property | What it does |
|---|---|
| `<ProsperoSprx Include="..." />` | Declares an extra system library the payload calls. Emitted as a `DT_NEEDED` in the linked ELF. |
| `<ProsperoKernelSprx>libkernel_sys.sprx</ProsperoKernelSprx>` | Overrides the default kernel library (`libkernel_web.sprx`). Use `libkernel_sys.sprx` for a payload that needs the kernel-side POSIX surface (`getmntinfo`, hardware info, and so on). |

## The output

A payload build writes:

- `out/SampleApp.elf` — the payload itself.
- `out/SampleApp.diag.elf` (when `-DiagnosticBreadcrumbs` is on) — a copy with the CRT breadcrumbs
  compiled in. Send this one when the device log has to show the bring-up sequence.

The `out/` folder is safe to delete and rebuild.

## What the linker does

The payload link is different from an application-module link:

- **Kind.** `--kind payload` writes an `ET_DYN` ELF with a `PT_DYNAMIC` segment holding the imports,
  the relocation tables, and the symbol tables. An application-module link writes an `eboot.bin`
  with a section table sized for the console's own program loader.
- **Start code.** `PayloadCrtEmitter` writes the payload's start object. The application-module link
  uses a different start object — `CrtEmitter` — that expects the console loader to have set up
  imports and thread-local storage before calling `main`.
- **Compat object.** The compatibility object that bridges the C runtime extends each `mprotect`
  length by an extra page for a payload build, so the collector's straddled bookkeeping page is
  covered. An application-module build uses the plain length.
- **Section headers.** A payload includes no section table. The loader reads only the segment
  headers.
- **DT_NEEDED.** The default set is `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`
  — plus every entry from `<ProsperoSprx>` items in the project. `<ProsperoKernelSprx>` replaces
  the first entry.

The application-module and payload code paths through the toolchain are kept strictly separate, so
a change on one side does not affect a build on the other. See [Build pipeline](build-pipeline.md)
for the three-step pipeline.

## Rebuilding after a CRT change

A change to the CRT (`SharpProspero.Link/PayloadCrtEmitter.cs`), the payload writer, the linker, or
the compat emitter is enough to invalidate every built payload. Rebuild the samples you rely on
before sending them. Compare the built ELF against the SDK sample corpus with the `elf` command:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- elf --file out/SampleApp.elf --sizes
```

## Iterating

While iterating on a payload, keep the console listening on the loader port and send the fresh
build over each cycle:

1. Edit the payload's `Program.cs`.
2. Rebuild: `pwsh build/build-app.ps1 -ProjectPath <project> -Payload -Output Folder`.
3. Send: `dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send --host <ip> --file out/<name>.elf`.
4. Watch the device log for the `sp:*` breadcrumbs and the payload's own output.

The default send port and the wire format are covered in [Sending a payload](payload-deploy.md).

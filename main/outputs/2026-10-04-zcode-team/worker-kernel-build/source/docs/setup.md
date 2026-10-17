---
title: Setup
nav_order: 3
---

# Setup

Everything needed to develop a homebrew application, a payload, or a library, on Windows or Linux
(x64 only). Follow the section for your operating system, then run `doctor.ps1` to confirm the
machine is ready.

## What the toolchain needs

Two things, whatever the host:

| Item | What it is | Needed for |
|---|---|---|
| **.NET 10 SDK** | The C# compiler and the ahead-of-time compiler. | Building and testing. |
| **SDK checkout** | This repository. Pointed at by `SHARPPROSPERO_ROOT`. | Samples and the shared build script. |

The runtime is not a separate item: the compile step restores the .NET SDK's own ahead-of-time runtime
pack, and `build.ps1` gathers it from there. The linker, the start object, the compat object and the
import stubs are part of the SDK itself, so there is no runtime pack, separate linker, start file, or
stub library to install. See [Build pipeline](build-pipeline.md) for what each step does.

## One thing to know about the compile step

The output is compiled ahead of time into an **ELF x86-64 object** (the console's ABI). The
ahead-of-time compiler emits an object only for the operating system it runs on, so the compile step
runs on Linux:

- **Linux (x64)** produces the object directly — the simplest host, and the whole pipeline runs there.
- **Windows (x64)** runs the link and pack steps on the host (they are plain .NET); for the compile
  step, `build.ps1` uses **WSL** automatically, so you build in place without switching hosts.

## Windows (x64)

1. **Install the .NET 10 SDK.** Download it from <https://dotnet.microsoft.com/download>, or:

   ```
   winget install Microsoft.DotNet.SDK.10
   ```

   Confirm with `dotnet --version` (reports `10.x`). Install PowerShell 7 (`winget install Microsoft.PowerShell`) if you do not have `pwsh`.

2. **Point the environment at the SDK** (persisted for new terminals):

   ```
   setx SHARPPROSPERO_ROOT "C:\path\to\SharpProspero"
   ```

3. **Install WSL for the compile step.** The build runs the compile inside WSL automatically; you do not
   run anything there by hand.

   ```
   wsl --install -d Debian
   # inside Debian, install the .NET 10 SDK (dotnet-install.sh --channel 10.0)
   ```

   With WSL and .NET present, `build.ps1` runs the compile in WSL and the link and pack on Windows, over
   the object WSL produced (both sides share the project's `obj` folder).

## Linux (x64)

1. **Install the .NET 10 SDK** — from the distribution feed or the install script:

   ```
   curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0
   export PATH="$HOME/.dotnet:$PATH"
   ```

   Install PowerShell (`dotnet tool install --global PowerShell`) so `pwsh` is available.

2. **Point the environment at the SDK** (add to `~/.bashrc` or `~/.profile`):

   ```
   export SHARPPROSPERO_ROOT="$HOME/SharpProspero"
   ```

3. Nothing more — Linux x64 produces the console object directly, so the whole `publish → link → pack`
   pipeline runs on this host with no extra setup.

## Confirm the setup

```
pwsh doctor.ps1
```

Expect `[ ok ]` for the .NET SDK and the SDK root, and (on Windows) the WSL compile host. Then build
the included sample end to end:

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-app/SampleApp.csproj
```

A `*.pkg` appears under the sample's `out` folder.

## Build an application module, a library, or a payload

The same pipeline produces all three; the link kind decides which:

- **Application module** (default) → an `eboot.bin` the console launches. See
  [Application Modules](application-modules.md) for the samples and the full API surface.
- **Library** → a relocatable `.prx` another module loads at run time. Set both
  `<OutputType>Library</OutputType>` and `<ProsperoModuleKind>Prx</ProsperoModuleKind>`; the module is
  named `<AssemblyName>.prx`. A project left as `Exe` stops at the compile step with an error asking
  for `OutputType=Library`, before it writes an object.
- **Payload** → a position-independent `.elf` a loader maps into a running process. Pass `-Payload`
  to `build-app.ps1`. See [Payloads](payloads.md) for the runtime bring-up, the payload API surface,
  and the sample gallery.

---
title: Home
nav_order: 1
---

# SharpProspero

A C# SDK and toolchain for building homebrew that runs on the console. Write it in C#; the
ahead-of-time compiler turns it into a self-contained ELF, the linker packs it into either an
installable application module or a position-independent payload, and nothing extra has to ship
alongside — no runtime pack, no separate linker, no start file, no stub library.

Two executable forms come out of the same C#:

- **Application modules** the console installs and launches from the home screen. Full lifecycle,
  full device-service surface. See [Application Modules](application-modules.md).
- **Payloads** a small loader maps into a process that is already running. No install, no
  signature, kernel access through the pipe primitive. See [Payloads](payloads.md).

## Start here

1. [Getting started](getting-started.md) takes an empty project to a running application in a few
   steps.
2. [Setup](setup.md) installs the toolchain for Windows and Linux (both x64).
3. [Application Modules](application-modules.md) covers the API surface for homebrew titles.
4. [Payloads](payloads.md) covers the runtime bring-up, the kernel-access surface, and the payload
   API.
5. [Extras](extras.md) collects the tips per form and the package metadata every title carries.

## The documentation

| Section | What it covers |
|---|---|
| [Getting started](getting-started.md) | From an empty folder to a running module. |
| [Setup](setup.md) | The per-operating-system install and `doctor.ps1`. |
| [Application Modules](application-modules.md) | The API surface for homebrew titles: the application host, graphics, input, audio, media, networking, memory, interface toolkit, data and utilities, system services, modules and libraries, samples, and the application-promotion pattern. |
| [Payloads](payloads.md) | An overview, how to build one, the runtime bring-up, the resolver cascade, the kernel-access surface, the SPRX declarations, the payload API, the samples, the send path, the daemon that promotes a running application, and troubleshooting. |
| [Extras](extras.md) | Tips per form and the `sce_sys/param.json` fields. |
| [Toolchain](toolchain.md) | The architecture, the compile-link-pack pipeline, and the modules-versus-payloads comparison. |
| [References](references.md) | Bindings, the signed and unsigned forms, module offsets, and the command reference. |
| [Help](help.md) | Troubleshooting for the build, the pack, the install, and the run. |

Not sure where a type lives? Every page names the namespace it documents, and the search box (press
`s`) indexes the whole site.

## What it gives you

- **Interop bindings** for the device services a module uses: display output, controller input and
  output, audio output and microphone input, files and directories, image decode and encode, the
  real-time clock, the entropy source, and the user and system services. See [Bindings](bindings.md).
- **A drawing layer**: a framebuffer surface with fills, lines, circles, opaque and alpha-blended
  copies, in-place image effects, PNG, JPEG, BMP, TGA, and GIF decode with PNG, JPEG, BMP, and TGA
  encode, a bitmap font and a scalable TrueType font, over a double-buffered display. See
  [Graphics](graphics.md).
- **A graphics-processor layer**: the command interface, a command buffer, render-target, blend,
  depth-stencil and viewport state, image and sampler descriptors, surface tiling, and a lit 3D
  mesh renderer over it. See [GPU command layer](graphics-gpu.md).
- **An application host**: derive from `ProsperoApp`, override `OnFrame`, call `Run`. See
  [Application host](application.md).
- **An interface toolkit**: build screens out of controls driven by the controller, so you do not
  draw the interface by hand. See [Interface toolkit](ui.md).
- **Audio, media and networking**: an output port paced to the audio clock, a mixer, synthesis and
  filters, a microphone port, compressed-audio and video decoding, and TCP, UDP and HTTP. See
  [Audio](audio.md), [Media](media.md), and [Networking](networking.md).
- **System services**: save data, on-screen dialogs, trophies, screen capture, package install, and
  firmware compatibility. See [System services](system-services.md).
- **A payload API**: `SharpProspero.Payload` provides the network, filesystem, process,
  notification, and kernel-access surface a payload uses instead of the application host. The CRT
  emits the syscall gateway, the resolver cascade, and per-field kernel accessors so a payload can
  walk the process list and modify credentials without a dynamic linker. See
  [Payloads](payloads.md).
- **A toolchain that stands alone**: the linker supplies its own start object and its own module
  stubs, so a build needs no separate linker, start file, or stub library. Its commands inspect,
  strip, retarget, convert and package a module. See [Toolchain](toolchain.md) and the
  [command reference](commands.md).

## Requirements

- .NET 10 SDK, on Windows or Linux (x64 only).
- The compile step runs on Linux; on Windows the build uses WSL for it automatically. See the
  [build pipeline](build-pipeline.md).

`pwsh doctor.ps1` in the SDK folder checks a machine and prints what to set for anything missing.

## License

GPL-3.0-or-later. Copyright (C) SvenGDK 2026.

---
title: Toolchain
nav_order: 7
has_children: true
---

# Toolchain

The toolchain turns C# into a module the console runs, without a separate linker, start file, or stub
library. These pages cover how the SDK layers fit together, the compile-link-output pipeline, and the
two executable forms the toolchain builds.

| Page | What it covers |
|---|---|
| [Architecture](architecture.md) | How the layers of the SDK fit together, from the interop bindings up to the application host. |
| [Build pipeline](build-pipeline.md) | The three steps from C# to a module: compile ahead of time, link, and write the output. |
| [Modules and payloads](modules-and-payloads.md) | The two execution forms the toolchain builds, why they differ, and which one to build. |

Reference material — bindings, signed and unsigned forms, module offsets, and the command reference —
lives under [References](references.md). Deep coverage of each executable form is under
[Application Modules](application-modules.md) and [Payloads](payloads.md).

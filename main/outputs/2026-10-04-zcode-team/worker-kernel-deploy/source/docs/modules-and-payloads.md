---
title: Modules and payloads
parent: Toolchain
nav_order: 3
---

# Modules and payloads

The toolchain builds two execution forms from the same C#. They reach the console in different ways
and are run by different loaders, and that decides which one fits a given program. This page covers
the comparison. For full coverage of each, see [Application Modules](application-modules.md) and
[Payloads](payloads.md).

## The two forms

**An application module** is what the console installs and launches. The build links an
`eboot.bin`, the packager wraps it in a signed container and assembles an installable package, and
the console installs it and shows it on the home screen. It runs as its own process with a full
lifecycle: install, launch, run, exit.

**A payload** is a position-independent executable a small loader maps into a process that is
already running, over a network connection, and jumps to. There is no package and no signature. It
runs inside the host process and ends when its entry returns.

| | Application module | Payload |
|---|---|---|
| Output | `eboot.bin` in a package | a single `.elf` |
| Signed and packaged | yes | no |
| Installed | yes, on the home screen | no, sent each run |
| Runs as | its own process | inside a running host process |
| Loaded by | the console's program loader and dynamic linker | a small loader over a network connection |
| Lifecycle | install, launch, run, exit | maps, runs once, returns |
| Delivery | install the package | send over the network each time |
| Sandbox | per-title sandbox, re-applied each launch | inherits the host process's view |
| Device-service surface | full: display, controller, audio, media, networking, save data, dialogs, trophies, capture, packages, firmware | what the resolver finds, plus kernel access through the pipe primitive |

## Why they differ

The decisive difference is what loads each one.

An application module is loaded by the console's own program loader and dynamic linker. That loader
binds the module's imports to the device libraries by name, gives the module its thread-local
storage, registers its exception frames, and runs its global constructors before the entry. The
module declares what it needs and the system provides it.

A payload's loader does none of that. It maps the segments, applies only base-relative fix-ups, and
calls the entry: no import binding, no thread-local set-up, no constructors. So a payload must be
self-contained. It resolves every function it calls at run time through a resolver the loader hands
it, and its start code performs the bring-up the dynamic linker would otherwise do — running the
global constructors, marking the C runtime threaded, allocating and installing the thread-local
block, walking its own GOT and populating each `GLOB_DAT` slot from the loader's resolver.

Signing follows the same split. An application module is signed and packaged so the console can
install and launch it on its own. A payload rides an already-running host process, so it needs no
signature or package, but it also cannot exist on its own: something must already be running to
load it.

Both forms carry the same ahead-of-time runtime, so a payload's `.elf` is as large as a module's
`eboot.bin`; it loads over the network rather than from storage.

## Which to build

Choose by how the program is meant to run.

- **An application module** for a program the user installs and launches from the home screen: a
  game, a tool, a file manager, a media player. It has a normal lifecycle and can reach the full
  device-service surface.
- **A payload** for a program that runs once inside a host process, launched from outside with no
  install: a background service such as a file-transfer, log, or web server; a one-shot action; or
  a bring-up experiment.

An application module can pair with a payload for one specific case: reaching past the per-title
credential set and sandbox. Because both are re-applied at every launch, the module cannot promote
itself; instead the user runs a companion payload (the unjail daemon) that promotes the
application on request at run time. See [Promoting an application](app-promotion.md) for the
application-module side and [Promoting a running application](payload-promotion.md) for the
payload side.

## Building each

An application module (compile, link with `--kind eboot`, pack) is the default of `build-app.ps1`:

```
pwsh build/build-app.ps1 -ProjectPath MyApp.csproj
```

See [Build pipeline](build-pipeline.md) for the three steps and their options.

A payload (compile, link `--kind payload`, send) adds the `-Payload` switch:

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-unjail/SampleApp.csproj -Payload -Output Folder
```

The output is a single `.elf` under the project's `out/` folder. Send it to a listening loader —
see [Sending a payload](payload-deploy.md).

## Separation

The application-module and payload code paths through the toolchain are strictly separate:

- **Emitters.** `CrtEmitter` writes the application-module start object; `PayloadCrtEmitter` writes
  the payload's. The two are unrelated code paths.
- **Compat forwarder.** The compatibility object that bridges the C runtime uses a plain
  `mprotect` length for an application-module build and adds an extra page for a payload build so
  the collector's straddled bookkeeping page is covered.
- **Linker.** `--kind eboot` and `--kind payload` produce different segment layouts and different
  dynamic sections. The payload has no section table.
- **Runtime helpers.** The `SharpProspero.Application` namespace is for application modules;
  `SharpProspero.Payload` is for payloads. Payloads never import the application namespace, and
  application modules never link against the payload runtime.

The separation is a hard rule: a change on one side must never affect a build on the other. See
[Building a payload](payload-build.md) for the payload's link details and
[Build pipeline](build-pipeline.md) for the application-module pipeline.

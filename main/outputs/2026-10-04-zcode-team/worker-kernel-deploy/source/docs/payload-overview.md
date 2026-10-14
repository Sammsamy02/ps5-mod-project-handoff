---
title: Overview
parent: Payloads
nav_order: 1
---

# Overview

A payload is a single position-independent `.elf` a loader maps into a process that is already
running, over a network connection, and jumps to. It runs inside the host process and ends when its
entry returns. This page collects the shape of a payload, when to build one, and the vocabulary the
rest of this section uses.

## What a payload is

The linker produces a `ET_DYN` ELF with three load segments, a `PT_DYNAMIC` segment, and one entry
point named `_start`. Under `_start` sits the payload's C runtime (the "CRT"): a self-contained
bring-up sequence that a dynamic linker would ordinarily supply for a program. Under the CRT sits
`main` from `libbootstrapper.o` — the ahead-of-time runtime's bootstrap that calls `RhInitialize`,
`RhRegisterOSModule`, and `InitializeModules`. Under that sits the payload's own managed entry:

```csharp
[UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
public static int Main(void* args)
{
    // your code
    return 0;
}
```

The loader on the console side is small: it accepts a TCP connection, reads the ELF, maps its LOAD
segments, applies base-relative fix-ups, and jumps to `_start`. It does not resolve imports, does not
allocate thread-local storage, and does not run constructors. Everything the loader would have done
for an application module, the CRT does at run time — see [Runtime bring-up](payload-runtime.md).

## When to build a payload

Build a payload when the program has to run inside a process that is already open:

- A **background service** the user starts from a launcher: an FTP server, a debug bridge, a
  status page, a companion daemon that other homebrew calls.
- A **one-shot action** with no interface of its own: send a notification, launch the browser at a
  URL, dump the hardware info, list the mounted file systems.
- A **bring-up experiment** that reads or writes kernel structures, walks the process list, or
  applies a small runtime patch to the host process.

An application module is the right form for anything the user installs and launches from the home
screen — see [Application Modules](application-modules.md).

## What the CRT provides

A payload's CRT is the layer that lets managed C# run inside a hijacked host process. It bakes in
everything a program's start-up would otherwise pull from the C library and the dynamic linker:

- The **syscall gateway** `__sp_crt_syscall` shuffles the C calling convention into the syscall
  ABI and dispatches through an instruction gadget derived once at start-up.
- The **resolver cascade** binds every function name the payload calls to a runtime address by
  probing three module handles in sequence — see [Resolver cascade](payload-resolver.md).
- The **kernel-access surface** carries per-firmware offsets and a pipe primitive so a payload can
  read and write kernel structures with per-field accessors — see [Kernel access](payload-kernel.md).
- The **thread-control block** is allocated at start-up, populated from the host's stack canary, and
  installed with `sysarch(AMD64_SET_FSBASE)`. Every managed thread-local access hits this block at
  a fixed offset from the thread pointer.
- The **thread exit** epilogue calls `SYS_thr_exit` (syscall 431) so returning from `main`
  terminates only the hijacked thread, not the host process.

## The wire

The default send path uses the SDK's `payload --send` command, which speaks the small loader wire
format: a TCP connection to port 9021, the raw ELF bytes, and a close. See
[Sending a payload](payload-deploy.md) for the command and its options.

## The API surface

Everything a payload calls from managed code lives under the `SharpProspero.Payload` namespace:
network, filesystem, process, notification, kernel access, dynamic library loading, hardware info,
sysctl, user service, browser, HTTP/2, package install, and random-bytes reading. See
[Payload API](payload-api.md) for the complete list.

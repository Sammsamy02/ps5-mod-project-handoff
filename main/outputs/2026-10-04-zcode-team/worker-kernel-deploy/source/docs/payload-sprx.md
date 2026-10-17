---
title: SPRX declarations
parent: Payloads
nav_order: 6
---

# SPRX declarations

A payload reaches system libraries through their SPRX modules. The default set covers most needs;
anything beyond it is declared per project. This page covers the two project properties, the
default set, the interaction with the resolver, and the common overrides.

## The two properties

```xml
<PropertyGroup>
  <ProsperoKernelSprx>libkernel_sys.sprx</ProsperoKernelSprx>
</PropertyGroup>

<ItemGroup>
  <ProsperoSprx Include="libSceRandom.sprx" />
  <ProsperoSprx Include="libSceUserService.sprx" />
</ItemGroup>
```

| Property | What it does |
|---|---|
| `<ProsperoSprx Include="..." />` | Adds one SPRX to the payload's `DT_NEEDED` list. The RTLD init loads the module and probes it during GOT walking, so a `LibraryImport` from managed code binds against it at bring-up. |
| `<ProsperoKernelSprx>` | Replaces the default kernel SPRX (`libkernel_web.sprx`) with the named one. Only one kernel SPRX at a time; the resolver's handle `0x2001` always refers to it. |

## The default set

Every payload gets three SPRXes in `DT_NEEDED`, in this order:

| Slot | Module | What it publishes |
|---|---|---|
| 1 | `libkernel_web.sprx` (or `libkernel_sys.sprx` when overridden) | Kernel services: `sceKernel*`, kernel-side pthread, `sysarch`, `mprotect`, notification requests, module loading. |
| 2 | `libSceLibcInternal.sprx` | C library: `puts`, `snprintf`, `memcpy`, `malloc`, POSIX pthread. |
| 3 | `libSceNet.sprx` | Socket API: `socket`, `bind`, `listen`, `accept`, `send`, `recv`. |

A `<ProsperoSprx>` entry that names one of the defaults is not duplicated — the item appears once
in `DT_NEEDED`, at the position `ProsperoSprx` places it. Extras always precede defaults, so an
override that changes an SPRX's order is possible without editing the defaults.

## When to override the kernel SPRX

The default `libkernel_web.sprx` is the browser-context kernel build. Some payload tasks need the
system-context build `libkernel_sys.sprx` instead:

- **Hardware information reads** (`getModelName`, `getSerialNumber`, temperature sensors) are only
  in the sys build.
- **`getmntinfo`** — used by a mount-info payload — is only in the sys build.
- **Some sysctl paths** are only in the sys build.

Set `<ProsperoKernelSprx>libkernel_sys.sprx</ProsperoKernelSprx>` in the project. The kernel-side
pthread and every `sceKernel*` still resolves through the same handle; the change is the SPRX file
the handle refers to.

## When to add an extra SPRX

Add one when the payload's managed code contains a `LibraryImport` for a symbol none of the
defaults publish:

| Extra SPRX | Names it publishes |
|---|---|
| `libSceRandom.sprx` | `sceRandomGetRandomNumber`. |
| `libScePad.sprx` | `scePadOpen`, `scePadReadState`, `scePadReadCurrent`. |
| `libSceUserService.sprx` | `sceUserServiceInitialize`, `sceUserServiceGetInitialUser`. |
| `libSceSystemService.sprx` | `sceSystemServiceLaunchWebBrowser`, `sceSystemServiceLoadExec`. |
| `libSceHttp2.sprx` | `sceHttp2*` — the HTTP/2 client surface. |
| `libSceSsl.sprx` | `sceSsl*` — TLS on top of the HTTP/2 stack. |
| `libSceNotification.sprx` | `sceNotificationSend`, `sceNotificationSendById` — user-visible toasts. |

Every one of these is a common pairing with a `SharpProspero.Payload.*` wrapper class. Check the
wrapper's source or its `[LibraryImport]` attribute to see the module it targets, and add the
matching `<ProsperoSprx>` entry.

## What the linker emits

The linker adds one `DT_NEEDED` entry per extra plus one per default (deduplicated). The RTLD init
in the CRT walks the entries in order, calls `sceKernelLoadStartModule` on each, and stores the
returned handle in a BSS slot for the GOT walk. Handles `0x1` (the payload itself), `0x2`
(`libSceLibcInternal`), and `0x2001` (the kernel SPRX) are populated during bring-up; other extras
receive slot indices starting at `0x2002`.

A `LibraryImport` from managed code binds against the extras through the same
`GLOB_DAT` walk. The order does not matter for correctness, only for the resolver cascade shadowing
described in [Resolver cascade](payload-resolver.md).

## Rebuilding after a declaration change

Adding a `<ProsperoSprx>` or changing `<ProsperoKernelSprx>` changes `DT_NEEDED`, which changes the
RTLD init sequence, which changes the CRT breadcrumbs. Rebuild:

```
pwsh build/build-app.ps1 -ProjectPath samples/<sample>/SampleApp.csproj -Payload -Output Folder
```

Verify with the `elf` command:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- elf --file out/SampleApp.elf --sizes
```

The dynamic section lists every `DT_NEEDED` entry the linker emitted.

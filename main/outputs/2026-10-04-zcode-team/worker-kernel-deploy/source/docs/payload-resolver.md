---
title: Resolver cascade
parent: Payloads
nav_order: 4
---

# Resolver cascade

Every function the payload calls at run time is bound to an address through the resolver cascade.
The loader hands the CRT one resolver entry — a syscall gadget — and three module handles. The CRT
probes each handle in sequence for a name, and the first hit wins. This page covers the handles,
the naming rules, and the failure modes.

## The three handles

The CRT queries the resolver with the caller's module handle and the encoded name (`SceNid`). The
handles are:

| Handle | Module | What it contains |
|---|---|---|
| `0x1` | The payload's own image | Symbols defined in the payload itself: the CRT helpers, the compat forwarders, and any `<UnmanagedCallersOnly>` exports. |
| `0x2` | `libSceLibcInternal` | The C library and the POSIX pthread surface — `puts`, `snprintf`, `strerror`, `pthread_*`, `memcpy`, `malloc`, and so on. |
| `0x2001` | `libkernel_web` (or `libkernel_sys` when overridden) | The kernel-services surface — `sceKernel*`, the kernel-side pthread, `sysarch`, `mprotect`, `sceKernelSendNotificationRequest`, and so on. |

The cascade order is fixed: `0x1` → `0x2` → `0x2001`. A name found earlier is not probed later,
so an internal name shadows a system name of the same shape.

## The naming rules

The console's export tables key on the `SceNid`: an eleven-character Base64 string derived from the
first eight bytes of SHA-1(name + salt). Two rules:

1. The resolver takes the **plain C name**. The CRT computes the `SceNid` from the name every
   time — the name never appears in the built ELF apart from the CRT's own literal strings for the
   symbols it resolves at bring-up (`puts`, `pthread_self`, `sysarch`, and so on).
2. The published dynsym on the device uses a **byte-swapped `SceNid`**. The encoder pins that swap;
   asking the resolver for the wrong-endianness form finds nothing. Managed code never sees this;
   the encoder in `SharpProspero.Payload` handles both directions.

## What the resolver actually does

The loader's resolver is a syscall trampoline: it calls `SYS_dynlib_dlsym` (591) with the handle
and the encoded name, and writes the resolved address into a pointer the caller supplies. When a
name is not found the resolver leaves the pointer alone — a subsequent read sees zero.

The CRT hands the resolver a stack slot to write into and reads the slot afterwards. A zero read
means "not found on this handle"; the CRT then probes the next handle. When every handle returns
zero the resolver produces a null address, and the CRT emits `sp:resolver:notfound:<name>` in the
diagnostic build.

## The CRT's own resolves

The CRT resolves a small set of names at bring-up so the rest of the payload has a working syscall
gateway, a working log, and a working thread. Every symbol below is resolved once, cached in a BSS
slot, and read from there afterwards.

| Name | Handle | What it is for |
|---|---|---|
| `snprintf`, `vsnprintf`, `strerror`, `__error` | `0x2` | Formatted output and errno access the CRT uses to build breadcrumbs. The klog helper itself issues `SYS_kexec` (syscall 7) directly, so no `puts` binding is needed. |
| `pthread_self` | `0x2` | Priming the pthread lazy-init before the bootstrap runs. |
| `sysarch` | `0x2001` | Installing the thread-control block via `AMD64_SET_FSBASE`. |
| `mprotect` | `0x2001` | Making writable pages executable when the compat forwarder needs to. |
| `sceKernelLoadStartModule` | `0x2001` | Loading extra SPRX modules during RTLD init. |

Every other name the payload calls — including `sceKernelSendNotificationRequest`, every SPRX
export, and every libc call the C# code makes — is bound during the RTLD GOT walk (step 6 of
[Runtime bring-up](payload-runtime.md)), not by the CRT bring-up.

## Managed code

Managed code that needs a resolved address does not call the resolver directly. Two paths are
available:

- **`LibraryImport`** with an `EntryPoint`. The source generator emits a marshalling stub and a
  `GLOB_DAT` slot in the payload's own imports. The RTLD init walks the imports at bring-up, calls
  the resolver for each entry, and writes the result into the slot. The `LibraryImport` call reads
  from the slot.
- **`SharpProspero.Payload.PayloadDlfcn`** for a symbol not known at link time. `Dlopen` opens a
  SPRX by name and returns a handle; `Dlsym` resolves a symbol on that handle and returns the
  address as a `void*`; `Dlclose` releases the handle.

Every `SharpProspero.Payload` wrapper class in the SDK uses the first path.

## SPRX modules other than the defaults

A payload that calls a function from a system library beyond the three defaults declares the extra
library in the project file:

```xml
<ItemGroup>
  <ProsperoSprx Include="libSceRandom.sprx" />
</ItemGroup>
```

The linker adds a matching `DT_NEEDED` entry, and the RTLD init loads the module through
`sceKernelLoadStartModule` and probes it during GOT walking. See [SPRX declarations](payload-sprx.md)
for the full set of options.

## The kernel-library override

The default kernel library is `libkernel_web.sprx`. Some payloads need `libkernel_sys.sprx` instead —
for `getmntinfo`, hardware-info reads, or sysctls that only the sys build publishes. Override the
default in the project file:

```xml
<PropertyGroup>
  <ProsperoKernelSprx>libkernel_sys.sprx</ProsperoKernelSprx>
</PropertyGroup>
```

The linker replaces the first `DT_NEEDED` entry; handle `0x2001` still resolves through this module.

## Failure signatures

| Symptom | What happened |
|---|---|
| `sp:resolver:notfound:<name>` appears in the log | The name is missing from every probed handle. Add the SPRX that publishes it with `<ProsperoSprx>`. |
| A call goes through and returns immediately | The name resolved to a stub that always returns zero (a name valid on `libkernel_web` but not on `libkernel_sys`, or vice versa). Check the kernel library override and the handle each name is expected on. |
| Every call fails, no breadcrumbs | The resolver entry itself is not what the CRT expects. The `__sp_crt_syscall_init` step probably picked up the wrong `getpid` layout. Rebuild with `-DiagnosticBreadcrumbs` and check for `sp:crt:syscall:done` in the log. |

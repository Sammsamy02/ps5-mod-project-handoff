---
title: Payload troubleshooting
parent: Payloads
nav_order: 11
---

# Payload troubleshooting

The common failures for a payload, the log lines that identify them, and the fixes. Every payload
built with `-DiagnosticBreadcrumbs` emits an `sp:*` line at each step of bring-up. When a
breadcrumb goes missing, the last one printed names the step that failed.

## Reading the breadcrumbs

A successful bring-up prints these in order:

```
sp:init:syscall
sp:init:kernel
sp:kernel:init:ok
sp:tcb:set
sp:init:klog
sp:klog:ok
sp:init:patch
sp:patch:ok
sp:init:rtld
sp:rtld:sprx:init
sp:rtld:so:init
sp:rtld:payload:init:start
sp:rtld:payload:init:done
sp:rtld:dlfcn:init
sp:rtld:ok
sp:init:done
sp:crt:enter
sp:isthreaded:ok
sp:main:enter
```

The payload's own output follows `sp:main:enter`. When the payload's managed entry returns, the
CRT epilogue prints `sp:main:exit` and then `sp:exit` before terminating the thread. If
`-DiagnosticBreadcrumbs` was not passed, none of these lines is present — the bring-up either
works silently, or the payload never runs.

## Bring-up failures

### No breadcrumbs at all

**Cause.** The loader mapped the payload, but `_start` never ran — the ELF header's entry point is
wrong, or the map itself failed.

**Fix.** Verify the ELF header with the toolchain:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- elf --file out/SampleApp.elf --sizes
```

The entry point has to be inside the first LOAD segment. If the loader crashed instead of running
the payload, the console will show a corresponding fault; check the console-side log.

### Stops before `sp:init:syscall`

**Cause.** The syscall gateway could not derive the instruction gadget address from the resolver's
`getpid` pointer. The CRT emits `sp:crt:syscall-init:fail` when that derivation fails.

**Fix.** The loader did not hand the resolver the layout the CRT expects (or handed a null
pointer). Confirm the loader is the one the SDK targets, and confirm `payload_args` on the host is
populated.

### Stops at `sp:init:kernel` (no `sp:kernel:init:ok`)

**Cause.** The per-firmware data table has no row for the running firmware major version, or the
BCD-to-linear conversion is wrong. When the kernel init falls through to a degenerate row, the CRT
emits `sp:kernel:init:degen` and the payload continues without kernel access; when the init fails
outright, it emits `sp:init:kernel:fail=0x<code>`.

**Fix.** Confirm the firmware version. If the running firmware is not in the table, add its row —
see [Working with module offsets](offsets.md).

### Stops at `sp:tcb:set` (no `sp:init:klog`)

**Cause.** `sysarch(AMD64_SET_FSBASE)` returned an error. Usually the resolver bound `sysarch` to
the wrong entry.

**Fix.** Verify the kernel SPRX handle the resolver is using. `sysarch` lives on `libkernel_web`
and `libkernel_sys`; if the override points at the wrong one, or the handle is not populated,
`sysarch` will fail.

### Stops at `sp:init:klog` (no `sp:klog:ok`)

**Cause.** A resolve for one of the klog helpers (`snprintf`, `vsnprintf`, `strerror`, `__error`)
failed on handle `0x2` (libSceLibcInternal). The resolver emits `sp:resolve:0` for each name that
did not resolve.

**Fix.** Check the resolver output for the missing name. The klog init needs libSceLibcInternal in
`DT_NEEDED`; that library is a default so absent hits only when the linker skipped an entry.

### Stops at `sp:init:rtld` with a `sp:rtld:sprx:init` that never returns

**Cause.** `sceKernelLoadStartModule` returned an error for the SPRX currently being loaded.
Either the SPRX is not present on the firmware, or the loader is not authorised to load it.

**Fix.** Confirm the SPRX is a real module on the running firmware. A payload that lists a SPRX
the firmware does not publish stalls at this step every time.

### Stops between `sp:init:rtld` and `sp:rtld:ok`

**Cause.** The GOT walk failed. Sub-breadcrumbs `sp:dl:walk:*` name what specifically went wrong:

| Sub-breadcrumb | Meaning |
|---|---|
| `sp:dl:walk:allproc:fail` | Could not read the process list. Kernel init did not populate `allproc`. |
| `sp:dl:walk:proc:0` | Own process pointer resolved to zero. |
| `sp:dl:walk:pid:miss` | Own pid did not match any process in the list. |
| `sp:dl:walk:handle:miss` | A `DT_NEEDED` name matches no loaded module handle. |
| `sp:dl:walk:obj:fail` | Could not read the module object. |
| `sp:dl:walk:meta:fail` | Could not read a module metadata slot. |
| `sp:dl:walk:mmap:fail` / `sp:dl:walk:copy:fail` | Could not map or copy a module's dynsym. |
| `sp:dl:walk:sym:miss` | A specific symbol did not resolve. |
| `sp:dl:walk:miss` / `sp:dl:fb:miss` | A GLOB_DAT slot did not bind. |

**Fix.** Trace back from the sub-breadcrumb. A `sp:resolve:0` line names the symbol that could not
bind; add the SPRX that publishes it with `<ProsperoSprx>`, or fix the name in a `LibraryImport`.

### Stops before `sp:main:enter`

**Cause.** The ahead-of-time runtime bootstrap failed. Almost always a malformed managed entry:
`__managed__Main` is not exported, or its signature is not `int(void*)`.

**Fix.** Confirm the payload's entry point matches:

```csharp
[UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
public static int Main(void* args) { ... }
```

## Managed failures

### `sp:main:enter` appears, then the payload dies

**Cause.** The payload's own code faulted. The CRT does not intercept managed faults, so the
console side sees a raw SIGSEGV or SIGBUS.

**Fix.** Instrument the payload's `__managed__Main` with `__prospero_klog` calls before every
significant step. The last logged line names the region that faulted. Common causes:

- A null pointer deref (a `[LibraryImport]` returned null and the caller did not check).
- A raw pointer read past the end of a buffer.
- A resolver lookup that returned null when the payload assumed non-null.

### Every kernel read returns zero

**Cause.** The pipe primitive did not initialise. See [Kernel access](payload-kernel.md) for the
BSS slots and the initialisation sequence.

**Fix.** Confirm `PayloadEntryPoint.Args` is non-null. If it is null, the loader did not supply
`payload_args`, and no kernel access will work.

### A single kernel read returns zero, others work

**Cause.** The address is on a page the kernel does not have mapped, or the per-firmware data
table's row is wrong for that address.

**Fix.** Read the address's presence with a `TryRead*` variant first, and confirm the firmware
version. If the table row is wrong, an offset for this firmware is missing — see
[Working with module offsets](offsets.md).

### The console page-faults after a write

**Cause.** The write went to the wrong address (a stale process pointer), or the value written is
outside what the kernel expects for that field.

**Fix.** Read the current value first, log it, and compute the desired value from the read. Every
credential-write path in `PayloadKernel` does this.

## Send-side failures

The send program surfaces "could not connect" and "broken pipe" — see [Sending a payload](payload-deploy.md)
for those. If the send succeeds but the payload does not run, the failure is on the console side
and the breadcrumb sequence above is the way to localise it.

## When the daemon fails to accept new connections

A daemon that binds and listens successfully, then stops accepting after a while, is usually one
of:

- The socket ran out of accepted-but-unclosed clients. Make sure every accept path calls
  `PayloadNetwork.Close(client)` after replying.
- The daemon's own listener descriptor was closed by another path. Confirm the accept loop is the
  only owner of the listener.
- The host process died and took the daemon's thread with it. Send the payload again.

## Where to look

- `sp:*` breadcrumbs — the bring-up sequence.
- The payload's own `__prospero_klog` output — the managed run.
- The console's user-visible notifications (`PayloadNotification.SendKernelNotification`) — a
  daemon can raise a toast when it starts and when it accepts a request, so the user sees the
  daemon is alive without watching the device log.

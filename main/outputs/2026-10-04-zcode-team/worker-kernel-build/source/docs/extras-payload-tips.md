---
title: Tips for payloads
parent: Extras
nav_order: 2
---

# Tips for payloads

Everyday recipes for building payloads. Each is a self-contained pattern the SDK's payload API
supports, so a payload can pull it in without extra scaffolding.

## Print to the kernel log

Every payload should print at least one line so the console log records the run:

```csharp
using System.Runtime.InteropServices;

internal static unsafe partial class Program
{
    [LibraryImport("libScePosix", EntryPoint = "__prospero_klog")]
    private static partial void Klog(byte* message);

    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        fixed (byte* p = "my-payload: hello\n"u8) Klog(p);
        return 0;
    }
}
```

`__prospero_klog` is defined by the CRT and resolved at link time — no `DT_NEEDED` needed, no
runtime resolve. Prefix every line with the payload's name so its output is greppable in the log.

## Build and send a payload

A payload is a single `.elf` a loader maps into a running process. Build one, then send it:

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-unjail/SampleApp.csproj -Payload -Output Folder
dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send \
    --host ps5-host-not-configured.invalid \
    --file samples/prospero-payload-unjail/out/SampleApp.elf
```

Add `-DiagnosticBreadcrumbs` to include the CRT's bring-up log checkpoints in the build. See
[Sending a payload](payload-deploy.md).

## Run as a daemon

Never return from `__managed__Main` when the payload has to persist. Return terminates the
hijacked thread, so a daemon that returns after a request stops serving after one client.

```csharp
[UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
public static int Main(void* args)
{
    int listener = PayloadNetwork.Listen(9069);
    while (true)
    {
        int client = PayloadNetwork.Accept(listener);
        if (client < 0) continue;
        HandleClient(client);
        PayloadNetwork.Close(client);
    }
}
```

A daemon that binds to a fixed port allows only one instance in the host process. Handle bind
failure by exiting; the user can send a fresh payload after restarting the host.

## Read the loader-supplied arguments

The loader hands the payload a `PayloadArgs*` at start-up. Check it first:

```csharp
PayloadArgs* pargs = PayloadEntryPoint.Args;
if (pargs == null)
{
    fixed (byte* p = "my-payload: no args\n"u8) Klog(p);
    return -1;
}
```

A null pointer means the pipe primitive is unavailable — no kernel access will work, and the
payload should exit rather than continue with a partial surface.

## Resolve an optional symbol

For a symbol that may or may not be on the running firmware, drive the resolver at run time
instead of a `LibraryImport`:

```csharp
nint addr = PayloadDlfcn.Dlopen("libSceOptional.sprx"u8, PayloadDlfcn.RTLD_LAZY);
if (addr == 0) { /* fall back */ }
else
{
    void* fn = PayloadDlfcn.Dlsym((void*)addr, "sceOptionalCall"u8);
    if (fn == null) { /* fall back */ }
    // ... use fn ...
}
```

A `LibraryImport` fails the whole payload's link if the symbol is missing; `Dlsym` returns null and
lets the payload continue.

## Send a user-visible notification

A daemon that finished bring-up successfully should tell the user, so the launcher shows it is
alive without opening the device log:

```csharp
PayloadNotification.SendKernelNotification("my-daemon: ready"u8);
```

For a longer message with a payload-specific icon, use the user-visible variants of
`PayloadNotification`.

## Iterate quickly

The fastest cycle is:

1. Edit the payload's `Program.cs`.
2. Rebuild: `build-app.ps1 -Payload -Output Folder`.
3. Send the fresh `.elf` to the same host and port.
4. Read the device log for the `sp:*` breadcrumbs and the payload's own output.

For a daemon, restart the host process between sends — see [Sending a payload](payload-deploy.md).

## Read the process list

For a payload that has to target another process by name or title id:

```csharp
var io = new PayloadKernelIo(PayloadEntryPoint.Args);
ulong proc = PayloadKernel.FindProcessByTitleId(io, titleIdBytes, titleIdBytes.Length);
if (proc == 0) return -1;

int pid = PayloadKernel.ReadPidFromProc(io, proc);
```

Walking the whole list on every request is fine — the list is short.

## Keep the pipe primitive alive

The pipe primitive is set up once by the CRT at start-up. It stays alive for the whole payload's
run, so hold a single `PayloadKernelIo` for the lifetime and reuse it:

```csharp
var io = new PayloadKernelIo(PayloadEntryPoint.Args);   // once
// every request:
ulong value = io.ReadU64(someKaddr);
```

Constructing a fresh `PayloadKernelIo` per request is not incorrect, but it re-reads `payload_args`
every time.

## Handle a per-firmware address

Every kernel address in the payload comes from the per-firmware data table baked into the CRT. A
payload that has to compute a fresh address at run time reads the firmware version first:

```csharp
byte fwMajor = /* read the running firmware major */;
if (fwMajor != 0x10)
{
    fixed (byte* p = "my-payload: unsupported firmware\n"u8) Klog(p);
    return -1;
}
```

Adding a firmware means adding a row to the CRT's table — see
[Working with module offsets](offsets.md).

## Add a system library to the project

Every `LibraryImport` for a symbol not in the default SPRX set requires a `<ProsperoSprx>` entry:

```xml
<ItemGroup>
  <ProsperoSprx Include="libSceRandom.sprx" />
</ItemGroup>
```

The linker emits a matching `DT_NEEDED` entry; the CRT's RTLD init loads the module and binds every
`GLOB_DAT` entry for it. See [SPRX declarations](payload-sprx.md) for the common overrides.

## Reserve the two 1280-byte buffers on the wire

Custom wire formats between an application module and a companion payload should always be
2576 bytes total, with the header at `+0x00…+0x0F` and the two 1280-byte reserved buffers at
`+0x10` and `+0x510`. That lines up with the unjail daemon's reader, so a future extension can
share the same TCP wire without reshaping the buffers. See
[Promoting a running application](payload-promotion.md) for the reference layout.

## General tips

- Prefix every log line with the payload's short name.
- Never call `Environment.Exit` or `Thread.Abort` from a payload — the CRT's epilogue is what
  cleanly terminates the hijacked thread.
- Prefer the SDK's `PayloadNetwork`, `PayloadFileSystem`, `PayloadNotification`, `PayloadRandom`,
  `PayloadHardwareInfo`, `PayloadSysctl` wrappers over raw `LibraryImport` calls. Every wrapper
  covers one shape of the underlying surface with a small managed API.
- Keep the payload's own state on the stack when possible. A `stackalloc byte[N]` is faster and
  more predictable than the heap.
- For a payload that reads `/data` or `/app0`, remember the paths are those of the host process —
  they only resolve if the caller's sandbox allows it.
- Verify the built ELF's `DT_NEEDED` list with the `elf` command before sending, to catch a missing
  `<ProsperoSprx>` at build time instead of a `sp:resolver:notfound` at run time.

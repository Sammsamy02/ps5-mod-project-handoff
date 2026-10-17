---
title: Promoting a running application
parent: Payloads
nav_order: 10
---

# Promoting a running application

The unjail payload is a persistent daemon that binds a TCP socket to `127.0.0.1:9069` and applies
an eleven-write credential, capability, and filesystem escalation on request. An application
module sends a request that names its own process identifier; the daemon promotes that process
and replies with the outcome. This page covers why the daemon is the right form, what the daemon
does step by step, and the complete C# code for the daemon.

## Why a payload does this

An application module runs with the per-title credential set the console handed it and inside the
per-title sandbox — both re-applied at every launch, so the module cannot promote itself. A
payload runs inside a host process the loader chose — a host with the credential set the loader
configured, and with the pipe primitive that gives the payload read/write access to kernel
memory. Only that combination can write another process's ucred, capability bitmasks, and
`fd_rdir`/`fd_jdir` pointers.

The payload is a persistent daemon because promotion has to be available on demand: the user sends
the daemon once, and every application module that needs the extended access asks it at run time.
See [Promoting an application](app-promotion.md) for the application-module side that sends the
request.

## The daemon's steps

At start-up, the daemon:

1. Reads `payload_args` — the loader-supplied argument block. A null pointer means the pipe
   primitive is unavailable; the daemon logs and exits.
2. Reads its own pid with `getpid()`. A pid of 0 or below means the loader did not set it up; the
   daemon logs and exits.
3. Raises its own privileges through the same eleven-write set it will later apply to callers.
   Without this the daemon's own pipe writes would fail authorization checks on the second call.
4. Sets its own debug authorization id.
5. Caches the root vnode's kernel address for the lifetime of the daemon. The root vnode is
   constant for the console session, so reading it once is enough.
6. Sends a user-visible notification that the daemon is ready.
7. Binds a TCP socket to `127.0.0.1:9069`, listens, and enters an accept loop.

For every accepted connection, the daemon:

1. Reads exactly 2576 bytes into the request buffer.
2. Validates the magic (`0xDEADBEEF`), the command (`5` = promote), and the pid (`>0`).
3. Applies the eleven-write promotion on the process the pid names, using
   `PayloadKernel.JailbreakByPid`.
4. Writes the outcome into the reply's `+0x0C` slot (`0` on success, `-1` on failure).
5. Sends the reply back and closes the connection.

## The eleven-write promotion

`PayloadKernel.JailbreakByPid(pid, rootvnode)` is a single standalone method that writes eleven
fields through the CRT-emitted pid-based accessors. Each accessor internally walks the process
list to locate the target and applies the write through the pipe primitive:

| Field | Value |
|---|---|
| `cr_uid` | `0` |
| `cr_ruid` | `0` |
| `cr_svuid` | `0` |
| `cr_ngroups` | `0` |
| `cr_rgid` | `0` |
| `cr_sceAuthId` | `0x4801000000000013` |
| `cr_sceCaps[0]` | `0xFFFFFFFFFFFFFFFF` |
| `cr_sceCaps[1]` | `0xFFFFFFFFFFFFFFFF` |
| `cr_sceAttr0` | `0x80` |
| `fd_rdir` | `rootvnode` |
| `fd_jdir` | `rootvnode` |

The first nine writes lift the target's credential and capability set to the full-authorization
values; the last two point the target's file-descriptor root and jail directories at the kernel's
real root vnode, so every path the target resolves afterwards starts from `/`.

## The complete daemon

```csharp
using System;
using System.Runtime.InteropServices;
using SharpProspero.Payload;

namespace SampleApp;

internal static unsafe partial class Program
{
    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__prospero_klog")]
    private static partial void Klog(byte* message);

    [SuppressGCTransition]
    [LibraryImport("libc", EntryPoint = "getpid")]
    private static partial int Getpid();

    [SuppressGCTransition]
    [LibraryImport("libkernel", EntryPoint = "sceKernelSendNotificationRequest")]
    private static partial int Notify(int device, void* request, nuint size, int blocking);

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_crt_syscall")]
    private static partial long CrtSyscall1(int sysno, long arg1);

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_crt_syscall")]
    private static partial long CrtSyscall2(int sysno, long arg1, long arg2);

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_crt_syscall")]
    private static partial long CrtSyscall3(int sysno, long arg1, long arg2, long arg3);

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_crt_syscall")]
    private static partial long CrtSyscall5(int sysno, long arg1, long arg2, long arg3, long arg4, long arg5);

    private const int SYS_read       = 3;
    private const int SYS_write      = 4;
    private const int SYS_close      = 6;
    private const int SYS_accept     = 30;
    private const int SYS_socket     = 97;
    private const int SYS_bind       = 104;
    private const int SYS_setsockopt = 105;
    private const int SYS_listen     = 106;
    private const int SYS_nanosleep  = 240;

    private const int AF_INET      = 2;
    private const int SOCK_STREAM  = 1;
    private const int SOL_SOCKET   = 0xFFFF;
    private const int SO_REUSEADDR = 0x0004;

    private const int DaemonPort     = 9069;
    private const int CommandSize    = 0xA10;
    private const uint ExpectedMagic = 0xDEADBEEF;
    private const int PromoteCmd     = 5;
    private const int MaxRetries     = 30;

    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        PayloadArgs* pargs = PayloadEntryPoint.Args;
        if (pargs == null) return -1;

        int ownPid = Getpid();
        if (ownPid <= 0) return -1;

        // Self-elevate so subsequent writes stay authorised.
        PayloadKernel.RaisePrivileges(ownPid);
        PayloadKernel.SetUcredAuthId(ownPid, 0x4800000000010003);

        // Cache the root vnode for the daemon's lifetime.
        ulong rootvnode = PayloadKernel.GetRootVnode();
        if (rootvnode == 0) return -1;

        SendNotification("unjail: daemon ready"u8);

        TcpAcceptLoop(rootvnode);
        return 0;
    }

    private static void TcpAcceptLoop(ulong rootvnode)
    {
        int s = (int)CrtSyscall3(SYS_socket, AF_INET, SOCK_STREAM, 0);
        if (s < 0) return;

        int one = 1;
        CrtSyscall5(SYS_setsockopt, s, SOL_SOCKET, SO_REUSEADDR, (long)(nint)(&one), 4);

        // FreeBSD sockaddr_in: sin_len(1), sin_family(1), sin_port(2 BE), sin_addr(4), sin_zero(8).
        byte* addr = stackalloc byte[16];
        new Span<byte>(addr, 16).Clear();
        addr[0] = 16;
        addr[1] = (byte)AF_INET;
        addr[2] = (byte)(DaemonPort >> 8);
        addr[3] = (byte)(DaemonPort & 0xFF);
        addr[4] = 127; addr[5] = 0; addr[6] = 0; addr[7] = 1;  // sin_addr = 127.0.0.1

        if (CrtSyscall3(SYS_bind, s, (long)(nint)addr, 16) < 0) { SysClose(s); return; }
        if (CrtSyscall2(SYS_listen, s, 2) < 0) { SysClose(s); return; }

        byte* cmdBuf   = stackalloc byte[CommandSize];
        byte* replyBuf = stackalloc byte[CommandSize];

        while (true)
        {
            int client = (int)CrtSyscall3(SYS_accept, s, 0, 0);
            if (client < 0) { SleepOneSecond(); continue; }

            int total = 0;
            while (total < CommandSize)
            {
                long n = SysRead(client, cmdBuf + total, CommandSize - total);
                if (n <= 0) break;
                total += (int)n;
            }

            new Span<byte>(replyBuf, CommandSize).Clear();

            if (total >= 16)
            {
                uint magic = *(uint*)(cmdBuf + 0);
                int  cmd   = *(int*)(cmdBuf + 4);
                int  pid   = *(int*)(cmdBuf + 8);

                if (magic == ExpectedMagic && cmd == PromoteCmd && pid > 0)
                {
                    bool ok = false;
                    for (int r = 0; r < MaxRetries && !ok; r++)
                        ok = PayloadKernel.JailbreakByPid(pid, rootvnode);

                    *(int*)(replyBuf + 0x0C) = ok ? 0 : -1;
                }
                else
                {
                    *(int*)(replyBuf + 0x0C) = -1;
                }
            }
            else
            {
                *(int*)(replyBuf + 0x0C) = -1;
            }

            SysWrite(client, replyBuf, CommandSize);
            SysClose(client);
        }
    }

    private static long SysRead(int fd, byte* buf, long count) => CrtSyscall3(SYS_read,  fd, (long)(nint)buf, count);
    private static long SysWrite(int fd, byte* buf, long count) => CrtSyscall3(SYS_write, fd, (long)(nint)buf, count);
    private static int  SysClose(int fd) => (int)CrtSyscall1(SYS_close, fd);

    private static void SleepOneSecond()
    {
        long* ts = stackalloc long[4];
        ts[0] = 1; ts[1] = 0;
        CrtSyscall2(SYS_nanosleep, (long)(nint)ts, (long)(nint)(ts + 2));
    }

    private static void SendNotification(ReadOnlySpan<byte> message)
    {
        byte* req = stackalloc byte[3120];
        new Span<byte>(req, 3120).Clear();
        int len = message.Length;
        if (len > 3074) len = 3074;
        fixed (byte* src = message)
        {
            for (int i = 0; i < len; i++)
                req[45 + i] = src[i];
        }
        Notify(0, req, 3120, 0);
    }
}
```

The sample `samples/prospero-payload-unjail` ships this code with diagnostic `__prospero_klog`
calls at every failure point and a user-visible notification when the daemon starts, plus the
project file that wires up the payload build. Copy the sample if the daemon needs adjustments,
or use it directly.

## Building and sending

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-unjail/SampleApp.csproj -Payload -Output Folder
dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send \
    --host ps5-host-not-configured.invalid \
    --file samples/prospero-payload-unjail/out/SampleApp.elf
```

The daemon runs in the host process on the console until the console reboots. See
[Sending a payload](payload-deploy.md) for the command's options.

## Extending the daemon

The wire format is deliberately simple, so an extended daemon can add commands without breaking
the promotion request:

- **Command `6` — Restore.** Read the caller's ucred before applying the promotion, save it, and
  expose a restore path that writes it back. A module that promotes for one operation and returns
  afterwards uses this pair.
- **Command `7` — Promote another pid.** The caller names both its own pid (`+0x08`) and the
  target (in the reserved buffer). Useful when a controller module promotes a slave module.
- **Command `8` — Read a kernel word.** The daemon returns a `ulong` from a caller-supplied kernel
  address. Diagnostic; do not ship this on a public build.

Each extension follows the same shape: the caller writes the command in `+0x04`, the daemon reads
it in the accept loop, dispatches, and writes the outcome in `+0x0C`. The two 1280-byte reserved
buffers are room to grow into.

## Security considerations

The daemon binds to `127.0.0.1:9069`, so only processes running on the same console can reach it.
Because the console does not distinguish between homebrew processes at the network layer, any
application module — trusted or not — can send the request. That is by design: the daemon is a
user-space authorisation gate the user opens by sending the payload.

Do not bind the daemon to a routable address (`0.0.0.0` or a LAN address). A promotion request
from the network side would let any device on the same LAN escalate homebrew credentials.

---
title: Promoting an application
parent: Application Modules
nav_order: 13
---

# Promoting an application

An application module runs with the per-title credential set the console handed it and inside the
per-title sandbox: `/app0` for its own package files and its per-title writable folder. Promoting
the application widens both — the credential set becomes the full-authorization set, the
capability bitmasks light up, the attribute byte flips on, and the filesystem view opens to the
real root. To reach any of that the module asks a companion payload — the unjail daemon — to apply
the promotion. This page covers why the console requires that pattern, when to promote at run
time, and the complete C# code that asks.

## Why an application module cannot promote itself

Every launch of an application module starts with a fresh credential set and a fresh sandbox. The
console's own program loader re-applies both before the module's first frame — checking the
credential fields, the capability bitmasks, the filesystem view, the loaded modules — because
these are per-launch guarantees. Anything the module writes to its own credential or view state at
run time is wiped on the next launch. A payload the user ran once against a previous launch does
not persist either, because a launch starts a new process.

That leaves two options for a module that has to run outside the per-title bounds:

1. **Promote once, per launch, at run time.** Send a request to a payload that is already running
   in another process — a persistent daemon — and let it apply the promotion from outside. The
   daemon holds the kernel-access surface and can write the module's credential, capability, and
   filesystem-view fields directly.
2. **Do not need to promote.** Restructure the module so its work stays inside `/app0` and the
   per-title writable folder. This is the right answer when the module is a game or a
   self-contained tool.

The rest of this page covers option 1.

## The pattern

The user does two things:

1. **Runs the unjail payload once.** The payload (`prospero-payload-unjail`) starts a background
   daemon in a host process. The daemon binds a TCP socket to `127.0.0.1:9069`, so only processes
   running on the same console can reach it.
2. **Launches the application module.** The module includes a button that reads "Promote this
   application" (or similar). When the user clicks it, the module sends a request to the daemon;
   the daemon applies the promotion; the module re-scans the file system and re-reads any
   authorisation-gated surface to pick up what the promotion now allows.

The module does not promote at startup. Users who do not need the extended access never see the
button do anything meaningful, and users who did not send the daemon see a clear failure message.

## The request

Every request is a fixed 2576-byte struct on the wire:

| Offset | Field | Size | Content |
|---|---|---|---|
| `+0x00` | Magic | `uint32` | `0xDEADBEEF` on the request; the daemon clears it on the reply. |
| `+0x04` | Command | `int32` | `5` for a promotion request. |
| `+0x08` | Pid | `int32` | The caller's process identifier. |
| `+0x0C` | Return | `int32` | `0` on the request; `0` on success, `-1` on failure in the reply. |
| `+0x10` | Reserved 1 | 1280 bytes | Zeroed on both sides. |
| `+0x510` | Reserved 2 | 1280 bytes | Zeroed on both sides. |

The wire format is fixed because the daemon reads a whole struct in one call, applies the writes,
and replies with the outcome slot at offset `0x0C`. The two reserved buffers are what the daemon's
reader relies on for the read size, so their presence matters even though nothing on the wire
uses them.

## The complete C# request

Add this class to the module. It uses only the SDK's own transport and process helpers, so no
extra dependency is needed.

```csharp
using SharpProspero.Application;
using SharpProspero.Platform;
using System;
using System.Buffers.Binary;

namespace MyModule.Shell;

internal static class PromotionRequest
{
    private const int DaemonPort = 9069;
    private const int CommandSize = 0xA10;
    private const uint Magic = 0xDEADBEEF;
    private const int PromoteCommand = 5;
    private const int ReceiveTimeoutMicroseconds = 3_000_000;

    public static bool TryRequest(out string failure)
    {
        try
        {
            using var conn = TcpConnection.Connect(SocketAddress.Loopback(DaemonPort));

            Span<byte> request = stackalloc byte[CommandSize];
            request.Clear();
            BinaryPrimitives.WriteUInt32LittleEndian(request, Magic);
            BinaryPrimitives.WriteInt32LittleEndian(request.Slice(4), PromoteCommand);
            BinaryPrimitives.WriteInt32LittleEndian(request.Slice(8), ProcessInfo.Id);

            conn.SendAll(request);
            conn.SetReceiveTimeout(ReceiveTimeoutMicroseconds);

            Span<byte> reply = stackalloc byte[CommandSize];
            int total = 0;
            while (total < CommandSize)
            {
                int n = conn.Receive(reply.Slice(total));
                if (n <= 0)
                    break;
                total += n;
            }

            if (total < 16)
            {
                failure = "The daemon closed the connection before answering.";
                return false;
            }

            int outcome = BinaryPrimitives.ReadInt32LittleEndian(reply.Slice(0x0C));
            if (outcome != 0)
            {
                failure = "The daemon refused the request.";
                return false;
            }

            failure = string.Empty;
            return true;
        }
        catch (Exception error)
        {
            failure = error.Message;
            return false;
        }
    }
}
```

The types the request uses live in the SDK:

- `SharpProspero.Platform.TcpConnection` — a lightweight TCP client that does not touch the
  sandbox filesystem.
- `SharpProspero.Platform.SocketAddress.Loopback(port)` — a loopback address helper.
- `SharpProspero.Application.ProcessInfo.Id` — the running process's identifier, which the daemon
  needs to know which process to promote.

## Wiring it into the UI

Add a button to the screen the user reaches when they need the extended access. A common shape
puts the button below a "Look for devices again" or "Rescan" button on the module's home screen:

```csharp
menu.Add(new Button("Promote this application", () =>
{
    if (PromotionRequest.TryRequest(out string reason))
    {
        Places.Refresh();
        Shell.Notify($"This application is now promoted. {Places.Reachable.Count} folders can be read.");
    }
    else
    {
        Shell.Notify($"Promotion refused: {reason}");
    }
}));
```

`Places.Refresh()` re-scans the reachable places after the daemon applies the promotion. Every
folder that failed with "permission denied" before the request now returns entries, and any
authorisation-gated service the module reaches for from here on reads its promoted state.

## What the daemon writes

The daemon calls `PayloadKernel.JailbreakByPid(pid, rootvnode)` on the caller's process. That
helper applies eleven writes through the CRT-emitted pid-based accessors:

- `cr_uid = 0`, `cr_ruid = 0`, `cr_svuid = 0`
- `cr_ngroups = 0`
- `cr_rgid = 0`
- `cr_sceAuthId = 0x4801000000000013`
- `cr_sceCaps[0] = 0xFFFFFFFFFFFFFFFF`, `cr_sceCaps[1] = 0xFFFFFFFFFFFFFFFF`
- `cr_sceAttr0 = 0x80`
- `fd_rdir = rootvnode`, `fd_jdir = rootvnode`

The first nine writes lift the credential and capability set to the full-authorization values; the
last two point the file-descriptor root and jail directories at the kernel's real root vnode. See
[Promoting a running application](payload-promotion.md) for the daemon side.

## Failure modes the module has to surface

The request fails when:

- **The daemon is not running.** The connect to `127.0.0.1:9069` fails immediately. Surface as
  "promotion refused: connection refused" and remind the user to send the daemon payload once.
- **The daemon refused.** The reply's outcome slot is non-zero. Surface as "the daemon refused the
  request".
- **The connection dropped mid-reply.** Total bytes read was under 16. Surface as "the daemon
  closed the connection before answering".

In every case the module remains in the per-title credential and sandbox state — nothing was
written, and the state is what it was before the request. The user can try again once the daemon
side is fixed.

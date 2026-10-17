// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using System;

namespace SharpProspero.Platform;

/// <summary>The readiness a socket is watched for, or that a socket reported.</summary>
[Flags]
public enum PollEvents : uint
{
    /// <summary>Nothing.</summary>
    None = 0,

    /// <summary>Readable: data has arrived, or a listener has a pending connection.</summary>
    Read = Socket.EpollIn,

    /// <summary>Writable: the socket can accept more data, or a connect has completed.</summary>
    Write = Socket.EpollOut,

    /// <summary>An error occurred on the socket. Reported, never watched for.</summary>
    Error = Socket.EpollErr,

    /// <summary>The peer hung up. Reported, never watched for.</summary>
    HangUp = Socket.EpollHup,
}

/// <summary>One ready socket, as the poller reports it: the token that was registered and what fired.</summary>
/// <param name="Token">The value registered for the socket, typically an index into the caller's own table.</param>
/// <param name="Events">The readiness that fired.</param>
public readonly record struct PollReady(uint Token, PollEvents Events)
{
    /// <summary>True when the socket is readable.</summary>
    public bool IsReadable => (Events & PollEvents.Read) != 0;

    /// <summary>True when the socket is writable.</summary>
    public bool IsWritable => (Events & PollEvents.Write) != 0;

    /// <summary>True when the socket reported an error or the peer hung up.</summary>
    public bool IsClosed => (Events & (PollEvents.Error | PollEvents.HangUp)) != 0;
}

/// <summary>
/// Watches many sockets at once and reports which are ready, so one thread can run a server that serves
/// many clients. Register each socket with a token the caller chooses, then call <see cref="Wait"/> to
/// block until one or more are ready. Set the sockets it watches to non-blocking so a ready socket is
/// serviced without stalling the loop.
/// </summary>
/// <example>
/// <code>
/// using var poller = SocketPoller.Create();
/// listener.Blocking = false;
/// poller.Add(listener.Handle, PollEvents.Read, token: 0);
/// Span&lt;PollReady&gt; ready = stackalloc PollReady[16];
/// int count = poller.Wait(ready, timeoutMicroseconds: -1);
/// </code>
/// </example>
public sealed unsafe class SocketPoller : IDisposable
{
    private int _epoll;
    private bool _disposed;

    private SocketPoller(int epoll) => _epoll = epoll;

    /// <summary>Creates a poller.</summary>
    /// <exception cref="ProsperoException">The poller could not be created.</exception>
    public static SocketPoller Create()
    {
        int epoll = SocketError.Check(Socket.sceNetEpollCreate("sp_poll", 0), nameof(Socket.sceNetEpollCreate));
        return new SocketPoller(epoll);
    }

    /// <summary>Watches <paramref name="socket"/> for <paramref name="events"/>, tagging it with <paramref name="token"/>.</summary>
    /// <exception cref="ProsperoException">The socket could not be added.</exception>
    public void Add(int socket, PollEvents events, uint token) => Control(Socket.EpollCtlAdd, socket, events, token);

    /// <summary>Changes what <paramref name="socket"/> is watched for and the token it carries.</summary>
    /// <exception cref="ProsperoException">The socket could not be changed.</exception>
    public void Modify(int socket, PollEvents events, uint token) => Control(Socket.EpollCtlMod, socket, events, token);

    /// <summary>Stops watching <paramref name="socket"/>.</summary>
    /// <exception cref="ProsperoException">The socket could not be removed.</exception>
    public void Remove(int socket)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        // Removing a socket describes nothing about it, so nothing is passed. Handing over an empty
        // record instead of none is refused: there is no event to register and the call says so.
        SocketError.Check(
            Socket.sceNetEpollControl(_epoll, Socket.EpollCtlDel, socket, null), nameof(Socket.sceNetEpollControl));
    }

    private void Control(int op, int socket, PollEvents events, uint token)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var ev = new SceNetEpollEvent { Events = (uint)events, Data = new SceNetEpollData { U32 = token } };
        SocketError.Check(
            Socket.sceNetEpollControl(_epoll, op, socket, &ev), nameof(Socket.sceNetEpollControl));
    }

    /// <summary>
    /// Waits until at least one watched socket is ready, or the timeout passes, and fills
    /// <paramref name="ready"/> with the sockets that fired. <paramref name="timeoutMicroseconds"/> is
    /// the longest to wait; a negative value waits forever. Returns the number of entries written,
    /// which is zero when the timeout passes with nothing ready.
    /// </summary>
    /// <exception cref="ProsperoException">The wait failed.</exception>
    public int Wait(Span<PollReady> ready, int timeoutMicroseconds)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (ready.IsEmpty)
            return 0;

        // Batch what the caller can hold. A small batch stays on the stack; a large one is taken from
        // the heap so a big destination span cannot overflow the stack.
        const int stackBatch = 64;
        int max = ready.Length;
        Span<SceNetEpollEvent> stackBuffer = stackalloc SceNetEpollEvent[stackBatch];
        Span<SceNetEpollEvent> events = max <= stackBatch ? stackBuffer[..max] : new SceNetEpollEvent[max];

        int count;
        fixed (SceNetEpollEvent* p = events)
            count = SocketError.Check(
                Socket.sceNetEpollWait(_epoll, p, max, timeoutMicroseconds), nameof(Socket.sceNetEpollWait));

        for (int i = 0; i < count; i++)
            ready[i] = new PollReady(events[i].Data.U32, (PollEvents)events[i].Events);
        return count;
    }

    /// <summary>Unblocks a thread parked in <see cref="Wait"/>, so a server loop can shut down.</summary>
    public void Abort()
    {
        if (_disposed)
            return;
        Socket.sceNetEpollAbort(_epoll, 0);
    }

    /// <summary>Destroys the poller.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_epoll >= 0)
        {
            Socket.sceNetEpollDestroy(_epoll);
            _epoll = -1;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Destroys the poller if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~SocketPoller() => Dispose();
}

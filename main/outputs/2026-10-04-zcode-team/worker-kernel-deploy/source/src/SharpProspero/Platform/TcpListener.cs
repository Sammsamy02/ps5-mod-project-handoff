// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// A listening TCP socket that accepts incoming connections, the server side of a network tool. Bind it
/// to a local address, then accept clients. Each accepted client is a <see cref="TcpConnection"/> the
/// caller owns and disposes. For a server that handles many clients at once without threads, set
/// <see cref="Blocking"/> to false and drive the listener and its connections from a <see cref="SocketPoller"/>.
/// </summary>
/// <example>
/// <code>
/// using var listener = TcpListener.Listen(SocketAddress.Any(8080));
/// using TcpConnection client = listener.Accept();
/// </code>
/// </example>
public sealed unsafe class TcpListener : IDisposable
{
    private int _socket;
    private bool _disposed;

    private TcpListener(int socket) => _socket = socket;

    /// <summary>The underlying socket id, for registering the listener with a <see cref="SocketPoller"/>.</summary>
    public int Handle => _socket;

    /// <summary>
    /// Binds to <paramref name="address"/> and starts listening. <paramref name="backlog"/> is the
    /// number of pending connections the system may queue before refusing more.
    /// </summary>
    /// <exception cref="ProsperoException">The socket could not be created, bound, or set to listen.</exception>
    public static TcpListener Listen(SocketAddress address, int backlog = 16)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(backlog);

        int socket = SocketError.Check(
            Socket.sceNetSocket("sp_listen", Socket.AfInet, Socket.SockStream, Socket.IpProtoTcp),
            nameof(Socket.sceNetSocket));
        try
        {
            SocketOptions.SetReuseAddress(socket, true);

            SceNetSockaddrIn native = address.ToNative();
            SocketError.Check(Socket.sceNetBind(socket, (SceNetSockaddr*)&native, 16), nameof(Socket.sceNetBind));
            SocketError.Check(Socket.sceNetListen(socket, backlog), nameof(Socket.sceNetListen));
            return new TcpListener(socket);
        }
        catch
        {
            Socket.sceNetSocketClose(socket);
            throw;
        }
    }

    /// <summary>
    /// Accepts one connection. On a blocking listener this waits until a client arrives; on a
    /// non-blocking listener it throws when none is ready, so poll for readiness first.
    /// </summary>
    /// <exception cref="ProsperoException">The accept failed.</exception>
    public TcpConnection Accept()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int client = SocketError.Check(Socket.sceNetAccept(_socket, null, null), nameof(Socket.sceNetAccept));
        return new TcpConnection(client);
    }

    /// <summary>The local endpoint the listener is bound to, resolving a zero port to the one assigned.</summary>
    public SocketAddress LocalAddress
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            SceNetSockaddrIn native = default;
            uint length = 16;
            if (Socket.sceNetGetsockname(_socket, (SceNetSockaddr*)&native, &length) < 0)
                return default;
            return SocketAddress.FromNative(native);
        }
    }

    /// <summary>Whether the listener blocks in <see cref="Accept"/> (the default) or returns immediately.</summary>
    /// <exception cref="ProsperoException">The mode could not be changed.</exception>
    public bool Blocking
    {
        set => SocketOptions.SetBlocking(_socket, value);
    }

    /// <summary>Stops the listener and releases its socket.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_socket >= 0)
        {
            Socket.sceNetSocketClose(_socket);
            _socket = -1;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Closes the socket if the listener was dropped without a <see cref="Dispose"/> call.</summary>
    ~TcpListener() => Dispose();
}

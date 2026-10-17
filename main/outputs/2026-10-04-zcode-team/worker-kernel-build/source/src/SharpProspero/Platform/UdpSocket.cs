// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// A datagram (UDP) socket: send a message to an address, receive a message and learn who sent it. Bind
/// it to a local port to receive, or leave it unbound to only send. Datagrams are independent, so there
/// is no connection and each send and receive is one whole message.
/// </summary>
/// <example>
/// <code>
/// using var udp = UdpSocket.Bind(SocketAddress.Any(9000));
/// Span&lt;byte&gt; buffer = stackalloc byte[1500];
/// int read = udp.ReceiveFrom(buffer, out SocketAddress sender);
/// </code>
/// </example>
public sealed unsafe class UdpSocket : IDisposable
{
    private int _socket;
    private bool _disposed;

    private UdpSocket(int socket) => _socket = socket;

    /// <summary>The underlying socket id, for registering the socket with a <see cref="SocketPoller"/>.</summary>
    public int Handle => _socket;

    /// <summary>Creates a datagram socket that can send but is not bound to a local port.</summary>
    /// <exception cref="ProsperoException">The socket could not be created.</exception>
    public static UdpSocket Create()
    {
        int socket = SocketError.Check(
            Socket.sceNetSocket("sp_udp", Socket.AfInet, Socket.SockDgram, Socket.IpProtoUdp),
            nameof(Socket.sceNetSocket));
        return new UdpSocket(socket);
    }

    /// <summary>Creates a datagram socket bound to <paramref name="address"/>, ready to receive.</summary>
    /// <exception cref="ProsperoException">The socket could not be created or bound.</exception>
    public static UdpSocket Bind(SocketAddress address)
    {
        UdpSocket udp = Create();
        try
        {
            SceNetSockaddrIn native = address.ToNative();
            SocketError.Check(Socket.sceNetBind(udp._socket, (SceNetSockaddr*)&native, 16), nameof(Socket.sceNetBind));
            return udp;
        }
        catch
        {
            udp.Dispose();
            throw;
        }
    }

    /// <summary>Sends <paramref name="data"/> as one datagram to <paramref name="destination"/>.</summary>
    /// <returns>The number of bytes sent.</returns>
    /// <exception cref="ProsperoException">The send failed.</exception>
    public int SendTo(ReadOnlySpan<byte> data, SocketAddress destination)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceNetSockaddrIn native = destination.ToNative();
        fixed (byte* p = data)
            return SocketError.Check(
                Socket.sceNetSendto(_socket, p, (nuint)data.Length, 0, (SceNetSockaddr*)&native, 16),
                nameof(Socket.sceNetSendto));
    }

    /// <summary>
    /// Receives one datagram into <paramref name="buffer"/> and reports the sender. A datagram longer
    /// than the buffer is truncated to it. Returns the number of bytes received.
    /// </summary>
    /// <exception cref="ProsperoException">The receive failed.</exception>
    public int ReceiveFrom(Span<byte> buffer, out SocketAddress sender)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceNetSockaddrIn native = default;
        uint length = 16;
        int read;
        fixed (byte* p = buffer)
            read = SocketError.Check(
                Socket.sceNetRecvfrom(_socket, p, (nuint)buffer.Length, 0, (SceNetSockaddr*)&native, &length),
                nameof(Socket.sceNetRecvfrom));
        sender = SocketAddress.FromNative(native);
        return read;
    }

    /// <summary>Whether the socket blocks (the default) or returns immediately when nothing is ready.</summary>
    /// <exception cref="ProsperoException">The mode could not be changed.</exception>
    public bool Blocking
    {
        set => SocketOptions.SetBlocking(_socket, value);
    }

    /// <summary>Allows sending to the broadcast address.</summary>
    /// <exception cref="ProsperoException">The option could not be set.</exception>
    public void EnableBroadcast() =>
        SocketOptions.SetUInt(_socket, Socket.SolSocket, Socket.SoBroadcast, 1);

    /// <summary>Closes the socket.</summary>
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

    /// <summary>Closes the socket if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~UdpSocket() => Dispose();
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Posix;
using System;

namespace SharpProspero.Payload.Net;

/// <summary>
/// Plain TCP for a payload, over the socket calls the operating-system library publishes under their
/// own names.
/// </summary>
/// <remarks>
/// A payload has no dynamic linker, so it cannot bind the wrapped network library an application module
/// uses; it reaches the operating-system library by name at run time, and only the plain socket calls
/// resolve. This is the small amount of that surface a headless service needs. An application module
/// should use the fuller network types in <see cref="Platform"/> instead.
/// </remarks>
public static unsafe class PayloadNetwork
{
    /// <summary>
    /// Opens a socket listening on <paramref name="port"/> on every local address, and returns its
    /// descriptor. The caller accepts connections with <see cref="Accept"/> and closes it with
    /// <see cref="Close"/>.
    /// </summary>
    /// <exception cref="InvalidOperationException">The socket could not be opened, bound or listened on.</exception>
    public static int Listen(ushort port, int backlog = 8)
    {
        int socket = PosixSocket.socket(PosixSocket.AfInet, PosixSocket.SockStream, 0);
        if (socket < 0)
            throw new InvalidOperationException($"socket failed ({socket}).");

        // A listener that has just closed leaves its address winding down, so reuse is set to let a
        // restart bind the same port at once rather than waiting the address out.
        int reuse = 1;
        PosixSocket.setsockopt(socket, PosixSocket.SolSocket, PosixSocket.SoReuseAddr, &reuse, sizeof(int));

        var address = new SockaddrIn
        {
            Length = 16,
            Family = (byte)PosixSocket.AfInet,
            Port = PosixSocket.HostToNetwork(port),
            Address = PosixSocket.InAddrAny,
        };
        if (PosixSocket.bind(socket, &address, 16) < 0)
        {
            PosixSocket.close(socket);
            throw new InvalidOperationException("bind failed. The port may be in use.");
        }
        if (PosixSocket.listen(socket, backlog) < 0)
        {
            PosixSocket.close(socket);
            throw new InvalidOperationException("listen failed.");
        }
        return socket;
    }

    /// <summary>
    /// Connects to <paramref name="a"/>.<paramref name="b"/>.<paramref name="c"/>.<paramref name="d"/>
    /// on <paramref name="port"/> and returns the socket descriptor.
    /// </summary>
    /// <exception cref="InvalidOperationException">The socket could not be opened or connected.</exception>
    public static int Connect(byte a, byte b, byte c, byte d, ushort port)
    {
        int socket = PosixSocket.socket(PosixSocket.AfInet, PosixSocket.SockStream, 0);
        if (socket < 0)
            throw new InvalidOperationException($"socket failed ({socket}).");

        var address = new SockaddrIn
        {
            Length = 16,
            Family = (byte)PosixSocket.AfInet,
            Port = PosixSocket.HostToNetwork(port),
            Address = (uint)(a | (b << 8) | (c << 16) | (d << 24)), // already network order, low byte first
        };
        if (PosixSocket.connect(socket, &address, 16) < 0)
        {
            PosixSocket.close(socket);
            throw new InvalidOperationException("connect failed.");
        }
        return socket;
    }

    /// <summary>Takes the next waiting connection on a listening socket, or a negative error.</summary>
    public static int Accept(int listeningSocket)
    {
        uint length = 16;
        var address = default(SockaddrIn);
        return PosixSocket.accept(listeningSocket, &address, &length);
    }

    /// <summary>
    /// Sends every byte of <paramref name="data"/>, looping until it is all gone. Returns false when the
    /// connection failed part way.
    /// </summary>
    public static bool SendAll(int socket, ReadOnlySpan<byte> data)
    {
        fixed (byte* p = data)
        {
            nuint sent = 0;
            while (sent < (nuint)data.Length)
            {
                long n = PosixSocket.send(socket, p + sent, (nuint)data.Length - sent, 0);
                if (n <= 0)
                    return false;
                sent += (nuint)n;
            }
        }
        return true;
    }

    /// <summary>
    /// Reads into <paramref name="buffer"/>, returning how many bytes arrived, zero at the end, or a
    /// negative error.
    /// </summary>
    public static long Receive(int socket, Span<byte> buffer)
    {
        fixed (byte* p = buffer)
            return PosixSocket.recv(socket, p, (nuint)buffer.Length, 0);
    }

    /// <summary>Closes a socket.</summary>
    public static void Close(int socket) => PosixSocket.close(socket);
}

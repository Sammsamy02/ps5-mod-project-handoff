// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Payload.Net;

/// <summary>
/// A poll-based non-blocking TCP server pattern. Wraps the accept/read/write loop with
/// poll-based waiting, SO_REUSEADDR+SO_REUSEPORT, and MSG_NOSIGNAL sends.
/// </summary>
public static unsafe class PayloadTcpServer
{
    /// <summary>
    /// Creates a listening socket on the given port with SO_REUSEADDR and SO_REUSEPORT
    /// set. Returns the socket descriptor.
    /// </summary>
    public static int Create(ushort port, int backlog = 8)
    {
        return PayloadNetwork.Listen(port, backlog);
    }

    /// <summary>
    /// Waits for a connection with a timeout using poll. Returns the accepted client
    /// socket, or -1 on timeout/error.
    /// </summary>
    /// <param name="listener">The listening socket from <see cref="Create"/>.</param>
    /// <param name="timeoutMs">Timeout in milliseconds, or -1 for infinite.</param>
    public static int AcceptWithTimeout(int listener, int timeoutMs)
    {
        PollFd pfd = new() { Fd = listener, Events = PayloadNetworkUtil.PollIn };
        int ready = PayloadNetworkUtil.poll(&pfd, 1, timeoutMs);
        if (ready <= 0) return -1;
        if ((pfd.Revents & PayloadNetworkUtil.PollIn) == 0) return -1;
        return PayloadNetwork.Accept(listener);
    }

    /// <summary>
    /// Reads exactly <paramref name="length"/> bytes from a socket. Returns the number
    /// of bytes actually read (which may be less than requested if the connection closed).
    /// </summary>
    public static int ReadExact(int socket, byte* buffer, int length)
    {
        int total = 0;
        while (total < length)
        {
            long n = PayloadNetwork.Receive(socket, new Span<byte>(buffer + total, length - total));
            if (n <= 0) break;
            total += (int)n;
        }
        return total;
    }

    /// <summary>
    /// Sends all bytes in <paramref name="data"/> through the socket with MSG_NOSIGNAL.
    /// Returns <see langword="true"/> on success.
    /// </summary>
    public static bool SendAll(int socket, ReadOnlySpan<byte> data)
    {
        return PayloadNetwork.SendAll(socket, data);
    }

    /// <summary>
    /// Sends all bytes from a pointer with MSG_NOSIGNAL.
    /// </summary>
    public static bool SendAll(int socket, byte* data, int length)
    {
        return PayloadNetwork.SendAll(socket, new ReadOnlySpan<byte>(data, length));
    }

    /// <summary>
    /// Closes a socket.
    /// </summary>
    public static void Close(int socket) => PayloadNetwork.Close(socket);
}

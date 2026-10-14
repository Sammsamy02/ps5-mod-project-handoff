// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Net;

/// <summary>
/// FreeBSD <c>struct pollfd</c> for the <see cref="PayloadNetworkUtil.poll"/> system call.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct PollFd
{
    /// <summary>The file descriptor to poll.</summary>
    public int Fd;

    /// <summary>Events to poll for (input).</summary>
    public short Events;

    /// <summary>Events that occurred (output).</summary>
    public short Revents;
}

/// <summary>
/// Network utility functions and I/O multiplexing for a payload context. Wraps <c>poll</c>,
/// byte-order conversion, address conversion, and socket inspection from <c>libc</c>.
/// </summary>
public static unsafe partial class PayloadNetworkUtil
{
    private const string Lib = "libc";
    private const string LibKernel = "libkernel";

    /// <summary>Data may be read without blocking.</summary>
    public const short PollIn = 0x0001;

    /// <summary>Data may be written without blocking.</summary>
    public const short PollOut = 0x0004;

    /// <summary>An error condition on the descriptor.</summary>
    public const short PollErr = 0x0008;

    /// <summary>Peer closed the connection.</summary>
    public const short PollHup = 0x0010;

    /// <summary>Invalid file descriptor.</summary>
    public const short PollNval = 0x0020;

    /// <summary>
    /// Waits for events on a set of file descriptors.
    /// </summary>
    /// <param name="fds">Array of <see cref="PollFd"/> entries to monitor.</param>
    /// <param name="nfds">Number of entries in <paramref name="fds"/>.</param>
    /// <param name="timeout">Timeout in milliseconds, or -1 for infinite.</param>
    /// <returns>The number of descriptors with events, 0 on timeout, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int poll(PollFd* fds, uint nfds, int timeout);

    /// <summary>Converts a 16-bit value from host to network byte order.</summary>
    public static ushort Htons(ushort value) =>
        (ushort)((value >> 8) | (value << 8));

    /// <summary>Converts a 32-bit value from host to network byte order.</summary>
    public static uint Htonl(uint value) =>
        (value >> 24) |
        ((value >> 8) & 0x0000FF00) |
        ((value << 8) & 0x00FF0000) |
        (value << 24);

    /// <summary>Converts a 16-bit value from network to host byte order.</summary>
    public static ushort Ntohs(ushort value) => Htons(value);

    /// <summary>Converts a 32-bit value from network to host byte order.</summary>
    public static uint Ntohl(uint value) => Htonl(value);

    /// <summary>
    /// Converts a numeric address in <paramref name="src"/> to a text string in
    /// <paramref name="dst"/>. <paramref name="af"/> is <c>AF_INET</c> (2) or
    /// <c>AF_INET6</c> (28).
    /// </summary>
    /// <returns>A pointer to <paramref name="dst"/> on success, or null on error.</returns>
    [LibraryImport(Lib)]
    public static partial byte* inet_ntop(int af, void* src, byte* dst, uint size);

    /// <summary>
    /// Converts a text address in <paramref name="src"/> to a numeric address in
    /// <paramref name="dst"/>. <paramref name="af"/> is <c>AF_INET</c> (2) or
    /// <c>AF_INET6</c> (28).
    /// </summary>
    /// <returns>1 on success, 0 if the string is not a valid address, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int inet_pton(int af, byte* src, void* dst);

    /// <summary>
    /// Reads the local address of a bound socket.
    /// </summary>
    /// <param name="sockfd">The socket file descriptor.</param>
    /// <param name="addr">Buffer to receive the address.</param>
    /// <param name="addrlen">On entry, the buffer size; on exit, the actual address size.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int getsockname(int sockfd, void* addr, uint* addrlen);

    /// <summary>
    /// Reads the remote address of a connected socket.
    /// </summary>
    /// <param name="sockfd">The socket file descriptor.</param>
    /// <param name="addr">Buffer to receive the address.</param>
    /// <param name="addrlen">On entry, the buffer size; on exit, the actual address size.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int getpeername(int sockfd, void* addr, uint* addrlen);

    /// <summary>IPv4 address family.</summary>
    public const int AfInet = 2;

    /// <summary>IPv6 address family.</summary>
    public const int AfInet6 = 28;

    /// <summary>Size of an IPv4 address text buffer (16 bytes including NUL).</summary>
    public const int Inet4AddrStrLen = 16;

    /// <summary>Size of an IPv6 address text buffer (46 bytes including NUL).</summary>
    public const int Inet6AddrStrLen = 46;
}

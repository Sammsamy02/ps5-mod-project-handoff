// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.IO;
using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Net;

/// <summary>
/// FreeBSD <c>struct sockaddr_un</c> for AF_UNIX domain sockets.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SockaddrUn
{
    /// <summary>Total length of the address (sizeof struct).</summary>
    public byte SunLen;

    /// <summary>Address family (<c>AF_UNIX</c> = 1).</summary>
    public byte SunFamily;

    /// <summary>Socket path (up to 104 bytes, NUL-terminated).</summary>
    public fixed byte SunPath[104];
}

/// <summary>
/// FreeBSD <c>struct msghdr</c> for scatter/gather I/O with ancillary data.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdMsghdr
{
    /// <summary>Optional address.</summary>
    public void* MsgName;

    /// <summary>Size of address.</summary>
    public uint MsgNamelen;

    /// <summary>Scatter/gather array.</summary>
    public FreeBsdIovec* MsgIov;

    /// <summary>Number of elements in <see cref="MsgIov"/>.</summary>
    public int MsgIovlen;

    /// <summary>Ancillary data (e.g. file descriptors).</summary>
    public void* MsgControl;

    /// <summary>Size of ancillary data.</summary>
    public uint MsgControllen;

    /// <summary>Flags on received message.</summary>
    public int MsgFlags;
}

/// <summary>
/// Unix domain sockets and scatter/gather I/O for a payload context.
/// </summary>
public static unsafe partial class PayloadUnixSocket
{
    private const string Lib = "libkernel";

    /// <summary>AF_UNIX / AF_LOCAL address family.</summary>
    public const int AfUnix = 1;

    /// <summary>
    /// Creates a pair of connected sockets.
    /// </summary>
    /// <param name="domain">Address family (<see cref="AfUnix"/>).</param>
    /// <param name="type">Socket type (SOCK_STREAM = 1, SOCK_DGRAM = 2).</param>
    /// <param name="protocol">Protocol (0 for default).</param>
    /// <param name="sv">Array of two integers to receive the socket descriptors.</param>
    [LibraryImport(Lib)]
    public static partial int socketpair(int domain, int type, int protocol, int* sv);

    /// <summary>
    /// Sends a message with scatter/gather I/O and optional ancillary data (file descriptor
    /// passing).
    /// </summary>
    [LibraryImport(Lib)]
    public static partial long sendmsg(int sockfd, FreeBsdMsghdr* msg, int flags);

    /// <summary>
    /// Receives a message with scatter/gather I/O and optional ancillary data.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial long recvmsg(int sockfd, FreeBsdMsghdr* msg, int flags);

    /// <summary>
    /// Sends data to a specific address (UDP or unconnected socket).
    /// </summary>
    [LibraryImport(Lib)]
    public static partial long sendto(int sockfd, void* buf, nuint len, int flags,
        void* destAddr, uint addrlen);

    /// <summary>
    /// Receives data and captures the source address.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial long recvfrom(int sockfd, void* buf, nuint len, int flags,
        void* srcAddr, uint* addrlen);

    /// <summary>
    /// Enumerates all network interfaces and their addresses.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int getifaddrs(void** ifap);

    /// <summary>
    /// Frees the interface list returned by <see cref="getifaddrs"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial void freeifaddrs(void* ifa);

    /// <summary>
    /// Shuts down part of a full-duplex connection.
    /// </summary>
    /// <param name="sockfd">The socket descriptor.</param>
    /// <param name="how">0 = stop receiving, 1 = stop sending, 2 = stop both.</param>
    [LibraryImport(Lib)]
    public static partial int shutdown(int sockfd, int how);

    /// <summary>MSG_NOSIGNAL — do not generate SIGPIPE on broken connection.</summary>
    public const int MsgNoSignal = 0x20000;

    /// <summary>SHUT_RD — stop receiving.</summary>
    public const int ShutRd = 0;

    /// <summary>SHUT_WR — stop sending.</summary>
    public const int ShutWr = 1;

    /// <summary>SHUT_RDWR — stop both.</summary>
    public const int ShutRdwr = 2;
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Posix;

/// <summary>
/// An IPv4 address, the form the socket calls below take.
/// </summary>
/// <remarks>
/// The length lives in the first byte and the family in the second, which is the platform's own layout
/// and not the sixteen-bit family the run time was built against. The port and the address are in
/// network byte order.
/// </remarks>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SockaddrIn
{
    /// <summary>The length of this address, sixteen.</summary>
    public byte Length;

    /// <summary>The address family, <see cref="PosixSocket.AfInet"/> for IPv4.</summary>
    public byte Family;

    /// <summary>The port, in network byte order.</summary>
    public ushort Port;

    /// <summary>The IPv4 address, in network byte order.</summary>
    public uint Address;

    private fixed byte _zero[8];
}

/// <summary>
/// The platform's own socket calls, the ones the operating-system library publishes under their plain
/// names rather than the wrapped forms the higher-level network library offers.
/// </summary>
/// <remarks>
/// These are here for a payload. A payload has no dynamic linker to bind the wrapped network library, so
/// it reaches the operating-system library by the plain name at run time, and only the plain names
/// resolve. The wrapped forms in <see cref="Net"/> are for an application module and do not resolve in a
/// payload. An application module may use either, but has no reason to prefer these.
/// </remarks>
public static unsafe partial class PosixSocket
{
    private const string Lib = "libScePosix";

    /// <summary>The IPv4 address family.</summary>
    public const int AfInet = 2;

    /// <summary>A reliable byte stream.</summary>
    public const int SockStream = 1;

    /// <summary>A datagram.</summary>
    public const int SockDgram = 2;

    /// <summary>The socket-level option set, for <see cref="setsockopt"/>.</summary>
    public const int SolSocket = 0xFFFF;

    /// <summary>Reuse a local address that is still winding down from a previous socket.</summary>
    public const int SoReuseAddr = 0x0004;

    /// <summary>Bind to every local address.</summary>
    public const uint InAddrAny = 0;

    /// <summary>The TCP protocol.</summary>
    public const int IpProtoTcp = 6;

    /// <summary>Puts a sixteen-bit value into network byte order.</summary>
    public static ushort HostToNetwork(ushort value) => (ushort)((value >> 8) | (value << 8));

    /// <summary>Opens a socket, returning its descriptor or a negative error.</summary>
    [LibraryImport(Lib)]
    public static partial int socket(int domain, int type, int protocol);

    /// <summary>Gives the socket a local address.</summary>
    [LibraryImport(Lib)]
    public static partial int bind(int socket, SockaddrIn* address, uint addressLength);

    /// <summary>Marks the socket as one that accepts connections, holding up to <paramref name="backlog"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int listen(int socket, int backlog);

    /// <summary>Takes the next waiting connection, returning a new descriptor for it.</summary>
    [LibraryImport(Lib)]
    public static partial int accept(int socket, SockaddrIn* address, uint* addressLength);

    /// <summary>Connects the socket to <paramref name="address"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int connect(int socket, SockaddrIn* address, uint addressLength);

    /// <summary>Sends up to <paramref name="length"/> bytes, returning how many were sent or a negative error.</summary>
    [LibraryImport(Lib)]
    public static partial long send(int socket, void* buffer, nuint length, int flags);

    /// <summary>Reads up to <paramref name="length"/> bytes, returning how many arrived, zero at the end, or a negative error.</summary>
    [LibraryImport(Lib)]
    public static partial long recv(int socket, void* buffer, nuint length, int flags);

    /// <summary>Sets a socket option.</summary>
    [LibraryImport(Lib)]
    public static partial int setsockopt(int socket, int level, int option, void* value, uint valueLength);

    /// <summary>Stops one or both directions of a socket. <paramref name="how"/> is 0 receives, 1 sends, 2 both.</summary>
    [LibraryImport(Lib)]
    public static partial int shutdown(int socket, int how);

    /// <summary>Closes a descriptor.</summary>
    [LibraryImport(Lib)]
    public static partial int close(int descriptor);
}

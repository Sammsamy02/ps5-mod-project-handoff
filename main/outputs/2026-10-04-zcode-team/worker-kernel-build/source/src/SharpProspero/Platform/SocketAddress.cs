// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Net;
using System;
using System.Buffers.Binary;

namespace SharpProspero.Platform;

/// <summary>
/// An IPv4 endpoint: a dotted-quad address and a port. It is the address a socket binds to, connects
/// to, or reports for a peer. Build one from four bytes, from a text address, or from the well-known
/// any and loopback addresses.
/// </summary>
/// <example>
/// <code>
/// var listen = SocketAddress.Any(8080);              // every interface, port 8080
/// var server = SocketAddress.Parse("ps5-host-not-configured.invalid", 21);
/// </code>
/// </example>
public readonly struct SocketAddress : IEquatable<SocketAddress>
{
    /// <summary>The first (most significant) octet of the address.</summary>
    public byte A { get; }

    /// <summary>The second octet of the address.</summary>
    public byte B { get; }

    /// <summary>The third octet of the address.</summary>
    public byte C { get; }

    /// <summary>The fourth (least significant) octet of the address.</summary>
    public byte D { get; }

    /// <summary>The port, 0 to 65535.</summary>
    public int Port { get; }

    /// <summary>Creates an endpoint from the four address octets and a port.</summary>
    /// <exception cref="ArgumentOutOfRangeException">The port is outside 0 to 65535.</exception>
    public SocketAddress(byte a, byte b, byte c, byte d, int port)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(port);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(port, 65535);
        A = a;
        B = b;
        C = c;
        D = d;
        Port = port;
    }

    /// <summary>The address every interface listens on (0.0.0.0) with <paramref name="port"/>.</summary>
    public static SocketAddress Any(int port) => new(0, 0, 0, 0, port);

    /// <summary>The loopback address (127.0.0.1) with <paramref name="port"/>.</summary>
    public static SocketAddress Loopback(int port) => new(127, 0, 0, 1, port);

    /// <summary>Parses a dotted-quad address such as <c>"ps5-host-not-configured.invalid"</c> with a port.</summary>
    /// <exception cref="FormatException">The address is not four octets separated by dots.</exception>
    /// <exception cref="ArgumentOutOfRangeException">The port is outside 0 to 65535.</exception>
    public static SocketAddress Parse(string ip, int port)
    {
        if (!TryParse(ip, port, out SocketAddress address))
            throw new FormatException($"'{ip}' is not a dotted-quad IPv4 address.");
        return address;
    }

    /// <summary>Parses a dotted-quad address and port, returning false instead of throwing on a bad address.</summary>
    public static bool TryParse(string ip, int port, out SocketAddress address)
    {
        address = default;
        if (string.IsNullOrEmpty(ip) || (uint)port > 65535)
            return false;

        Span<byte> octets = stackalloc byte[4];
        int part = 0;
        int value = -1;
        foreach (char ch in ip)
        {
            if (ch == '.')
            {
                if (value < 0 || part >= 3)
                    return false;
                octets[part++] = (byte)value;
                value = -1;
            }
            else if (ch is >= '0' and <= '9')
            {
                value = (value < 0 ? 0 : value) * 10 + (ch - '0');
                if (value > 255)
                    return false;
            }
            else
            {
                return false;
            }
        }
        if (value < 0 || part != 3)
            return false;
        octets[3] = (byte)value;

        address = new SocketAddress(octets[0], octets[1], octets[2], octets[3], port);
        return true;
    }

    /// <summary>The address as dotted-quad text, without the port.</summary>
    public string IpString => $"{A}.{B}.{C}.{D}";

    /// <summary>The endpoint as <c>address:port</c>.</summary>
    public override string ToString() => $"{A}.{B}.{C}.{D}:{Port}";

    /// <summary>The address octets packed in network byte order, as the socket structure holds them.</summary>
    internal uint NetworkAddress => (uint)(A | (B << 8) | (C << 16) | (D << 24));

    /// <summary>The port in network byte order, as the socket structure holds it.</summary>
    internal ushort NetworkPort => BinaryPrimitives.ReverseEndianness((ushort)Port);

    /// <summary>Fills a socket address structure with this endpoint.</summary>
    internal SceNetSockaddrIn ToNative() => new()
    {
        Len = 16,
        Family = (byte)Socket.AfInet,
        Port = NetworkPort,
        Addr = NetworkAddress,
        VPort = 0,
    };

    /// <summary>Reads an endpoint back from a socket address structure.</summary>
    internal static SocketAddress FromNative(in SceNetSockaddrIn addr)
    {
        int port = BinaryPrimitives.ReverseEndianness(addr.Port);
        return FromNetworkAddress(addr.Addr, port);
    }

    /// <summary>Builds an endpoint from a network-order address word and a port.</summary>
    internal static SocketAddress FromNetworkAddress(uint networkAddress, int port) =>
        new((byte)(networkAddress & 0xFF), (byte)((networkAddress >> 8) & 0xFF),
            (byte)((networkAddress >> 16) & 0xFF), (byte)((networkAddress >> 24) & 0xFF), port);

    /// <inheritdoc/>
    public bool Equals(SocketAddress other) =>
        A == other.A && B == other.B && C == other.C && D == other.D && Port == other.Port;

    /// <inheritdoc/>
    public override bool Equals(object? obj) => obj is SocketAddress other && Equals(other);

    /// <inheritdoc/>
    public override int GetHashCode() => HashCode.Combine(A, B, C, D, Port);

    /// <summary>Compares two endpoints for equality.</summary>
    public static bool operator ==(SocketAddress left, SocketAddress right) => left.Equals(right);

    /// <summary>Compares two endpoints for inequality.</summary>
    public static bool operator !=(SocketAddress left, SocketAddress right) => !left.Equals(right);
}

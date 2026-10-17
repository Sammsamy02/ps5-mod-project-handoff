// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// Turns a host name into an address, so a client can connect to <c>"example.com"</c> rather than a
/// numeric address. Create one, resolve as many names as needed, dispose it. A resolver draws on a
/// small network pool it owns for the lifetime of the object.
/// </summary>
/// <example>
/// <code>
/// using var dns = HostResolver.Create();
/// SocketAddress address = dns.Resolve("example.com", 80);
/// using var conn = TcpConnection.Connect(address);
/// </code>
/// </example>
public sealed unsafe class HostResolver : IDisposable
{
    private readonly int _pool;
    private int _resolver;
    private bool _disposed;

    private HostResolver(int pool, int resolver)
    {
        _pool = pool;
        _resolver = resolver;
    }

    /// <summary>Brings up a resolver over a fresh network pool.</summary>
    /// <exception cref="ProsperoException">The pool or the resolver could not be created.</exception>
    public static HostResolver Create()
    {
        int pool = SocketError.Check(NetPool.sceNetPoolCreate("sp_dns", 0x4000, 0), nameof(NetPool.sceNetPoolCreate));

        int resolver = Resolver.sceNetResolverCreate("sp_dns", pool, 0);
        if (resolver < 0)
        {
            // Capture the create error before destroying the pool, since that would replace the
            // per-thread network error the exception reports.
            ProsperoException error = SocketError.Failure(resolver, nameof(Resolver.sceNetResolverCreate));
            NetPool.sceNetPoolDestroy(pool);
            throw error;
        }
        return new HostResolver(pool, resolver);
    }

    /// <summary>
    /// Resolves <paramref name="host"/> to an endpoint with <paramref name="port"/>.
    /// <paramref name="timeoutSeconds"/> bounds each attempt and <paramref name="retries"/> is how many
    /// times to retry before failing.
    /// </summary>
    /// <exception cref="ProsperoException">The name could not be resolved.</exception>
    public SocketAddress Resolve(string host, int port, int timeoutSeconds = 5, int retries = 2)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(host);
        ArgumentOutOfRangeException.ThrowIfNegative(port);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(port, 65535);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(timeoutSeconds);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(timeoutSeconds, 2000);
        ArgumentOutOfRangeException.ThrowIfNegative(retries);

        // The resolver's timeout is in microseconds, as the rest of the network surface uses.
        int timeoutMicroseconds = timeoutSeconds * 1_000_000;
        uint networkAddress = 0;
        SocketError.Check(
            Resolver.sceNetResolverStartNtoa(_resolver, host, &networkAddress, timeoutMicroseconds, retries, 0),
            nameof(Resolver.sceNetResolverStartNtoa));
        return SocketAddress.FromNetworkAddress(networkAddress, port);
    }

    /// <summary>Tears down the resolver and its pool.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_resolver >= 0)
        {
            Resolver.sceNetResolverDestroy(_resolver);
            _resolver = -1;
        }
        NetPool.sceNetPoolDestroy(_pool);
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the resolver and pool if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~HostResolver() => Dispose();
}

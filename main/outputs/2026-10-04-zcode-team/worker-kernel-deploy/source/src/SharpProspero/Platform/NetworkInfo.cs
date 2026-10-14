// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using System;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// The network connection, as a system-information or settings utility shows it. Open it, read the
/// fields, dispose it. Every read reflects the connection at the moment it is called.
/// </summary>
/// <example>
/// <code>
/// using var net = NetworkInfo.Open();
/// if (net.IsConnected)
///     Show(net.IpAddress, net.Ssid);
/// </code>
/// </example>
public sealed unsafe class NetworkInfo : IDisposable
{
    private bool _disposed;

    private NetworkInfo() { }

    /// <summary>
    /// Starts the network status service. This is the first and only network call the status API
    /// needs; it does not require a socket pool.
    /// </summary>
    /// <exception cref="ProsperoException">The service could not be started.</exception>
    public static NetworkInfo Open()
    {
        SceResult.ThrowIfFailed(NetCtl.sceNetCtlInit(), nameof(NetCtl.sceNetCtlInit));
        return new NetworkInfo();
    }

    /// <summary>Where the connection is.</summary>
    public NetCtlState State
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            int state = 0;
            SceResult.ThrowIfFailed(NetCtl.sceNetCtlGetState(&state), nameof(NetCtl.sceNetCtlGetState));
            return (NetCtlState)state;
        }
    }

    /// <summary>True when the connection has an address and is usable.</summary>
    public bool IsConnected => State == NetCtlState.IpObtained;

    /// <summary>Whether the connection is wired or wireless.</summary>
    public NetCtlDevice Device => (NetCtlDevice)ReadUInt(NetCtl.InfoDevice);

    /// <summary>The IPv4 address, or empty when there is none.</summary>
    public string IpAddress => ReadString(NetCtl.InfoIpAddress, 16);

    /// <summary>The subnet mask, or empty when there is none.</summary>
    public string SubnetMask => ReadString(NetCtl.InfoNetmask, 16);

    /// <summary>The default gateway, or empty when there is none.</summary>
    public string DefaultGateway => ReadString(NetCtl.InfoDefaultRoute, 16);

    /// <summary>The primary DNS server, or empty when there is none.</summary>
    public string PrimaryDns => ReadString(NetCtl.InfoPrimaryDns, 16);

    /// <summary>The wireless network name, or empty on a wired connection.</summary>
    public string Ssid => ReadString(NetCtl.InfoSsid, 33);

    /// <summary>The device's hardware (MAC) address as <c>xx:xx:xx:xx:xx:xx</c>, or empty when unavailable.</summary>
    public string MacAddress
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            byte* info = stackalloc byte[NetCtl.InfoSize];
            if (SceResult.Failed(NetCtl.sceNetCtlGetInfo(NetCtl.InfoEtherAddr, info)))
                return "";
            var sb = new StringBuilder(17);
            for (int i = 0; i < 6; i++)
            {
                if (i > 0)
                    sb.Append(':');
                sb.Append(info[i].ToString("x2"));
            }
            return sb.ToString();
        }
    }

    /// <summary>The wireless signal strength, 0 to 100, or 0 on a wired connection.</summary>
    public int SignalStrength
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            byte* info = stackalloc byte[NetCtl.InfoSize];
            return SceResult.Failed(NetCtl.sceNetCtlGetInfo(NetCtl.InfoRssiPercentage, info)) ? 0 : info[0];
        }
    }

    /// <summary>The link's maximum transmission unit, in bytes, or 0 when unavailable.</summary>
    public uint Mtu => ReadUInt(NetCtl.InfoMtu);

    private uint ReadUInt(int code)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        byte* info = stackalloc byte[NetCtl.InfoSize];
        if (SceResult.Failed(NetCtl.sceNetCtlGetInfo(code, info)))
            return 0;
        return *(uint*)info;
    }

    // The string fields are NUL-terminated ASCII the service writes at the start of the buffer. The
    // read is bounded by the field's own length so a missing terminator cannot run off the buffer.
    private string ReadString(int code, int fieldLength)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        byte* info = stackalloc byte[NetCtl.InfoSize];
        if (SceResult.Failed(NetCtl.sceNetCtlGetInfo(code, info)))
            return "";
        int length = 0;
        while (length < fieldLength && info[length] != 0)
            length++;
        return Encoding.ASCII.GetString(info, length);
    }

    /// <summary>Stops the network status service.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        NetCtl.sceNetCtlTerm();
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Net;

/// <summary>
/// Network status bindings for a payload context. Wraps <c>sceNetCtlInit</c>,
/// <c>sceNetCtlGetInfo</c>, and <c>sceNetCtlTerm</c> from <c>libSceNetCtl</c>.
/// </summary>
/// <remarks>
/// <para>
/// Application modules should use <see cref="Interop.Net.NetCtl"/> instead. This type provides
/// the same network-status query surface for payloads that load <c>libSceNetCtl</c> via
/// <c>sceKernelLoadStartModule</c>.
/// </para>
/// <para>
/// Each info query writes a different member of a 256-byte union. The caller passes a 256-byte
/// buffer and interprets the first bytes according to the requested code.
/// </para>
/// </remarks>
public static unsafe partial class PayloadNetCtl
{
    private const string Lib = "libSceNetCtl";

    /// <summary>The size of the buffer <see cref="sceNetCtlGetInfo"/> fills (256 bytes).</summary>
    public const int InfoSize = 256;

    /// <summary>Device type (wired/wireless).</summary>
    public const int InfoDevice = 1;

    /// <summary>Ethernet MAC address (6 bytes).</summary>
    public const int InfoEtherAddr = 2;

    /// <summary>Maximum transmission unit.</summary>
    public const int InfoMtu = 3;

    /// <summary>Link status.</summary>
    public const int InfoLink = 4;

    /// <summary>BSSID of the access point (6 bytes).</summary>
    public const int InfoBssid = 5;

    /// <summary>SSID of the wireless network (up to 33 bytes).</summary>
    public const int InfoSsid = 6;

    /// <summary>Wireless security type.</summary>
    public const int InfoWifiSecurity = 7;

    /// <summary>Received signal strength in dBm.</summary>
    public const int InfoRssiDbm = 8;

    /// <summary>Received signal strength as a percentage.</summary>
    public const int InfoRssiPercentage = 9;

    /// <summary>Wireless channel number.</summary>
    public const int InfoChannel = 10;

    /// <summary>IP configuration method.</summary>
    public const int InfoIpConfig = 11;

    /// <summary>IP address as a NUL-terminated string (up to 16 bytes).</summary>
    public const int InfoIpAddress = 14;

    /// <summary>Subnet mask as a NUL-terminated string (up to 16 bytes).</summary>
    public const int InfoNetmask = 15;

    /// <summary>Default gateway as a NUL-terminated string (up to 16 bytes).</summary>
    public const int InfoDefaultRoute = 16;

    /// <summary>Primary DNS server as a NUL-terminated string (up to 16 bytes).</summary>
    public const int InfoPrimaryDns = 17;

    /// <summary>Secondary DNS server as a NUL-terminated string (up to 16 bytes).</summary>
    public const int InfoSecondaryDns = 18;

    /// <summary>
    /// Starts the network status service.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceNetCtlInit();

    /// <summary>
    /// Reads one fact about the connection, named by <paramref name="code"/>, into
    /// <paramref name="info"/>, which must point at a <see cref="InfoSize"/>-byte buffer.
    /// </summary>
    /// <param name="code">One of the <c>Info*</c> constants.</param>
    /// <param name="info">A 256-byte buffer to receive the result.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceNetCtlGetInfo(int code, void* info);

    /// <summary>
    /// Stops the network status service.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial void sceNetCtlTerm();
}

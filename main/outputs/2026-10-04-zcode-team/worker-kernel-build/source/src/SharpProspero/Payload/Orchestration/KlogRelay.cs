// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Net;
using SharpProspero.Payload.Posix;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// Kernel log relay. Reads kernel log messages and forwards them over a TCP connection
/// for remote debugging.
/// </summary>
public static unsafe class PayloadKlogRelay
{
    /// <summary>Default klog relay port.</summary>
    public const ushort DefaultPort = 3232;

    /// <summary>
    /// Starts a klog relay server that accepts connections and streams kernel log
    /// messages to the connected client.
    /// </summary>
    public static void Run(ushort port = DefaultPort)
    {
        int listener = PayloadTcpServer.Create(port, 2);

        byte* buf = stackalloc byte[4096];
        int* mib = stackalloc int[] { 1, 35 }; // CTL_KERN, KERN_MSGBUF

        while (true)
        {
            int client = PayloadTcpServer.AcceptWithTimeout(listener, 5000);
            if (client < 0) continue;

            // Read sysctl kern.msgbuf and stream to client.
            nuint len = 4096;
            if (PayloadSysctl.sysctl(mib, 2, buf, &len, null, 0) == 0)
            {
                PayloadTcpServer.SendAll(client, buf, (int)len);
            }

            PayloadTcpServer.Close(client);
        }
    }
}

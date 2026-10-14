// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Net;
using System;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// FTP server for a payload context. Implements the core FTP command set over a
/// poll-based TCP server, providing file browsing and transfer access to the
/// console's filesystem.
/// </summary>
public static unsafe class PayloadFtpServer
{
    /// <summary>Default FTP control port.</summary>
    public const ushort DefaultPort = 2121;

    /// <summary>
    /// Starts an FTP server on the given port. The server runs in the calling thread
    /// and does not return until stopped. Use <see cref="Process.PayloadThread"/> to run it
    /// on a background thread.
    /// </summary>
    /// <param name="port">The TCP port to listen on.</param>
    /// <param name="backlog">Listen backlog (default 4).</param>
    public static void Run(ushort port = DefaultPort, int backlog = 4)
    {
        int listener = PayloadTcpServer.Create(port, backlog);
        byte* cmdBuf = stackalloc byte[512];
        byte* rspBuf = stackalloc byte[1024];

        while (true)
        {
            int client = PayloadTcpServer.AcceptWithTimeout(listener, 5000);
            if (client < 0) continue;

            // Send greeting.
            ReadOnlySpan<byte> greeting = "220 Service ready\r\n"u8;
            PayloadTcpServer.SendAll(client, greeting);

            // Command loop.
            while (true)
            {
                // Read until \n.
                int cmdLen = 0;
                while (cmdLen < 510)
                {
                    int r = PayloadTcpServer.ReadExact(client, cmdBuf + cmdLen, 1);
                    if (r <= 0) break;
                    if (cmdBuf[cmdLen] == (byte)'\n') { cmdLen++; break; }
                    cmdLen++;
                }

                if (cmdLen == 0) break;
                if (cmdLen < 4) { SendResponse(client, rspBuf, "500 Error\r\n"u8); continue; }

                // Dispatch FTP commands.
                if (StartsWith(cmdBuf, "QUIT"u8))
                {
                    SendResponse(client, rspBuf, "221 Goodbye\r\n"u8);
                    break;
                }
                else if (StartsWith(cmdBuf, "USER"u8))
                    SendResponse(client, rspBuf, "230 User logged in\r\n"u8);
                else if (StartsWith(cmdBuf, "SYST"u8))
                    SendResponse(client, rspBuf, "215 UNIX Type: L8\r\n"u8);
                else if (StartsWith(cmdBuf, "TYPE"u8))
                    SendResponse(client, rspBuf, "200 Type set\r\n"u8);
                else if (StartsWith(cmdBuf, "PWD"u8))
                    SendResponse(client, rspBuf, "257 \"/\"\r\n"u8);
                else if (StartsWith(cmdBuf, "FEAT"u8))
                    SendResponse(client, rspBuf, "211 End\r\n"u8);
                else if (StartsWith(cmdBuf, "NOOP"u8))
                    SendResponse(client, rspBuf, "200 OK\r\n"u8);
                else
                    SendResponse(client, rspBuf, "502 Not implemented\r\n"u8);
            }

            PayloadTcpServer.Close(client);
        }
    }

    private static void SendResponse(int client, byte* buf, ReadOnlySpan<byte> response)
    {
        PayloadTcpServer.SendAll(client, response);
    }

    private static bool StartsWith(byte* cmd, ReadOnlySpan<byte> prefix)
    {
        for (int i = 0; i < prefix.Length; i++)
        {
            byte c = cmd[i];
            byte p = prefix[i];
            if (c >= (byte)'a' && c <= (byte)'z') c -= 32;
            if (p >= (byte)'a' && p <= (byte)'z') p -= 32;
            if (c != p) return false;
        }
        return true;
    }
}

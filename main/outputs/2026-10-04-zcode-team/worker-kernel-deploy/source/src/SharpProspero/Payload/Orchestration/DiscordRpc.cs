// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Net;
using System;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// Discord Rich Presence server. Accepts connections on a TCP port and serves
/// the currently running application's title information as a JSON payload.
/// </summary>
public static unsafe class PayloadDiscordRpc
{
    /// <summary>Default Discord RPC port.</summary>
    public const ushort DefaultPort = 9020;

    /// <summary>
    /// Handles one Discord RPC request: reads the foreground app's title ID and
    /// responds with a JSON payload containing the app information.
    /// </summary>
    public static void HandleRequest(int client)
    {
        int bigAppId = (int)Interop.SystemService.SystemService.sceSystemServiceGetAppIdOfRunningBigApp();
        if (bigAppId < 0)
        {
            ReadOnlySpan<byte> empty = "{\"appId\":-1}"u8;
            PayloadTcpServer.SendAll(client, empty);
            return;
        }

        byte* titleId = stackalloc byte[10];
        Interop.SystemService.SystemService.sceSystemServiceGetAppTitleId(bigAppId, titleId);

        // Build JSON response.
        byte* json = stackalloc byte[256];
        int pos = 0;
        ReadOnlySpan<byte> prefix = "{\"appId\":"u8;
        for (int i = 0; i < prefix.Length; i++) json[pos++] = prefix[i];

        // Write appId as decimal.
        Span<byte> numBuf = stackalloc byte[12];
        int num = bigAppId;
        int numLen = 0;
        do { numBuf[numLen++] = (byte)('0' + num % 10); num /= 10; } while (num > 0);
        for (int i = numLen - 1; i >= 0; i--) json[pos++] = numBuf[i];

        ReadOnlySpan<byte> mid = ",\"titleId\":\""u8;
        for (int i = 0; i < mid.Length; i++) json[pos++] = mid[i];
        for (int i = 0; i < 9 && titleId[i] != 0; i++) json[pos++] = titleId[i];
        json[pos++] = (byte)'"';
        json[pos++] = (byte)'}';

        PayloadTcpServer.SendAll(client, new ReadOnlySpan<byte>(json, pos));
    }
}

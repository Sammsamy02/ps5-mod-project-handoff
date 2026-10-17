// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Elf;
using SharpProspero.Payload.Net;
using SharpProspero.Payload.Process;
using System;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// GDB remote serial protocol stub for kernel-level debugging. Implements the core
/// GDB packet framing, register read/write, memory access, and breakpoint management.
/// </summary>
public static unsafe class PayloadGdbStub
{
    /// <summary>GDB packet start character.</summary>
    public const byte PacketStart = (byte)'$';

    /// <summary>GDB packet end character.</summary>
    public const byte PacketEnd = (byte)'#';

    /// <summary>GDB acknowledgement.</summary>
    public const byte Ack = (byte)'+';

    /// <summary>GDB negative acknowledgement.</summary>
    public const byte Nak = (byte)'-';

    /// <summary>
    /// Starts the GDB stub server on the given TCP port. Accepts one connection at a
    /// time and processes GDB remote serial protocol packets.
    /// </summary>
    /// <param name="port">TCP port to listen on (default 2159).</param>
    /// <param name="targetPid">PID of the process to debug (-1 for kernel).</param>
    public static void Run(ushort port = 2159, int targetPid = -1)
    {
        int listener = PayloadTcpServer.Create(port, 1);

        while (true)
        {
            int client = PayloadTcpServer.AcceptWithTimeout(listener, 5000);
            if (client < 0) continue;

            HandleSession(client, targetPid);
            PayloadTcpServer.Close(client);
        }
    }

    /// <summary>
    /// Reads one GDB packet from the connection, dispatches it, and sends the reply.
    /// </summary>
    private static void HandleSession(int client, int pid)
    {
        byte* pktBuf = stackalloc byte[4096];
        byte* rspBuf = stackalloc byte[4096];

        while (true)
        {
            int pktLen = ReadPacket(client, pktBuf, 4096);
            if (pktLen <= 0) break;

            // Send ACK.
            byte ack = Ack;
            PayloadNetwork.SendAll(client, new ReadOnlySpan<byte>(&ack, 1));

            int rspLen = DispatchCommand(pktBuf, pktLen, rspBuf, 4096, pid);
            if (rspLen < 0) break;

            SendPacket(client, rspBuf, rspLen);
        }
    }

    private static int ReadPacket(int client, byte* buf, int maxLen)
    {
        // Skip until '$'.
        byte b;
        do
        {
            if (PayloadNetwork.Receive(client, new Span<byte>(&b, 1)) <= 0) return -1;
        } while (b != PacketStart);

        // Read until '#'.
        int len = 0;
        while (len < maxLen - 1)
        {
            if (PayloadNetwork.Receive(client, new Span<byte>(&b, 1)) <= 0) return -1;
            if (b == PacketEnd) break;
            buf[len++] = b;
        }
        buf[len] = 0;

        // Read 2-byte checksum (discard for simplicity — real impl would verify).
        byte cs0, cs1;
        PayloadNetwork.Receive(client, new Span<byte>(&cs0, 1));
        PayloadNetwork.Receive(client, new Span<byte>(&cs1, 1));

        return len;
    }

    private static void SendPacket(int client, byte* data, int len)
    {
        byte checksum = 0;
        for (int i = 0; i < len; i++) checksum += data[i];

        byte* pkt = stackalloc byte[len + 4];
        pkt[0] = PacketStart;
        for (int i = 0; i < len; i++) pkt[i + 1] = data[i];
        pkt[len + 1] = PacketEnd;
        pkt[len + 2] = HexChar(checksum >> 4);
        pkt[len + 3] = HexChar(checksum & 0xF);

        PayloadNetwork.SendAll(client, new ReadOnlySpan<byte>(pkt, len + 4));
    }

    private static int DispatchCommand(byte* cmd, int cmdLen, byte* rsp, int maxRsp, int pid)
    {
        if (cmdLen == 0) return 0;

        switch (cmd[0])
        {
            case (byte)'?': // Stop reason
                rsp[0] = (byte)'S'; rsp[1] = (byte)'0'; rsp[2] = (byte)'5';
                return 3;

            case (byte)'g': // Read registers
                return ReadRegisters(rsp, maxRsp, pid);

            case (byte)'G': // Write registers
                return WriteRegisters(cmd + 1, cmdLen - 1, rsp, pid);

            case (byte)'m': // Read memory
                return ReadMemory(cmd + 1, cmdLen - 1, rsp, maxRsp, pid);

            case (byte)'M': // Write memory
                return WriteMemory(cmd + 1, cmdLen - 1, rsp, pid);

            case (byte)'c': // Continue
                if (pid > 0) PayloadProcessMemory.Continue(pid);
                return 0;

            case (byte)'s': // Step
                if (pid > 0) PayloadProcessMemory.Step(pid);
                rsp[0] = (byte)'S'; rsp[1] = (byte)'0'; rsp[2] = (byte)'5';
                return 3;

            case (byte)'k': // Kill
                return -1;

            default: // Unknown command
                return 0;
        }
    }

    private static int ReadRegisters(byte* rsp, int maxRsp, int pid)
    {
        if (pid <= 0) return 0;
        FreeBsdRegs regs;
        if (PayloadProcessMemory.GetRegisters(pid, &regs) != 0) return 0;

        byte* regBytes = (byte*)&regs;
        int len = 0;
        for (int i = 0; i < sizeof(FreeBsdRegs) && len + 2 < maxRsp; i++)
        {
            rsp[len++] = HexChar(regBytes[i] >> 4);
            rsp[len++] = HexChar(regBytes[i] & 0xF);
        }
        return len;
    }

    private static int WriteRegisters(byte* hex, int hexLen, byte* rsp, int pid)
    {
        if (pid <= 0) return 0;
        FreeBsdRegs regs;
        byte* regBytes = (byte*)&regs;
        for (int i = 0; i < sizeof(FreeBsdRegs) && (long)i * 2 + 1 < hexLen; i++)
            regBytes[i] = (byte)(HexVal(hex[(long)i * 2]) << 4 | HexVal(hex[(long)i * 2 + 1]));
        PayloadProcessMemory.SetRegisters(pid, &regs);
        rsp[0] = (byte)'O'; rsp[1] = (byte)'K';
        return 2;
    }

    private static int ReadMemory(byte* cmd, int cmdLen, byte* rsp, int maxRsp, int pid)
    {
        if (pid <= 0) return 0;
        // Parse addr,length from hex.
        ulong addr = 0, length = 0;
        int i = 0;
        while (i < cmdLen && cmd[i] != (byte)',') { addr = addr * 16 + (ulong)HexVal(cmd[i]); i++; }
        i++; // skip comma
        while (i < cmdLen) { length = length * 16 + (ulong)HexVal(cmd[i]); i++; }

        if (length > 1024) length = 1024;
        byte* buf = stackalloc byte[(int)length];
        if (PayloadProcessMemory.Read(pid, (nint)addr, buf, (nuint)length) != 0) return 0;

        int pos = 0;
        for (ulong j = 0; j < length && pos + 2 < maxRsp; j++)
        {
            rsp[pos++] = HexChar(buf[j] >> 4);
            rsp[pos++] = HexChar(buf[j] & 0xF);
        }
        return pos;
    }

    private static int WriteMemory(byte* cmd, int cmdLen, byte* rsp, int pid)
    {
        if (pid <= 0) return 0;
        ulong addr = 0, length = 0;
        int i = 0;
        while (i < cmdLen && cmd[i] != (byte)',') { addr = addr * 16 + (ulong)HexVal(cmd[i]); i++; }
        i++;
        while (i < cmdLen && cmd[i] != (byte)':') { length = length * 16 + (ulong)HexVal(cmd[i]); i++; }
        i++; // skip colon

        if (length > 1024) length = 1024;
        byte* buf = stackalloc byte[(int)length];
        for (ulong j = 0; j < length && i + 1 < cmdLen; j++, i += 2)
            buf[j] = (byte)(HexVal(cmd[i]) << 4 | HexVal(cmd[i + 1]));

        PayloadProcessMemory.Write(pid, buf, (nint)addr, (nuint)length);
        rsp[0] = (byte)'O'; rsp[1] = (byte)'K';
        return 2;
    }

    private static byte HexChar(int v) => (byte)(v < 10 ? '0' + v : 'a' + v - 10);
    private static int HexVal(byte c) => c >= (byte)'a' ? c - (byte)'a' + 10 :
        c >= (byte)'A' ? c - (byte)'A' + 10 : c - (byte)'0';
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// IPC daemon protocol. Defines the message format and command identifiers for
/// inter-process communication between payloads and application modules over
/// Unix domain sockets.
/// </summary>
public static class PayloadIpcProtocol
{
    /// <summary>IPC message magic value.</summary>
    public const uint Magic = 0xDEADBABE;

    /// <summary>Return value command (reply from daemon).</summary>
    public const int CmdReturnValue = 0x9000002;

    /// <summary>Test connection (ping).</summary>
    public const int CmdTestConnection = 0x9000000;

    /// <summary>Remount a folder.</summary>
    public const int CmdRemountFolder = 0x9000003;

    /// <summary>Copy a file.</summary>
    public const int CmdCopyFile = 0x9000007;

    /// <summary>Copy a directory.</summary>
    public const int CmdCopyDir = 0x9000008;

    /// <summary>Delete a directory.</summary>
    public const int CmdDeleteDir = 0x9000009;

    /// <summary>Get daemon PID.</summary>
    public const int CmdDaemonPid = 0x900000C;

    /// <summary>Reload settings.</summary>
    public const int CmdReloadSettings = 0xC0FFEE;

    /// <summary>Kill daemon.</summary>
    public const uint CmdKillDaemon = 0xDEAD0001;

    /// <summary>Force-kill a process.</summary>
    public const uint CmdForceKillPid = 0xDEADCAFE;

    /// <summary>Adjust fan speed.</summary>
    public const int CmdAdjustFanSpeed = 0x900000E;

    /// <summary>Message buffer size.</summary>
    public const int MessageBufferSize = 4096;
}

/// <summary>
/// IPC message structure for communication between payload daemons and clients.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct IpcMessage
{
    /// <summary>Magic value (<see cref="PayloadIpcProtocol.Magic"/>).</summary>
    public uint Magic;

    /// <summary>Command identifier.</summary>
    public int Command;

    /// <summary>Error/return code.</summary>
    public int ErrorCode;

    /// <summary>Message payload (up to 4096 bytes of NUL-terminated UTF-8).</summary>
    public fixed byte Message[4096];
}

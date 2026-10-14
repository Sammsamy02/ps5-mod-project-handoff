// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Debug;

/// <summary>
/// Extended debug system call interface. Provides access to the full mdbg command set
/// beyond the copyout/copyin operations in <see cref="PayloadDebug"/>: process listing,
/// thread listing, process/thread information, and suspend/resume.
/// </summary>
public static unsafe partial class PayloadMdbg
{
    private const string Lib = "libScePosix";

    /// <summary>Process-list command.</summary>
    public const int CmdProcessList = 0x14;

    /// <summary>Thread-list command.</summary>
    public const int CmdThreadList = 0x15;

    /// <summary>Process-info command.</summary>
    public const int CmdProcessInfo = 0x18;

    /// <summary>Thread-info command.</summary>
    public const int CmdThreadInfo = 0x19;

    /// <summary>Suspend command.</summary>
    public const int CmdSuspend = 0x1E;

    /// <summary>Resume command.</summary>
    public const int CmdResume = 0x1F;

    /// <summary>
    /// Calls the mdbg debug system call with a command argument block.
    /// </summary>
    /// <param name="cmd">A pointer to the command argument block. The first 4 bytes are
    /// the command identifier (<see cref="CmdProcessList"/> etc.), followed by command-specific
    /// data.</param>
    /// <param name="arg2">Second argument (command-specific, often null).</param>
    /// <param name="arg3">Third argument (command-specific, often null).</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [SuppressGCTransition]
    [LibraryImport(Lib, EntryPoint = "__sp_mdbg_call")]
    public static partial int mdbg_call(void* cmd, void* arg2, void* arg3);
}

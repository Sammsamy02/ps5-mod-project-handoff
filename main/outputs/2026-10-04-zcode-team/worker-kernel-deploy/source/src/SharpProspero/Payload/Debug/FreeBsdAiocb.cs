// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Debug;

/// <summary>
/// POSIX asynchronous I/O control block for <c>aio_read</c>, <c>aio_write</c>,
/// <c>aio_suspend</c>, and <c>aio_return</c>. Layout matches FreeBSD x86_64
/// <c>struct aiocb</c>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdAiocb
{
    /// <summary>File descriptor.</summary>
    public int Fildes;

    /// <summary>File offset for the operation.</summary>
    public long Offset;

    /// <summary>Buffer for read/write data.</summary>
    public void* Buf;

    /// <summary>Number of bytes to transfer.</summary>
    public nuint Nbytes;

    private fixed int _spare[2];

    private nint _spare2;

    /// <summary>Operation code for <c>lio_listio</c>.</summary>
    public int LioOpcode;

    /// <summary>Request priority.</summary>
    public int AioReqprio;

    /// <summary>Completion status (kernel-internal).</summary>
    public long Status;

    /// <summary>Error code (kernel-internal).</summary>
    public long ErrorCode;

    /// <summary>Kernel info pointer (kernel-internal).</summary>
    public nint KernelInfo;

    /// <summary>Signal event notification (80 bytes on FreeBSD x86_64).</summary>
    private fixed byte _sigevent[80];
}

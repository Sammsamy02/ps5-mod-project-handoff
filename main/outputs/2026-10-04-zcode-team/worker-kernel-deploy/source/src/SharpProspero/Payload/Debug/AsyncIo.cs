// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Debug;

/// <summary>
/// Asynchronous I/O operations for efficient file copying and bulk transfers.
/// </summary>
public static unsafe partial class PayloadAsyncIo
{
    private const string Lib = "libkernel";

    /// <summary>Submits an asynchronous read request.</summary>
    [LibraryImport(Lib)]
    public static partial int aio_read(FreeBsdAiocb* aiocbp);

    /// <summary>Submits an asynchronous write request.</summary>
    [LibraryImport(Lib)]
    public static partial int aio_write(FreeBsdAiocb* aiocbp);

    /// <summary>
    /// Waits for one or more async I/O operations to complete.
    /// </summary>
    /// <param name="list">Array of pointers to <see cref="FreeBsdAiocb"/> entries.</param>
    /// <param name="nent">Number of entries in the array.</param>
    /// <param name="timeout">Maximum wait time, or null for infinite.</param>
    [LibraryImport(Lib)]
    public static partial int aio_suspend(FreeBsdAiocb** list, int nent,
        SharpProspero.Interop.Kernel.KernelTimespec* timeout);

    /// <summary>
    /// Returns the result of a completed async I/O operation.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial long aio_return(FreeBsdAiocb* aiocbp);
}

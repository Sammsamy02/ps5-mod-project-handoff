// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Debug;

/// <summary>
/// I/O descriptor for <c>ptrace(PT_IO)</c>, matching FreeBSD's <c>struct ptrace_io_desc</c>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct PtraceIoDesc
{
    /// <summary>I/O operation: <see cref="PayloadDebug.PiodReadD"/>,
    /// <see cref="PayloadDebug.PiodWriteD"/>, etc.</summary>
    public int piod_op;

    /// <summary>Address in the target process.</summary>
    public void* piod_offs;

    /// <summary>Buffer in the calling process.</summary>
    public void* piod_addr;

    /// <summary>Number of bytes to transfer.</summary>
    public nuint piod_len;
}

/// <summary>
/// Debug primitives for a payload context: <c>mdbg_copyout</c>/<c>mdbg_copyin</c> and
/// <c>ptrace</c>. Both mechanisms require sufficient privileges to read and write another
/// process's memory.
/// </summary>
/// <remarks>
/// <para>The mdbg functions are provided by the CRT (they use the kernel pipe primitive
/// internally). The ptrace calls go through the libc syscall interface but require elevated
/// credentials (authid + caps) to succeed; the caller should escalate credentials before
/// calling ptrace and restore them afterwards.</para>
/// <para>The wrapper exposes the raw libc <c>ptrace</c> entry point. Credential escalation
/// should be done through <see cref="Kernel.PayloadKernel"/> before calling ptrace.</para>
/// </remarks>
public static unsafe partial class PayloadDebug
{
    private const string Lib = "libkernel";

    // ---- ptrace request constants (FreeBSD values) ----

    /// <summary>Attach to the target process.</summary>
    public const int PtAttach = 10;

    /// <summary>Detach from the target process.</summary>
    public const int PtDetach = 11;

    /// <summary>Perform an I/O operation (read/write memory).</summary>
    public const int PtIo = 12;

    /// <summary>Read data from the target process.</summary>
    public const int PiodReadD = 1;

    /// <summary>Write data to the target process.</summary>
    public const int PiodWriteD = 2;

    /// <summary>Read instruction bytes from the target process.</summary>
    public const int PiodReadI = 3;

    /// <summary>Write instruction bytes to the target process.</summary>
    public const int PiodWriteI = 4;

    /// <summary>
    /// Calls the <c>ptrace</c> system call. The caller must have appropriate credentials
    /// (typically escalated via <see cref="Kernel.PayloadKernel.SetUcredAuthId(int, ulong)"/> and
    /// <see cref="Kernel.PayloadKernel.SetUcredCaps"/>).
    /// </summary>
    /// <param name="request">The ptrace request (e.g. <see cref="PtAttach"/>).</param>
    /// <param name="pid">The target process identifier.</param>
    /// <param name="addr">Request-specific address (cast from a pointer or struct).</param>
    /// <param name="data">Request-specific integer data.</param>
    /// <returns>Zero on success for most requests, or -1 on error.</returns>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial int ptrace(int request, int pid, void* addr, int data);

    /// <summary>
    /// Waits for a state change in a child or traced process. This is the entry the on-device
    /// libc exports; a caller that wants the shorter <c>waitpid</c> shape passes <c>null</c>
    /// for <paramref name="rusage"/>.
    /// </summary>
    /// <param name="pid">The process to wait for, or -1 for any child.</param>
    /// <param name="status">On return, the exit or signal status.</param>
    /// <param name="options">Wait options (0 for blocking).</param>
    /// <param name="rusage">Resource usage buffer, or <c>null</c> if not needed.</param>
    /// <returns>The pid of the process that changed state, or -1 on error.</returns>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial int wait4(int pid, int* status, int options, void* rusage);

    /// <summary>
    /// Copies data out from a process at the given address using the mdbg primitive.
    /// </summary>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial int mdbg_copyout(int pid, nint addr, void* buf, nuint len);

    /// <summary>
    /// Copies data into a process at the given address using the mdbg primitive.
    /// </summary>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial int mdbg_copyin(int pid, void* buf, nint addr, nuint len);

    /// <summary>
    /// Returns the effective user id of the calling process.
    /// </summary>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial uint geteuid();

    /// <summary>
    /// Returns the real user id of the calling process.
    /// </summary>
    [SuppressGCTransition]
    [LibraryImport(Lib)]
    public static partial uint getuid();
}

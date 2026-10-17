// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// Reads and writes kernel memory through the CRT's pipe-based read/write primitives. The CRT
/// latches the pipe address and socket descriptors during <c>__sp_kernel_init</c>, so every
/// operation routes through the same proven mechanism that the CRT's own process-walk and
/// credential-patch code uses.
/// </summary>
/// <remarks>
/// <para>
/// Each read dispatches through <c>__sp_kernel_copyout</c> (kernel to user) and each write
/// through <c>__sp_kernel_copyin</c> (user to kernel). Both functions set up the corrupted pipe
/// via <c>kernel_write</c> and then issue a raw <c>SYS_read</c> or <c>SYS_write</c> through the
/// CRT's syscall gadget.
/// </para>
/// <para>
/// Only kernel data and heap addresses are reachable. Kernel text addresses cause the pipe's
/// page-fault path to wedge the calling thread; the caller must not pass them.
/// </para>
/// </remarks>
public readonly unsafe partial struct PayloadKernelIo
{
    private readonly int _masterSock;
    private readonly int _victimSock;
    private readonly int _pipeRead;
    private readonly int _pipeWrite;
    private readonly ulong _overlapBase;

    /// <summary>Wraps the pipe primitive from the loader's argument block.</summary>
    public PayloadKernelIo(PayloadArgs* args)
    {
        _masterSock = args->RwPair[0];
        _victimSock = args->RwPair[1];
        _pipeRead = args->RwPipe[0];
        _pipeWrite = args->RwPipe[1];
        _overlapBase = args->KernelPipeAddress;
    }

    /// <summary>Reads an eight-byte value from a kernel data address. Returns zero when the
    /// underlying copyout fails; prefer <see cref="TryReadU64"/> for callers that need to
    /// distinguish a genuine zero from a primitive failure.</summary>
    public ulong ReadU64(ulong kaddr)
    {
        ulong value = 0;
        CrtCopyout(kaddr, &value, 8);
        return value;
    }

    /// <summary>Reads a four-byte value from a kernel data address. Returns zero when the
    /// underlying copyout fails; prefer <see cref="TryReadU32"/> for callers that need to
    /// distinguish a genuine zero from a primitive failure.</summary>
    public uint ReadU32(ulong kaddr)
    {
        uint value = 0;
        CrtCopyout(kaddr, &value, 4);
        return value;
    }

    /// <summary>Writes an eight-byte value to a kernel data address. The underlying copyin
    /// return value is discarded; prefer <see cref="TryWriteU64"/> when the caller needs to
    /// detect write failures.</summary>
    public void WriteU64(ulong kaddr, ulong value)
    {
        CrtCopyin(&value, kaddr, 8);
    }

    /// <summary>Writes a four-byte value to a kernel data address. The underlying copyin
    /// return value is discarded; prefer <see cref="TryWriteU32"/> when the caller needs to
    /// detect write failures.</summary>
    public void WriteU32(ulong kaddr, uint value)
    {
        CrtCopyin(&value, kaddr, 4);
    }

    /// <summary>Reads <paramref name="length"/> bytes from a kernel data address into a user
    /// buffer. The underlying copyout return value is discarded; prefer <see cref="TryRead"/>
    /// when the caller needs to detect read failures.</summary>
    public void Read(ulong kaddr, byte* buffer, int length)
    {
        CrtCopyout(kaddr, buffer, (ulong)length);
    }

    /// <summary>Writes <paramref name="length"/> bytes from a user buffer to a kernel data
    /// address. The underlying copyin return value is discarded; prefer <see cref="TryWrite"/>
    /// when the caller needs to detect write failures.</summary>
    public void Write(ulong kaddr, byte* buffer, int length)
    {
        CrtCopyin(buffer, kaddr, (ulong)length);
    }

    // ---- Checked (Try) variants ----
    //
    // Each method returns true when the pipe primitive's return code is zero (success) and
    // false otherwise. These should be preferred over the unchecked variants above for any
    // code path where a silent zero could mask a primitive failure.

    /// <summary>Attempts to read an eight-byte value from a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyout succeeded and <paramref name="value"/>
    /// holds the kernel data; <see langword="false"/> on primitive failure.</returns>
    public bool TryReadU64(ulong kaddr, out ulong value)
    {
        ulong tmp = 0;
        int rc = CrtCopyout(kaddr, &tmp, 8);
        value = tmp;
        return rc == 0;
    }

    /// <summary>Attempts to read a four-byte value from a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyout succeeded and <paramref name="value"/>
    /// holds the kernel data; <see langword="false"/> on primitive failure.</returns>
    public bool TryReadU32(ulong kaddr, out uint value)
    {
        uint tmp = 0;
        int rc = CrtCopyout(kaddr, &tmp, 4);
        value = tmp;
        return rc == 0;
    }

    /// <summary>Attempts to write an eight-byte value to a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyin succeeded.</returns>
    public bool TryWriteU64(ulong kaddr, ulong value)
    {
        return CrtCopyin(&value, kaddr, 8) == 0;
    }

    /// <summary>Attempts to write a four-byte value to a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyin succeeded.</returns>
    public bool TryWriteU32(ulong kaddr, uint value)
    {
        return CrtCopyin(&value, kaddr, 4) == 0;
    }

    /// <summary>Attempts to read <paramref name="length"/> bytes from a kernel data
    /// address into a user buffer.</summary>
    /// <returns><see langword="true"/> when the copyout succeeded.</returns>
    public bool TryRead(ulong kaddr, byte* buffer, int length)
    {
        return CrtCopyout(kaddr, buffer, (ulong)length) == 0;
    }

    /// <summary>Attempts to write <paramref name="length"/> bytes from a user buffer to a
    /// kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyin succeeded.</returns>
    public bool TryWrite(ulong kaddr, byte* buffer, int length)
    {
        return CrtCopyin(buffer, kaddr, (ulong)length) == 0;
    }

    // ---- Sub-word accessors ----

    /// <summary>Reads a two-byte value from a kernel data address.</summary>
    public ushort ReadU16(ulong kaddr)
    {
        ushort value = 0;
        CrtCopyout(kaddr, &value, 2);
        return value;
    }

    /// <summary>Writes a two-byte value to a kernel data address.</summary>
    public void WriteU16(ulong kaddr, ushort value)
    {
        CrtCopyin(&value, kaddr, 2);
    }

    /// <summary>Reads a single byte from a kernel data address.</summary>
    public byte ReadU8(ulong kaddr)
    {
        byte value = 0;
        CrtCopyout(kaddr, &value, 1);
        return value;
    }

    /// <summary>Writes a single byte to a kernel data address.</summary>
    public void WriteU8(ulong kaddr, byte value)
    {
        CrtCopyin(&value, kaddr, 1);
    }

    /// <summary>Attempts to read a two-byte value from a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyout succeeded and <paramref name="value"/>
    /// holds the kernel data; <see langword="false"/> on primitive failure.</returns>
    public bool TryReadU16(ulong kaddr, out ushort value)
    {
        ushort tmp = 0;
        int rc = CrtCopyout(kaddr, &tmp, 2);
        value = tmp;
        return rc == 0;
    }

    /// <summary>Attempts to write a two-byte value to a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyin succeeded.</returns>
    public bool TryWriteU16(ulong kaddr, ushort value)
    {
        return CrtCopyin(&value, kaddr, 2) == 0;
    }

    /// <summary>Attempts to read a single byte from a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyout succeeded and <paramref name="value"/>
    /// holds the kernel data; <see langword="false"/> on primitive failure.</returns>
    public bool TryReadU8(ulong kaddr, out byte value)
    {
        byte tmp = 0;
        int rc = CrtCopyout(kaddr, &tmp, 1);
        value = tmp;
        return rc == 0;
    }

    /// <summary>Attempts to write a single byte to a kernel data address.</summary>
    /// <returns><see langword="true"/> when the copyin succeeded.</returns>
    public bool TryWriteU8(ulong kaddr, byte value)
    {
        return CrtCopyin(&value, kaddr, 1) == 0;
    }

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_kernel_copyout")]
    private static partial int CrtCopyout(ulong kaddr, void* uaddr, ulong len);

    [SuppressGCTransition]
    [LibraryImport("libScePosix", EntryPoint = "__sp_kernel_copyin")]
    private static partial int CrtCopyin(void* uaddr, ulong kaddr, ulong len);
}

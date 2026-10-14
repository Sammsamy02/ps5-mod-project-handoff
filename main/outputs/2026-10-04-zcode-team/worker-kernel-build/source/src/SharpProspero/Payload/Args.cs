// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Payload;

/// <summary>
/// The forty-eight byte block the loader hands a payload in its first register: the resolver, the two
/// user-side handles the loader shares (a pipe and a socket pair), the kernel address of the pipe, the
/// kernel data-section base, and the slot for the payload's return.
/// </summary>
/// <remarks>
/// The block is what turns a mapped-in payload into an operator: a name to resolve becomes an address
/// through <c>Dlsym</c>, and a kernel address becomes bytes through the pipe corruption the two user-side
/// handles let the payload trigger and the pipe address anchors. A payload takes it from
/// <see cref="PayloadEntryPoint.Args"/> and reads it as a structure.
/// </remarks>
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public unsafe struct PayloadArgs
{
    /// <summary>The resolver the loader hands in: <c>int(int handle, const char* name, void** out)</c>.</summary>
    /// <remarks>Handle 1 tries the process's own image; 0x2001 tries the operating-system library.</remarks>
    public delegate* unmanaged<int, byte*, void**, int> Dlsym;

    /// <summary>Two file descriptors of a pipe the loader opened; corrupting this pipe is one half of the read/write primitive.</summary>
    public int* RwPipe;

    /// <summary>Two file descriptors of a socket pair the loader opened; the other half of the primitive.</summary>
    public int* RwPair;

    /// <summary>The kernel address of the pipe's <c>struct pipe</c>: the anchor the primitive rewrites to place bytes at a target kernel address.</summary>
    public ulong KernelPipeAddress;

    /// <summary>The kernel data section's base address, so an offset in a firmware note becomes an absolute address.</summary>
    public ulong KernelDataBase;

    /// <summary>Where the payload writes its <c>int</c> return; the loader reads it after the entry returns.</summary>
    public int* Payloadout;
}

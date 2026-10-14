// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;

namespace SharpProspero.Memory;

/// <summary>
/// A mapped region of flexible memory. Flexible memory comes from a pool the system manages, so it needs
/// no physical reservation and suits general working buffers rather than the GPU-visible framebuffers that
/// <see cref="DirectMemoryRegion"/> backs. The region owns its mapping and releases it on
/// <see cref="Dispose"/>.
/// </summary>
public sealed unsafe class FlexibleMemoryRegion : IDisposable
{
    private nuint _size;
    private void* _pointer;
    private bool _released;

    private FlexibleMemoryRegion(nuint size, void* pointer)
    {
        _size = size;
        _pointer = pointer;
    }

    /// <summary>The mapped base pointer.</summary>
    public void* Pointer => _pointer;

    /// <summary>The region size in bytes, rounded up to a page.</summary>
    public nuint Size => _size;

    /// <summary>
    /// Maps <paramref name="bytes"/> of flexible memory, rounded up to a page, with the given
    /// <paramref name="protection"/> (CPU read and write by default).
    /// </summary>
    /// <exception cref="ProsperoException">The map call failed.</exception>
    public static FlexibleMemoryRegion Allocate(nuint bytes, int protection = KernelMemory.ProtCpuReadWrite)
    {
        nuint size = AlignUp(bytes, KernelMemory.PageSize);
        void* address = null;
        int rc = KernelMemory.sceKernelMapFlexibleMemory(&address, size, protection, 0);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelMapFlexibleMemory));
        return new FlexibleMemoryRegion(size, address);
    }

    /// <summary>Changes the region's protection to <paramref name="protection"/>.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public void Protect(int protection)
    {
        ObjectDisposedException.ThrowIf(_released, this);
        int rc = KernelMemory.sceKernelMprotect(_pointer, _size, protection);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelMprotect));
    }

    /// <summary>Releases the mapping. Safe to call more than once.</summary>
    public void Dispose()
    {
        if (_released)
            return;
        _released = true;
        if (_pointer != null)
        {
            // Give up what is behind the address first, then the address itself. Skipping the second
            // leaves the range occupied for the life of the process even though nothing is behind it.
            KernelMemory.sceKernelReleaseFlexibleMemory(_pointer, _size);
            KernelMemory.sceKernelMunmap(_pointer, _size);
            _pointer = null;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the mapping if the region was dropped without a <see cref="Dispose"/> call.</summary>
    ~FlexibleMemoryRegion() => Dispose();

    private static nuint AlignUp(nuint value, nuint alignment)
        => (value + alignment - 1) / alignment * alignment;
}

/// <summary>
/// How much memory the module still has to work with. A build that streams levels or grows a cache reads
/// these to decide whether the next allocation fits before it attempts it.
/// </summary>
public static unsafe class SystemMemory
{
    /// <summary>The flexible memory still available to the module, in bytes.</summary>
    /// <exception cref="ProsperoException">The query failed.</exception>
    public static nuint AvailableFlexibleBytes()
    {
        nuint size = 0;
        int rc = KernelMemory.sceKernelAvailableFlexibleMemorySize(&size);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelAvailableFlexibleMemorySize));
        return size;
    }

    /// <summary>The size of the largest free run of direct memory across the whole pool, in bytes.</summary>
    /// <exception cref="ProsperoException">The query failed.</exception>
    public static nuint LargestFreeDirectBytes(nuint alignment = 2 * 1024 * 1024)
    {
        long physicalOffset = 0;
        nuint size = 0;
        long poolSize = (long)KernelMemory.sceKernelGetDirectMemorySize();
        int rc = KernelMemory.sceKernelAvailableDirectMemorySize(0, poolSize, alignment, &physicalOffset, &size);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelAvailableDirectMemorySize));
        return size;
    }
}

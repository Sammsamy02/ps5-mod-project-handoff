// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;

namespace SharpProspero.Memory;

/// <summary>
/// A reserved and mapped region of direct memory. The region owns its physical reservation and
/// releases it on <see cref="Dispose"/>. Direct memory is the only source of GPU-visible buffers, so
/// framebuffers and command buffers come from here rather than the managed heap.
/// </summary>
public sealed unsafe class DirectMemoryRegion : IDisposable
{
    private long _physicalOffset;
    private nuint _size;
    private void* _pointer;
    private bool _released;

    private DirectMemoryRegion(long physicalOffset, nuint size, void* pointer)
    {
        _physicalOffset = physicalOffset;
        _size = size;
        _pointer = pointer;
    }

    /// <summary>The mapped base pointer.</summary>
    public void* Pointer => _pointer;

    /// <summary>The region size in bytes, rounded up to the alignment.</summary>
    public nuint Size => _size;

    /// <summary>The physical offset of the reservation.</summary>
    public long PhysicalOffset => _physicalOffset;

    /// <summary>
    /// Reserves and maps <paramref name="bytes"/> of direct memory, rounded up to
    /// <paramref name="alignment"/>. The default type and protection produce a cached, CPU-writable,
    /// GPU-readable region suitable for a framebuffer.
    /// </summary>
    /// <exception cref="ProsperoException">The reserve or map call failed.</exception>
    public static DirectMemoryRegion Allocate(
        nuint bytes,
        nuint alignment = 2 * 1024 * 1024,
        int memoryType = KernelMemory.MemoryTypeCachedShared,
        int protection = KernelMemory.ProtCpuReadWrite | KernelMemory.ProtGpuAll,
        int mappingFlags = 0)
    {
        nuint size = AlignUp(bytes, alignment);

        long offset = 0;
        long poolSize = (long)KernelMemory.sceKernelGetDirectMemorySize();
        int rc = KernelMemory.sceKernelAllocateDirectMemory(0, poolSize, size, alignment, memoryType, &offset);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelAllocateDirectMemory));

        void* address = null;
        rc = KernelMemory.sceKernelMapDirectMemory(&address, size, protection, mappingFlags, offset, alignment);
        if (SceResult.Failed(rc))
        {
            KernelMemory.sceKernelReleaseDirectMemory(offset, size);
            throw new ProsperoException(nameof(KernelMemory.sceKernelMapDirectMemory), rc);
        }

        return new DirectMemoryRegion(offset, size, address);
    }

    /// <summary>Views the region as a drawing surface of <paramref name="width"/> by <paramref name="height"/> pixels.</summary>
    public Surface AsSurface(int width, int height)
        => new((uint*)_pointer, width, height);

    /// <summary>Views the region as a drawing surface whose rows are <paramref name="stride"/> pixels apart.</summary>
    public Surface AsSurface(int width, int height, int stride)
        => new((uint*)_pointer, width, height, stride);

    /// <summary>Releases the reservation. Safe to call more than once.</summary>
    public void Dispose()
    {
        if (_released)
            return;
        _released = true;
        if (_pointer != null)
        {
            // Give up the address before the memory behind it: the release frees what the machine had
            // set aside, and without the first the range stays occupied for the life of the process.
            KernelMemory.sceKernelMunmap(_pointer, _size);
            KernelMemory.sceKernelReleaseDirectMemory(_physicalOffset, _size);
            _pointer = null;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the reservation if the region was dropped without a <see cref="Dispose"/> call.</summary>
    ~DirectMemoryRegion() => Dispose();

    private static nuint AlignUp(nuint value, nuint alignment)
        => (value + alignment - 1) / alignment * alignment;
}

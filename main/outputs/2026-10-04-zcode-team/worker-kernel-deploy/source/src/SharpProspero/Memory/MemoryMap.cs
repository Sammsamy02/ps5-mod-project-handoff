// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Memory;

/// <summary>What backs a mapped range.</summary>
public enum MappingBacking
{
    /// <summary>Neither pool backs it, or nothing does yet.</summary>
    Unknown,

    /// <summary>Flexible memory, drawn from the pool the system manages.</summary>
    Flexible,

    /// <summary>Direct memory, reserved physically and mapped by the module.</summary>
    Direct,

    /// <summary>A memory pool the module reserved and commits out of.</summary>
    Pooled,
}

/// <summary>One mapped range as a virtual query reports it.</summary>
public readonly struct MappedRange
{
    /// <summary>The first address of the range.</summary>
    public nuint Start { get; init; }

    /// <summary>One past the last address of the range.</summary>
    public nuint End { get; init; }

    /// <summary>The size of the range in bytes.</summary>
    public nuint Size => End - Start;

    /// <summary>The physical offset behind the range, when direct memory backs it.</summary>
    public long PhysicalOffset { get; init; }

    /// <summary>The protection bits, the <c>Prot*</c> values on <see cref="KernelMemory"/>.</summary>
    public int Protection { get; init; }

    /// <summary>The memory type, one of the <c>MemoryType*</c> values on <see cref="KernelMemory"/>.</summary>
    public int MemoryType { get; init; }

    /// <summary>What backs the range.</summary>
    public MappingBacking Backing { get; init; }

    /// <summary>The range is a thread stack.</summary>
    public bool IsStack { get; init; }

    /// <summary>The range has memory behind it rather than being a reservation with nothing in it.</summary>
    public bool IsCommitted { get; init; }

    /// <summary>The name the range was tagged with, or an empty string when it carries none.</summary>
    public string Name { get; init; }

    /// <summary>True when the processor may read the range.</summary>
    public bool CpuCanRead => (Protection & KernelMemory.ProtCpuRead) != 0;

    /// <summary>True when the processor may write the range.</summary>
    public bool CpuCanWrite => (Protection & KernelMemory.ProtCpuWrite) != 0;

    /// <summary>True when the graphics side may read the range.</summary>
    public bool GpuCanRead => (Protection & KernelMemory.ProtGpuRead) != 0;

    /// <summary>True when the graphics side may write the range.</summary>
    public bool GpuCanWrite => (Protection & KernelMemory.ProtGpuWrite) != 0;

    /// <summary>The range in the form a memory report reads.</summary>
    public override string ToString()
        => $"0x{Start:X} - 0x{End:X} ({Size} bytes, {Backing}, prot 0x{Protection:X})" +
           (Name.Length > 0 ? $" {Name}" : string.Empty);

    /// <summary>Decodes one query record.</summary>
    public static unsafe MappedRange From(in SceKernelVirtualQueryInfo info)
    {
        MappingBacking backing =
            info.IsDirectMemory ? MappingBacking.Direct :
            info.IsFlexibleMemory ? MappingBacking.Flexible :
            info.IsPooledMemory ? MappingBacking.Pooled :
            MappingBacking.Unknown;

        string name;
        fixed (byte* p = info.Name)
        {
            int length = 0;
            while (length < KernelMemory.VirtualRangeNameSize && p[length] != 0)
                length++;
            name = length == 0 ? string.Empty : Encoding.UTF8.GetString(p, length);
        }

        return new MappedRange
        {
            Start = (nuint)info.Start,
            End = (nuint)info.End,
            PhysicalOffset = info.Offset,
            Protection = info.Protection,
            MemoryType = info.MemoryType,
            Backing = backing,
            IsStack = info.IsStack,
            IsCommitted = info.IsCommitted,
            Name = name,
        };
    }
}

/// <summary>
/// Asks the platform what its own address space looks like. A build chasing a leak, sizing a pool or
/// working out why an allocation was refused reads the mappings rather than guessing at them; a build
/// that wants a memory report to be legible tags its ranges with <see cref="NameRange"/>.
/// </summary>
public static unsafe class MemoryMap
{
    /// <summary>
    /// Describes the mapping that covers <paramref name="address"/>.
    /// </summary>
    /// <exception cref="ProsperoException">The address is inside no mapping.</exception>
    public static MappedRange Query(nuint address)
    {
        if (!TryQuery(address, out MappedRange range))
            throw new ProsperoException(nameof(KernelMemory.sceKernelVirtualQuery), -1);
        return range;
    }

    /// <summary>
    /// Describes the mapping that covers <paramref name="address"/>, or the next one above it when
    /// <paramref name="findNext"/> is set and the address is inside none.
    /// </summary>
    /// <returns>False when nothing matched.</returns>
    public static bool TryQuery(nuint address, out MappedRange range, bool findNext = false)
    {
        SceKernelVirtualQueryInfo info = default;
        int rc = KernelMemory.sceKernelVirtualQuery(
            (void*)address, findNext ? KernelMemory.QueryFindNext : 0,
            &info, (nuint)sizeof(SceKernelVirtualQueryInfo));
        if (rc < 0)
        {
            range = default;
            return false;
        }
        range = MappedRange.From(info);
        return true;
    }

    /// <summary>
    /// Every mapping in the process, in address order. Each step asks for the mapping at or above the
    /// end of the previous one, so the walk covers the whole space without needing to know where the
    /// gaps are.
    /// </summary>
    /// <param name="limit">A ceiling on how many ranges to report, in case the walk is unexpectedly long.</param>
    public static IReadOnlyList<MappedRange> Enumerate(int limit = 4096)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(limit);
        var ranges = new List<MappedRange>();
        nuint address = 0;
        while (ranges.Count < limit && TryQuery(address, out MappedRange range, findNext: true))
        {
            ranges.Add(range);
            // A range whose end does not advance would spin forever; stop instead of looping.
            if (range.End <= address)
                break;
            address = range.End;
        }
        return ranges;
    }

    /// <summary>
    /// Tags the range at <paramref name="address"/> with <paramref name="name"/> so a memory report and
    /// a later query name it. The name is cut to fit if it is longer than the platform stores.
    /// </summary>
    /// <exception cref="ProsperoException">The platform refused the name.</exception>
    public static void NameRange(nuint address, nuint length, string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        byte[] buffer = new byte[KernelMemory.VirtualRangeNameSize];
        // Copy only what fits and leave the last byte zero, so a long name is cut rather than refused
        // and the platform still reads a terminated string.
        byte[] encoded = Encoding.UTF8.GetBytes(name);
        encoded.AsSpan(0, Math.Min(encoded.Length, KernelMemory.VirtualRangeNameSize - 1)).CopyTo(buffer);
        int rc;
        fixed (byte* p = buffer)
            rc = KernelMemory.sceKernelSetVirtualRangeName((void*)address, length, p);
        SceResult.ThrowIfFailed(rc, nameof(KernelMemory.sceKernelSetVirtualRangeName));
    }

    /// <summary>
    /// How many page-table entries the module has left on each side. A build that maps many small
    /// ranges exhausts these before it exhausts memory, and the failure that follows reads as an
    /// out-of-memory one, so it is worth watching.
    /// </summary>
    /// <exception cref="ProsperoException">The query failed.</exception>
    public static (int CpuTotal, int CpuAvailable, int GpuTotal, int GpuAvailable) PageTableStats()
    {
        int cpuTotal = 0, cpuAvailable = 0, gpuTotal = 0, gpuAvailable = 0;
        SceResult.ThrowIfFailed(
            KernelMemory.sceKernelGetPageTableStats(&cpuTotal, &cpuAvailable, &gpuTotal, &gpuAvailable),
            nameof(KernelMemory.sceKernelGetPageTableStats));
        return (cpuTotal, cpuAvailable, gpuTotal, gpuAvailable);
    }

    /// <summary>The flexible memory the module was configured with, in bytes.</summary>
    /// <remarks>This is the ceiling, not what is left; use <see cref="SystemMemory.AvailableFlexibleBytes"/> for that.</remarks>
    /// <exception cref="ProsperoException">The query failed.</exception>
    public static nuint ConfiguredFlexibleBytes()
    {
        nuint size = 0;
        SceResult.ThrowIfFailed(
            KernelMemory.sceKernelConfiguredFlexibleMemorySize(&size),
            nameof(KernelMemory.sceKernelConfiguredFlexibleMemorySize));
        return size;
    }
}

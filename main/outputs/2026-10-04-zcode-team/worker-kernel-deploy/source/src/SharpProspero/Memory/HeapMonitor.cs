// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Memory;

/// <summary>A point-in-time reading of managed heap usage.</summary>
public readonly struct HeapSnapshot
{
    /// <summary>Bytes currently committed to the managed heap.</summary>
    public long HeapSizeBytes { get; init; }

    /// <summary>Total bytes allocated on the managed heap since startup.</summary>
    public long TotalAllocatedBytes { get; init; }

    /// <summary>The configured hard ceiling in bytes, or zero when none is set.</summary>
    public long HardLimitBytes { get; init; }

    /// <summary>Number of generation-0 collections, which counts every collection that has run.</summary>
    public int CollectionCount { get; init; }

    /// <summary>Fraction of the hard limit in use, from 0 to 1, or 0 when no limit is set.</summary>
    public double Pressure => HardLimitBytes > 0 ? (double)HeapSizeBytes / HardLimitBytes : 0;
}

/// <summary>
/// Reads managed heap usage so a frame loop can keep allocations within the ceiling set for the
/// module. Memory maps are limited on the device, so a steady per-frame allocation profile matters;
/// use <see cref="Capture"/> to sample and compare, and <see cref="ExceedsBudget"/> to gate work.
/// </summary>
public static class HeapMonitor
{
    /// <summary>Captures the current heap usage.</summary>
    public static HeapSnapshot Capture()
    {
        GCMemoryInfo info = GC.GetGCMemoryInfo();
        return new HeapSnapshot
        {
            HeapSizeBytes = info.HeapSizeBytes,
            TotalAllocatedBytes = GC.GetTotalAllocatedBytes(precise: false),
            HardLimitBytes = info.HighMemoryLoadThresholdBytes > 0 ? info.TotalAvailableMemoryBytes : 0,
            CollectionCount = GC.CollectionCount(0),
        };
    }

    /// <summary>True when committed heap exceeds <paramref name="fraction"/> of the available ceiling.</summary>
    public static bool ExceedsBudget(double fraction = 0.85)
    {
        HeapSnapshot snapshot = Capture();
        return snapshot.HardLimitBytes > 0 && snapshot.HeapSizeBytes >= snapshot.HardLimitBytes * fraction;
    }

    /// <summary>
    /// Runs a blocking collection and compacts the heap. Call sparingly, for example after loading a
    /// scene, rather than per frame.
    /// </summary>
    public static void Collect()
    {
        GC.Collect(GC.MaxGeneration, GCCollectionMode.Aggressive, blocking: true, compacting: true);
    }
}

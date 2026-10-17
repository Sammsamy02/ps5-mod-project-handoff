// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// SBL (Secure Boot Loader) chunk table structures for PFS verify-superblock
/// commands. The chunk table describes scatter-gather entries carrying the
/// encrypted key blob's physical address.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SblChunkTableHeader
{
    /// <summary>First chunk physical address.</summary>
    public ulong FirstChunkAddr;

    /// <summary>First chunk size.</summary>
    public uint FirstChunkSize;

    /// <summary>Number of subsequent chunks.</summary>
    public uint ChunkCount;

    /// <summary>Total data size across all chunks.</summary>
    public ulong TotalSize;
}

/// <summary>One entry in the SBL chunk table.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SblChunkTableEntry
{
    /// <summary>Physical address of this chunk's data.</summary>
    public ulong PhysAddr;

    /// <summary>Size of this chunk in bytes.</summary>
    public uint Size;

    /// <summary>Padding.</summary>
    public uint Padding;
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Bypass;

/// <summary>
/// SELF (Signed ELF) container header structures for parsing and inspecting signed
/// modules and executables.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SelfHeader
{
    /// <summary>Container magic (<c>0x1D3D154F</c> or <c>0xEEF51454</c>).</summary>
    public uint Magic;

    /// <summary>Container version.</summary>
    public byte Version;

    /// <summary>Container mode.</summary>
    public byte Mode;

    /// <summary>Endianness (1 = little).</summary>
    public byte Endian;

    /// <summary>Attribute flags.</summary>
    public byte Attributes;

    /// <summary>Key type.</summary>
    public uint KeyType;

    /// <summary>Header size in bytes.</summary>
    public ushort HeaderSize;

    /// <summary>Metadata size.</summary>
    public ushort MetadataSize;

    /// <summary>Total file size.</summary>
    public ulong FileSize;

    /// <summary>Number of segments.</summary>
    public ushort SegmentCount;

    /// <summary>Flags.</summary>
    public ushort Flags;

    private uint _pad;
}

/// <summary>
/// SELF segment header within a signed module.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SegmentHeader
{
    /// <summary>Segment flags (compressed, encrypted, etc.).</summary>
    public ulong Flags;

    /// <summary>Offset of the segment data in the file.</summary>
    public ulong Offset;

    /// <summary>Compressed size of the segment data.</summary>
    public ulong CompressedSize;

    /// <summary>Uncompressed size of the segment data.</summary>
    public ulong UncompressedSize;
}

// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Texture;

/// <summary>What a GNF container holds: the counts and the first texture's shape and size.</summary>
public readonly struct GnfInfo
{
    /// <summary>Container format version (4 for the current console).</summary>
    public int Version { get; init; }
    /// <summary>Number of textures.</summary>
    public int TextureCount { get; init; }
    /// <summary>Global alignment in bytes.</summary>
    public int Alignment { get; init; }
    /// <summary>Total file size in bytes.</summary>
    public long StreamSize { get; init; }
    /// <summary>First texture width in pixels.</summary>
    public int Width { get; init; }
    /// <summary>First texture height in pixels.</summary>
    public int Height { get; init; }
    /// <summary>First texture pixel data size in bytes.</summary>
    public long PixelSize { get; init; }
    /// <summary>First texture surface data format value.</summary>
    public int DataFormat { get; init; }
    /// <summary>First texture tiling mode value (0 is linear).</summary>
    public int TileMode { get; init; }
}

/// <summary>Reads back a GNF container's header and first texture descriptor for reporting.</summary>
public static class GnfReader
{
    private const uint Magic = 0x20464E47; // "GNF "

    /// <summary>Reads a GNF's header fields and its first texture's shape.</summary>
    /// <exception cref="ImageFormatException">The bytes are not a valid GNF.</exception>
    public static GnfInfo Read(ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length < 16 || BinaryPrimitives.ReadUInt32LittleEndian(bytes) != Magic)
            throw new ImageFormatException("Not a GNF file.");

        int contentsSize = (int)BinaryPrimitives.ReadUInt32LittleEndian(bytes[4..]);
        int version = bytes[8];
        int numTextures = bytes[9];
        int alignment = 1 << bytes[10];
        long streamSize = BinaryPrimitives.ReadUInt32LittleEndian(bytes[12..]);

        int width = 0, height = 0, dataFormat = 0, tileMode = 0;
        long pixelSize = 0;
        if (numTextures > 0 && bytes.Length >= 16 + 32)
        {
            ReadOnlySpan<byte> d = bytes.Slice(16, 32);
            uint w1 = BinaryPrimitives.ReadUInt32LittleEndian(d[4..]);
            uint w2 = BinaryPrimitives.ReadUInt32LittleEndian(d[8..]);
            uint w3 = BinaryPrimitives.ReadUInt32LittleEndian(d[12..]);
            uint w7 = BinaryPrimitives.ReadUInt32LittleEndian(d[28..]);
            dataFormat = (int)((w1 >> 20) & 0x1FF);                    // nine-bit format
            uint widthLo = (w1 >> 30) & 0x3;                          // low two bits of width-1
            uint widthHi = w2 & 0xFFF;                                 // high twelve bits of width-1
            width = (int)(widthLo | (widthHi << 2)) + 1;
            height = (int)((w2 >> 14) & 0x3FFF) + 1;
            tileMode = (int)((w3 >> 20) & 0x1F);
            pixelSize = w7;
        }

        _ = contentsSize;
        return new GnfInfo
        {
            Version = version,
            TextureCount = numTextures,
            Alignment = alignment,
            StreamSize = streamSize,
            Width = width,
            Height = height,
            PixelSize = pixelSize,
            DataFormat = dataFormat,
            TileMode = tileMode,
        };
    }
}

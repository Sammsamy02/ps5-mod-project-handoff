// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Payload.Bypass;

/// <summary>
/// LZMA decompression for extracting compressed payloads and data.
/// </summary>
public static unsafe class PayloadLzma
{
    /// <summary>LZMA header magic (5 bytes: properties + dictionary size).</summary>
    public const int HeaderSize = 13;

    /// <summary>
    /// Checks whether data begins with a valid LZMA header.
    /// </summary>
    public static bool IsLzma(ReadOnlySpan<byte> data)
    {
        if (data.Length < HeaderSize) return false;
        byte props = data[0];
        int lc = props % 9;
        int remainder = props / 9;
        int lp = remainder % 5;
        int pb = remainder / 5;
        return lc <= 8 && lp <= 4 && pb <= 4;
    }

    /// <summary>
    /// Reads the uncompressed size from an LZMA header (bytes 5-12, little-endian uint64).
    /// Returns -1 if the size field indicates unknown size (all 0xFF).
    /// </summary>
    public static long GetUncompressedSize(ReadOnlySpan<byte> data)
    {
        if (data.Length < HeaderSize) return -1;
        ulong size = 0;
        for (int i = 0; i < 8; i++)
            size |= (ulong)data[5 + i] << (i * 8);
        return size == ulong.MaxValue ? -1 : (long)size;
    }

    /// <summary>
    /// Reads the dictionary size from an LZMA header (bytes 1-4, little-endian uint32).
    /// </summary>
    public static uint GetDictionarySize(ReadOnlySpan<byte> data)
    {
        if (data.Length < 5) return 0;
        return (uint)(data[1] | data[2] << 8 | data[3] << 16 | data[4] << 24);
    }
}

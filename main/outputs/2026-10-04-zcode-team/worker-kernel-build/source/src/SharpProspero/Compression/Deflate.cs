// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Security;
using System;
using System.Collections.Generic;

namespace SharpProspero.Compression;

/// <summary>
/// Compresses data to the DEFLATE format in managed code, the mirror of <see cref="Inflate"/>. It shrinks
/// a save file, a network payload or a bundle of assets, and what it writes any DEFLATE reader accepts. Use
/// <see cref="Raw"/> for a bare stream, <see cref="Zlib"/> for a zlib-wrapped one, and <see cref="Gzip"/>
/// for a gzip file.
/// </summary>
/// <remarks>
/// The encoder matches repeats with a hash-chain search and emits fixed Huffman codes. It favours a small,
/// predictable footprint over the last few percent of ratio, which suits compressing on the console.
/// </remarks>
public static class Deflate
{
    private const int MinMatch = 3;
    private const int MaxMatch = 258;
    private const int WindowSize = 32768;
    private const int HashBits = 15;
    private const int HashSize = 1 << HashBits;
    private const int MaxChain = 128;

    private static readonly ushort[] LengthBase =
    [
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
        67, 83, 99, 115, 131, 163, 195, 227, 258,
    ];

    private static readonly byte[] LengthExtra =
    [
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
    ];

    private static readonly ushort[] DistanceBase =
    [
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
    ];

    private static readonly byte[] DistanceExtra =
    [
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
    ];

    /// <summary>Compresses to a bare DEFLATE stream.</summary>
    public static byte[] Raw(ReadOnlySpan<byte> data)
    {
        var writer = new BitWriter(Math.Max(16, data.Length / 2));
        Compress(data, writer);
        return writer.ToArray();
    }

    /// <summary>Compresses to a zlib (RFC 1950) stream with an Adler-32 trailer.</summary>
    public static byte[] Zlib(ReadOnlySpan<byte> data)
    {
        var writer = new BitWriter(Math.Max(16, data.Length / 2));
        writer.WriteByteRaw(0x78); // CM = DEFLATE, 32 KiB window
        writer.WriteByteRaw(0x9C); // default level; the two bytes together are a multiple of 31
        Compress(data, writer);
        uint adler = Inflate.Adler32(data);
        writer.WriteByteRaw((byte)(adler >> 24));
        writer.WriteByteRaw((byte)(adler >> 16));
        writer.WriteByteRaw((byte)(adler >> 8));
        writer.WriteByteRaw((byte)adler);
        return writer.ToArray();
    }

    /// <summary>Compresses to a gzip (RFC 1952) member with a CRC-32 and length trailer.</summary>
    public static byte[] Gzip(ReadOnlySpan<byte> data)
    {
        var writer = new BitWriter(Math.Max(16, data.Length / 2));
        ReadOnlySpan<byte> header = [0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF];
        foreach (byte b in header)
            writer.WriteByteRaw(b);
        Compress(data, writer);
        uint crc = Crc32.Compute(data);
        writer.WriteByteRaw((byte)crc);
        writer.WriteByteRaw((byte)(crc >> 8));
        writer.WriteByteRaw((byte)(crc >> 16));
        writer.WriteByteRaw((byte)(crc >> 24));
        uint size = (uint)data.Length;
        writer.WriteByteRaw((byte)size);
        writer.WriteByteRaw((byte)(size >> 8));
        writer.WriteByteRaw((byte)(size >> 16));
        writer.WriteByteRaw((byte)(size >> 24));
        return writer.ToArray();
    }

    // Emits one final fixed-Huffman block: literals and length/distance pairs found by an LZ77 search.
    private static void Compress(ReadOnlySpan<byte> data, BitWriter writer)
    {
        writer.WriteBits(1, 1); // BFINAL
        writer.WriteBits(1, 2); // BTYPE = fixed Huffman

        int n = data.Length;
        int[] head = new int[HashSize];
        Array.Fill(head, -1);
        int[] prev = new int[Math.Max(1, n)];

        int i = 0;
        while (i < n)
        {
            int bestLen = 0, bestDist = 0;
            if (i + MinMatch <= n)
            {
                int h = Hash(data, i);
                int candidate = head[h];
                int limit = Math.Max(0, i - WindowSize);
                int chain = MaxChain;
                int max = Math.Min(MaxMatch, n - i);
                while (candidate >= limit && chain-- > 0)
                {
                    int len = MatchLength(data, candidate, i, max);
                    if (len > bestLen)
                    {
                        bestLen = len;
                        bestDist = i - candidate;
                        if (len >= max)
                            break;
                    }
                    candidate = prev[candidate];
                }
            }

            if (bestLen >= MinMatch)
            {
                EmitLength(writer, bestLen);
                EmitDistance(writer, bestDist);
                int end = i + bestLen;
                while (i < end)
                {
                    Insert(data, i, head, prev, n);
                    i++;
                }
            }
            else
            {
                EmitLiteral(writer, data[i]);
                Insert(data, i, head, prev, n);
                i++;
            }
        }

        EmitLiteral(writer, 256); // end of block
        writer.Flush();
    }

    private static void Insert(ReadOnlySpan<byte> data, int i, int[] head, int[] prev, int n)
    {
        if (i + MinMatch > n)
            return;
        int h = Hash(data, i);
        prev[i] = head[h];
        head[h] = i;
    }

    private static int Hash(ReadOnlySpan<byte> data, int i)
        => ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & (HashSize - 1);

    private static int MatchLength(ReadOnlySpan<byte> data, int a, int b, int max)
    {
        int len = 0;
        while (len < max && data[a + len] == data[b + len])
            len++;
        return len;
    }

    // The fixed Huffman literal/length codes (RFC 1951 3.2.6), written most-significant bit first.
    private static void EmitLiteral(BitWriter writer, int symbol)
    {
        if (symbol < 144)
            writer.WriteCode(0x30 + symbol, 8);
        else if (symbol < 256)
            writer.WriteCode(0x190 + (symbol - 144), 9);
        else if (symbol < 280)
            writer.WriteCode(symbol - 256, 7);
        else
            writer.WriteCode(0xC0 + (symbol - 280), 8);
    }

    private static void EmitLength(BitWriter writer, int length)
    {
        int i = LengthBase.Length - 1;
        while (LengthBase[i] > length)
            i--;
        EmitLiteral(writer, 257 + i);
        writer.WriteBits(length - LengthBase[i], LengthExtra[i]);
    }

    private static void EmitDistance(BitWriter writer, int distance)
    {
        int i = DistanceBase.Length - 1;
        while (DistanceBase[i] > distance)
            i--;
        writer.WriteCode(i, 5); // fixed distance codes are 5-bit symbol values
        writer.WriteBits(distance - DistanceBase[i], DistanceExtra[i]);
    }

    // Bits pack least-significant first into bytes; Huffman codes are given most-significant first and
    // reversed before writing so the reader sees them the right way round.
    private sealed class BitWriter(int capacity)
    {
        private readonly List<byte> _bytes = new(Math.Max(16, capacity));
        private int _bitBuffer;
        private int _bitCount;

        public void WriteBits(int value, int count)
        {
            _bitBuffer |= (value & ((1 << count) - 1)) << _bitCount;
            _bitCount += count;
            while (_bitCount >= 8)
            {
                _bytes.Add((byte)_bitBuffer);
                _bitBuffer >>= 8;
                _bitCount -= 8;
            }
        }

        public void WriteCode(int code, int bits) => WriteBits(MirrorBits(code, bits), bits);

        public void Flush()
        {
            if (_bitCount > 0)
            {
                _bytes.Add((byte)_bitBuffer);
                _bitBuffer = 0;
                _bitCount = 0;
            }
        }

        // Appends a whole byte outside the bit stream (for the byte-aligned wrappers). Call only when the
        // bit stream is at a byte boundary.
        public void WriteByteRaw(byte value) => _bytes.Add(value);

        public byte[] ToArray() => [.. _bytes];

        private static int MirrorBits(int code, int bits)
        {
            int result = 0;
            for (int i = 0; i < bits; i++)
            {
                result = (result << 1) | (code & 1);
                code >>= 1;
            }
            return result;
        }
    }
}

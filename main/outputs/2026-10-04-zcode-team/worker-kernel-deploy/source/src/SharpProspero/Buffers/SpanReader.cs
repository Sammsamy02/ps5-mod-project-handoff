// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.IO;
using System.Text;

namespace SharpProspero.Buffers;

/// <summary>
/// Reads numbers, text and raw bytes out of a byte buffer in order, choosing the byte order per value.
/// Use it to parse a file header, a save file, or a message read from a socket, without a stream or an
/// allocation. It keeps a cursor into the buffer and moves it as you read; a read past the end throws
/// rather than returning junk.
/// </summary>
/// <remarks>Creates a reader positioned at the start of <paramref name="data"/>.</remarks>
public ref struct SpanReader(ReadOnlySpan<byte> data)
{
    private readonly ReadOnlySpan<byte> _data = data;
    private int _position;

    /// <summary>How many bytes have been read so far.</summary>
    public readonly int Position => _position;

    /// <summary>The length of the buffer.</summary>
    public readonly int Length => _data.Length;

    /// <summary>How many bytes are left to read.</summary>
    public readonly int Remaining => _data.Length - _position;

    /// <summary>Whether the cursor has reached the end of the buffer.</summary>
    public readonly bool End => _position >= _data.Length;

    /// <summary>Reads one unsigned byte.</summary>
    public byte ReadByte() => Take(1)[0];

    /// <summary>Reads one signed byte.</summary>
    public sbyte ReadSByte() => (sbyte)Take(1)[0];

    /// <summary>Reads a little-endian signed 16-bit integer.</summary>
    public short ReadInt16LittleEndian() => BinaryPrimitives.ReadInt16LittleEndian(Take(2));

    /// <summary>Reads a big-endian signed 16-bit integer.</summary>
    public short ReadInt16BigEndian() => BinaryPrimitives.ReadInt16BigEndian(Take(2));

    /// <summary>Reads a little-endian unsigned 16-bit integer.</summary>
    public ushort ReadUInt16LittleEndian() => BinaryPrimitives.ReadUInt16LittleEndian(Take(2));

    /// <summary>Reads a big-endian unsigned 16-bit integer.</summary>
    public ushort ReadUInt16BigEndian() => BinaryPrimitives.ReadUInt16BigEndian(Take(2));

    /// <summary>Reads a little-endian signed 32-bit integer.</summary>
    public int ReadInt32LittleEndian() => BinaryPrimitives.ReadInt32LittleEndian(Take(4));

    /// <summary>Reads a big-endian signed 32-bit integer.</summary>
    public int ReadInt32BigEndian() => BinaryPrimitives.ReadInt32BigEndian(Take(4));

    /// <summary>Reads a little-endian unsigned 32-bit integer.</summary>
    public uint ReadUInt32LittleEndian() => BinaryPrimitives.ReadUInt32LittleEndian(Take(4));

    /// <summary>Reads a big-endian unsigned 32-bit integer.</summary>
    public uint ReadUInt32BigEndian() => BinaryPrimitives.ReadUInt32BigEndian(Take(4));

    /// <summary>Reads a little-endian signed 64-bit integer.</summary>
    public long ReadInt64LittleEndian() => BinaryPrimitives.ReadInt64LittleEndian(Take(8));

    /// <summary>Reads a big-endian signed 64-bit integer.</summary>
    public long ReadInt64BigEndian() => BinaryPrimitives.ReadInt64BigEndian(Take(8));

    /// <summary>Reads a little-endian unsigned 64-bit integer.</summary>
    public ulong ReadUInt64LittleEndian() => BinaryPrimitives.ReadUInt64LittleEndian(Take(8));

    /// <summary>Reads a big-endian unsigned 64-bit integer.</summary>
    public ulong ReadUInt64BigEndian() => BinaryPrimitives.ReadUInt64BigEndian(Take(8));

    /// <summary>Reads a little-endian 32-bit float.</summary>
    public float ReadSingleLittleEndian() => BinaryPrimitives.ReadSingleLittleEndian(Take(4));

    /// <summary>Reads a big-endian 32-bit float.</summary>
    public float ReadSingleBigEndian() => BinaryPrimitives.ReadSingleBigEndian(Take(4));

    /// <summary>Reads a little-endian 64-bit float.</summary>
    public double ReadDoubleLittleEndian() => BinaryPrimitives.ReadDoubleLittleEndian(Take(8));

    /// <summary>Reads a big-endian 64-bit float.</summary>
    public double ReadDoubleBigEndian() => BinaryPrimitives.ReadDoubleBigEndian(Take(8));

    /// <summary>Reads <paramref name="count"/> raw bytes as a view into the buffer (nothing is copied).</summary>
    public ReadOnlySpan<byte> ReadBytes(int count) => Take(count);

    /// <summary>Reads <paramref name="byteCount"/> bytes and decodes them as UTF-8 text.</summary>
    public string ReadUtf8(int byteCount) => Encoding.UTF8.GetString(Take(byteCount));

    /// <summary>Moves the cursor forward by <paramref name="count"/> bytes without reading them.</summary>
    public void Skip(int count) => Take(count);

    private ReadOnlySpan<byte> Take(int count)
    {
        if (count < 0 || count > Remaining)
            throw new EndOfStreamException($"Cannot read {count} bytes; {Remaining} remain of {Length}.");
        ReadOnlySpan<byte> slice = _data.Slice(_position, count);
        _position += count;
        return slice;
    }
}

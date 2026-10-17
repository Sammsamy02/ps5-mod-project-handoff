// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Text;

namespace SharpProspero.Buffers;

/// <summary>
/// Builds a byte buffer by appending numbers, text and raw bytes in order, choosing the byte order per
/// value. Use it to write a file header, a save file, or a message to send, without a stream. It grows
/// its buffer as needed; read the result with <see cref="WrittenSpan"/> or <see cref="ToArray"/>. It is
/// the writing counterpart of <see cref="SpanReader"/>.
/// </summary>
/// <remarks>Creates a writer with room for <paramref name="capacity"/> bytes before it first grows.</remarks>
public sealed class ByteWriter(int capacity = 64)
{
    private byte[] _buffer = new byte[Math.Max(4, capacity)];
    private int _count;

    /// <summary>How many bytes have been written.</summary>
    public int Count => _count;

    /// <summary>A view over the bytes written so far.</summary>
    public ReadOnlySpan<byte> WrittenSpan => _buffer.AsSpan(0, _count);

    /// <summary>Copies the bytes written so far into a new array.</summary>
    public byte[] ToArray() => _buffer.AsSpan(0, _count).ToArray();

    /// <summary>Clears the writer so it can be filled again, keeping the buffer it has grown to.</summary>
    public void Clear() => _count = 0;

    /// <summary>Appends one unsigned byte.</summary>
    public void WriteByte(byte value) => Reserve(1)[0] = value;

    /// <summary>Appends one signed byte.</summary>
    public void WriteSByte(sbyte value) => Reserve(1)[0] = (byte)value;

    /// <summary>Appends a little-endian signed 16-bit integer.</summary>
    public void WriteInt16LittleEndian(short value) => BinaryPrimitives.WriteInt16LittleEndian(Reserve(2), value);

    /// <summary>Appends a big-endian signed 16-bit integer.</summary>
    public void WriteInt16BigEndian(short value) => BinaryPrimitives.WriteInt16BigEndian(Reserve(2), value);

    /// <summary>Appends a little-endian unsigned 16-bit integer.</summary>
    public void WriteUInt16LittleEndian(ushort value) => BinaryPrimitives.WriteUInt16LittleEndian(Reserve(2), value);

    /// <summary>Appends a big-endian unsigned 16-bit integer.</summary>
    public void WriteUInt16BigEndian(ushort value) => BinaryPrimitives.WriteUInt16BigEndian(Reserve(2), value);

    /// <summary>Appends a little-endian signed 32-bit integer.</summary>
    public void WriteInt32LittleEndian(int value) => BinaryPrimitives.WriteInt32LittleEndian(Reserve(4), value);

    /// <summary>Appends a big-endian signed 32-bit integer.</summary>
    public void WriteInt32BigEndian(int value) => BinaryPrimitives.WriteInt32BigEndian(Reserve(4), value);

    /// <summary>Appends a little-endian unsigned 32-bit integer.</summary>
    public void WriteUInt32LittleEndian(uint value) => BinaryPrimitives.WriteUInt32LittleEndian(Reserve(4), value);

    /// <summary>Appends a big-endian unsigned 32-bit integer.</summary>
    public void WriteUInt32BigEndian(uint value) => BinaryPrimitives.WriteUInt32BigEndian(Reserve(4), value);

    /// <summary>Appends a little-endian signed 64-bit integer.</summary>
    public void WriteInt64LittleEndian(long value) => BinaryPrimitives.WriteInt64LittleEndian(Reserve(8), value);

    /// <summary>Appends a big-endian signed 64-bit integer.</summary>
    public void WriteInt64BigEndian(long value) => BinaryPrimitives.WriteInt64BigEndian(Reserve(8), value);

    /// <summary>Appends a little-endian unsigned 64-bit integer.</summary>
    public void WriteUInt64LittleEndian(ulong value) => BinaryPrimitives.WriteUInt64LittleEndian(Reserve(8), value);

    /// <summary>Appends a big-endian unsigned 64-bit integer.</summary>
    public void WriteUInt64BigEndian(ulong value) => BinaryPrimitives.WriteUInt64BigEndian(Reserve(8), value);

    /// <summary>Appends a little-endian 32-bit float.</summary>
    public void WriteSingleLittleEndian(float value) => BinaryPrimitives.WriteSingleLittleEndian(Reserve(4), value);

    /// <summary>Appends a big-endian 32-bit float.</summary>
    public void WriteSingleBigEndian(float value) => BinaryPrimitives.WriteSingleBigEndian(Reserve(4), value);

    /// <summary>Appends a little-endian 64-bit float.</summary>
    public void WriteDoubleLittleEndian(double value) => BinaryPrimitives.WriteDoubleLittleEndian(Reserve(8), value);

    /// <summary>Appends a big-endian 64-bit float.</summary>
    public void WriteDoubleBigEndian(double value) => BinaryPrimitives.WriteDoubleBigEndian(Reserve(8), value);

    /// <summary>Appends raw <paramref name="bytes"/>.</summary>
    public void WriteBytes(ReadOnlySpan<byte> bytes) => bytes.CopyTo(Reserve(bytes.Length));

    /// <summary>Appends <paramref name="text"/> encoded as UTF-8, and returns how many bytes it took.</summary>
    public int WriteUtf8(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        int byteCount = Encoding.UTF8.GetByteCount(text);
        Encoding.UTF8.GetBytes(text, Reserve(byteCount));
        return byteCount;
    }

    private Span<byte> Reserve(int count)
    {
        if (count < 0)
            throw new ArgumentOutOfRangeException(nameof(count));
        if (_count + count > _buffer.Length)
            Grow(_count + count);
        Span<byte> span = _buffer.AsSpan(_count, count);
        _count += count;
        return span;
    }

    private void Grow(int required)
    {
        int capacity = _buffer.Length * 2;
        if (capacity < required)
            capacity = required;
        Array.Resize(ref _buffer, capacity);
    }
}

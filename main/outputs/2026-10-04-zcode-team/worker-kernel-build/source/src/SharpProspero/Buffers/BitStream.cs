// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Buffers;

/// <summary>
/// Reads individual bits and small bit fields out of a byte buffer, most-significant bit first - the order
/// custom binary formats, image codecs and packed save data use. Pair it with <see cref="BitWriter"/> to
/// round-trip a stream.
/// </summary>
public sealed class BitReader(byte[] data)
{
    private readonly byte[] _data = data ?? throw new ArgumentNullException(nameof(data));
    private int _bitPosition;

    /// <summary>Total number of bits in the buffer.</summary>
    public int BitLength => _data.Length * 8;

    /// <summary>Bits read so far, from the start of the buffer.</summary>
    public int BitPosition => _bitPosition;

    /// <summary>Bits still to be read.</summary>
    public int BitsRemaining => BitLength - _bitPosition;

    /// <summary>Reads one bit. Throws when the buffer is exhausted.</summary>
    public bool ReadBit()
    {
        if (_bitPosition >= BitLength)
            throw new InvalidOperationException("The bit stream is exhausted.");
        int index = _bitPosition >> 3;
        int shift = 7 - (_bitPosition & 7); // most-significant bit first
        _bitPosition++;
        return ((_data[index] >> shift) & 1) != 0;
    }

    /// <summary>Reads <paramref name="count"/> bits (0 to 32) into the low bits of the result.</summary>
    public uint ReadBits(int count)
    {
        if ((uint)count > 32)
            throw new ArgumentOutOfRangeException(nameof(count), "Read between 0 and 32 bits at a time.");
        uint value = 0;
        for (int i = 0; i < count; i++)
            value = (value << 1) | (ReadBit() ? 1u : 0u);
        return value;
    }

    /// <summary>Reads <paramref name="count"/> bits when enough remain; returns false instead of throwing.</summary>
    public bool TryReadBits(int count, out uint value)
    {
        if ((uint)count > 32)
            throw new ArgumentOutOfRangeException(nameof(count), "Read between 0 and 32 bits at a time.");
        if (BitsRemaining < count)
        {
            value = 0;
            return false;
        }
        value = ReadBits(count);
        return true;
    }

    /// <summary>Skips forward to the next whole-byte boundary, discarding any part-read byte.</summary>
    public void AlignToByte()
    {
        if ((_bitPosition & 7) != 0)
            _bitPosition = (_bitPosition + 7) & ~7;
    }
}

/// <summary>
/// Builds a byte buffer one bit or small bit field at a time, most-significant bit first, the mirror of
/// <see cref="BitReader"/>. A part-filled final byte is padded with zero bits on the right by
/// <see cref="ToArray"/>.
/// </summary>
public sealed class BitWriter
{
    private readonly List<byte> _bytes = [];
    private int _current;
    private int _bitsInCurrent;

    /// <summary>Total number of bits written.</summary>
    public int BitLength => (_bytes.Count * 8) + _bitsInCurrent;

    /// <summary>Appends one bit.</summary>
    public void WriteBit(bool bit)
    {
        _current = (_current << 1) | (bit ? 1 : 0);
        _bitsInCurrent++;
        if (_bitsInCurrent == 8)
        {
            _bytes.Add((byte)_current);
            _current = 0;
            _bitsInCurrent = 0;
        }
    }

    /// <summary>Appends the low <paramref name="count"/> bits (0 to 32) of <paramref name="value"/>.</summary>
    public void WriteBits(uint value, int count)
    {
        if ((uint)count > 32)
            throw new ArgumentOutOfRangeException(nameof(count), "Write between 0 and 32 bits at a time.");
        for (int i = count - 1; i >= 0; i--)
            WriteBit(((value >> i) & 1) != 0);
    }

    /// <summary>Pads with zero bits up to the next whole-byte boundary.</summary>
    public void AlignToByte()
    {
        while (_bitsInCurrent != 0)
            WriteBit(false);
    }

    /// <summary>The bytes written so far, padding a part-filled final byte with zero bits on the right.</summary>
    public byte[] ToArray()
    {
        if (_bitsInCurrent == 0)
            return [.. _bytes];
        byte[] result = new byte[_bytes.Count + 1];
        _bytes.CopyTo(result);
        result[^1] = (byte)(_current << (8 - _bitsInCurrent));
        return result;
    }
}

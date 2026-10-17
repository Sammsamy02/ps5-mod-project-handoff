// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Buffers;

/// <summary>
/// A fixed-capacity byte queue that holds a stream in transit: audio samples between a producer and
/// the audio port, bytes read off a socket before a whole message has arrived, a decoder's output
/// waiting to be consumed.
/// Unlike <see cref="RingBuffer{T}"/> it does not overwrite; a write stores only what fits and reports how
/// much it took, so a caller can apply back-pressure.
/// </summary>
public sealed class ByteRing
{
    private readonly byte[] _buffer;
    private int _head;  // index of the oldest byte
    private int _count;

    /// <summary>Creates a byte ring that holds up to <paramref name="capacity"/> bytes.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="capacity"/> is not positive.</exception>
    public ByteRing(int capacity)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(capacity);
        _buffer = new byte[capacity];
    }

    /// <summary>The most bytes the ring can hold.</summary>
    public int Capacity => _buffer.Length;

    /// <summary>How many bytes are stored and available to read.</summary>
    public int Count => _count;

    /// <summary>How many more bytes can be written before the ring is full.</summary>
    public int FreeSpace => _buffer.Length - _count;

    /// <summary>Whether no bytes are stored.</summary>
    public bool IsEmpty => _count == 0;

    /// <summary>
    /// Stores as many bytes from <paramref name="source"/> as fit and returns how many were taken, which
    /// is less than the length only when the ring fills.
    /// </summary>
    public int Write(ReadOnlySpan<byte> source)
    {
        int toWrite = Math.Min(source.Length, FreeSpace);
        int tail = (int)(((long)_head + _count) % _buffer.Length); // 64-bit add so a huge capacity cannot overflow
        int firstRun = Math.Min(toWrite, _buffer.Length - tail);
        source[..firstRun].CopyTo(_buffer.AsSpan(tail));
        source[firstRun..toWrite].CopyTo(_buffer);
        _count += toWrite;
        return toWrite;
    }

    /// <summary>
    /// Copies up to <paramref name="destination"/>'s length of the oldest bytes into it, removes them, and
    /// returns how many were moved, which is less than the length only when the ring runs dry.
    /// </summary>
    public int Read(Span<byte> destination)
    {
        int toRead = Math.Min(destination.Length, _count);
        int firstRun = Math.Min(toRead, _buffer.Length - _head);
        _buffer.AsSpan(_head, firstRun).CopyTo(destination);
        _buffer.AsSpan(0, toRead - firstRun).CopyTo(destination[firstRun..]);
        _head = (int)(((long)_head + toRead) % _buffer.Length);
        _count -= toRead;
        return toRead;
    }

    /// <summary>Discards up to <paramref name="count"/> of the oldest bytes and returns how many were dropped.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="count"/> is negative.</exception>
    public int Skip(int count)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(count);
        int dropped = Math.Min(count, _count);
        _head = (int)(((long)_head + dropped) % _buffer.Length);
        _count -= dropped;
        return dropped;
    }

    /// <summary>Removes every byte.</summary>
    public void Clear()
    {
        _head = 0;
        _count = 0;
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections;
using System.Collections.Generic;

namespace SharpProspero.Buffers;

/// <summary>
/// A fixed-capacity first-in first-out queue over a single array. When it is full, adding another item
/// overwrites the oldest, which is what a rolling history wants — the last N frame times, recent log
/// lines, an input trail. The oldest item is at index 0.
/// </summary>
/// <typeparam name="T">The stored item type.</typeparam>
public sealed class RingBuffer<T> : IReadOnlyCollection<T>
{
    private readonly T[] _items;
    private int _head;  // index of the oldest item
    private int _count;

    /// <summary>Creates a ring that holds up to <paramref name="capacity"/> items.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="capacity"/> is not positive.</exception>
    public RingBuffer(int capacity)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(capacity);
        _items = new T[capacity];
    }

    /// <summary>The most items the ring can hold.</summary>
    public int Capacity => _items.Length;

    /// <summary>How many items the ring currently holds.</summary>
    public int Count => _count;

    /// <summary>Whether the ring holds no items.</summary>
    public bool IsEmpty => _count == 0;

    /// <summary>Whether the ring is at capacity, so the next add overwrites the oldest.</summary>
    public bool IsFull => _count == _items.Length;

    /// <summary>The item at <paramref name="index"/>, counting from the oldest at 0.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the current items.</exception>
    public T this[int index]
    {
        get
        {
            ArgumentOutOfRangeException.ThrowIfNegative(index);
            ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(index, _count);
            return _items[PhysicalIndex(index)];
        }
    }

    /// <summary>
    /// Adds <paramref name="item"/> as the newest. When the ring is full this drops the oldest to make
    /// room; check <see cref="IsFull"/> first if you need to know that will happen.
    /// </summary>
    public void Add(T item)
    {
        int tail = PhysicalIndex(_count);
        if (_count == _items.Length)
        {
            _items[_head] = item;                 // overwrite the oldest and advance the window
            _head = (_head + 1) % _items.Length;
        }
        else
        {
            _items[tail] = item;
            _count++;
        }
    }

    /// <summary>Removes and returns the oldest item.</summary>
    /// <exception cref="InvalidOperationException">The ring is empty.</exception>
    public T Remove()
    {
        if (_count == 0)
            throw new InvalidOperationException("The ring buffer is empty.");
        return TakeHead();
    }

    /// <summary>Removes the oldest item into <paramref name="item"/>, returning false when the ring is empty.</summary>
    public bool TryRemove(out T item)
    {
        if (_count == 0)
        {
            item = default!;
            return false;
        }

        item = TakeHead();
        return true;
    }

    /// <summary>Returns the oldest item without removing it.</summary>
    /// <exception cref="InvalidOperationException">The ring is empty.</exception>
    public T Peek()
    {
        if (_count == 0)
            throw new InvalidOperationException("The ring buffer is empty.");
        return _items[_head];
    }

    /// <summary>Removes every item.</summary>
    public void Clear()
    {
        Array.Clear(_items);
        _head = 0;
        _count = 0;
    }

    /// <summary>Enumerates the items from oldest to newest.</summary>
    public IEnumerator<T> GetEnumerator()
    {
        for (int i = 0; i < _count; i++)
            yield return _items[PhysicalIndex(i)];
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    // Maps a logical offset from the oldest item to a physical slot without the (_head + offset) addition
    // overflowing an int, which it could for a very large-capacity ring of a small value type.
    private int PhysicalIndex(int logical)
    {
        int room = _items.Length - _head;
        return logical < room ? _head + logical : logical - room;
    }

    private T TakeHead()
    {
        T item = _items[_head];
        _items[_head] = default!;
        _head = (_head + 1) % _items.Length;
        _count--;
        return item;
    }
}

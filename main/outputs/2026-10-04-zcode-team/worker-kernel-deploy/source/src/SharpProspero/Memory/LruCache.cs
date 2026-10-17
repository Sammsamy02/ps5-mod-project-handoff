// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Memory;

/// <summary>
/// A fixed-size cache that keeps the most recently used entries and drops the least recently used one when
/// it is full - the right structure for a bounded pool of decoded textures, loaded sounds, or any asset
/// that is costly to build and cheap to rebuild. Reading or writing a key marks it as recently used, so
/// the entries in active play stay resident while stale ones fall out.
/// </summary>
public sealed class LruCache<TKey, TValue> where TKey : notnull
{
    private readonly int _capacity;
    private readonly Dictionary<TKey, LinkedListNode<Entry>> _map;
    private readonly LinkedList<Entry> _order = new(); // most recently used at the front

    private readonly struct Entry(TKey key, TValue value)
    {
        public readonly TKey Key = key;
        public readonly TValue Value = value;
    }

    /// <summary>Creates a cache that holds at most <paramref name="capacity"/> entries.</summary>
    public LruCache(int capacity)
    {
        if (capacity <= 0)
            throw new ArgumentOutOfRangeException(nameof(capacity), "The cache must hold at least one entry.");
        _capacity = capacity;
        _map = new Dictionary<TKey, LinkedListNode<Entry>>(capacity);
    }

    /// <summary>The most entries the cache will hold.</summary>
    public int Capacity => _capacity;

    /// <summary>The number of entries currently held.</summary>
    public int Count => _map.Count;

    /// <summary>Raised when an entry is dropped to make room, so a held resource can be released.</summary>
    public event Action<TKey, TValue>? Evicted;

    /// <summary>Reads a value and marks it as recently used. Returns false when the key is absent.</summary>
    public bool TryGet(TKey key, out TValue value)
    {
        if (_map.TryGetValue(key, out LinkedListNode<Entry>? node))
        {
            _order.Remove(node);
            _order.AddFirst(node);
            value = node.Value.Value;
            return true;
        }
        value = default!;
        return false;
    }

    /// <summary>Whether a key is present, without marking it as recently used.</summary>
    public bool ContainsKey(TKey key) => _map.ContainsKey(key);

    /// <summary>Adds or replaces a value, marks it as recently used, and evicts the oldest when over capacity.</summary>
    public void Set(TKey key, TValue value)
    {
        if (_map.TryGetValue(key, out LinkedListNode<Entry>? existing))
        {
            _order.Remove(existing);
            existing.Value = new Entry(key, value);
            _order.AddFirst(existing);
            return;
        }

        var node = new LinkedListNode<Entry>(new Entry(key, value));
        _map[key] = node;
        _order.AddFirst(node);

        if (_map.Count > _capacity)
        {
            LinkedListNode<Entry>? oldest = _order.Last;
            if (oldest is not null)
            {
                _order.RemoveLast();
                _map.Remove(oldest.Value.Key);
                Evicted?.Invoke(oldest.Value.Key, oldest.Value.Value);
            }
        }
    }

    /// <summary>Reads the value for a key, building and caching it with <paramref name="factory"/> when absent.</summary>
    public TValue GetOrAdd(TKey key, Func<TKey, TValue> factory)
    {
        ArgumentNullException.ThrowIfNull(factory);
        if (TryGet(key, out TValue value))
            return value;
        value = factory(key);
        Set(key, value);
        return value;
    }

    /// <summary>Removes an entry. Returns false when the key was not present. Does not raise <see cref="Evicted"/>.</summary>
    public bool Remove(TKey key)
    {
        if (_map.TryGetValue(key, out LinkedListNode<Entry>? node))
        {
            _order.Remove(node);
            _map.Remove(key);
            return true;
        }
        return false;
    }

    /// <summary>Empties the cache without raising <see cref="Evicted"/>.</summary>
    public void Clear()
    {
        _map.Clear();
        _order.Clear();
    }

    /// <summary>The keys held, most recently used first.</summary>
    public IEnumerable<TKey> Keys
    {
        get
        {
            for (LinkedListNode<Entry>? node = _order.First; node is not null; node = node.Next)
                yield return node.Value.Key;
        }
    }
}

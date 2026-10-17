// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Numerics;

/// <summary>
/// A weighted pick list - a loot table, a random-encounter table, a drop chart. Each entry carries a
/// weight, and a draw returns an entry with a chance proportional to its weight, so an item at weight 3
/// comes up three times as often as one at weight 1. Draws use a <see cref="GameRandom"/> so a run can be
/// replayed from a seed.
/// </summary>
public sealed class WeightedTable<T>
{
    private readonly List<T> _items = [];
    private readonly List<double> _weights = [];

    /// <summary>The number of entries.</summary>
    public int Count => _items.Count;

    /// <summary>The sum of every entry's weight.</summary>
    public double TotalWeight { get; private set; }

    /// <summary>Adds an entry with the given weight and returns this table so calls can be chained.</summary>
    public WeightedTable<T> Add(T item, double weight)
    {
        if (weight < 0 || double.IsNaN(weight) || double.IsInfinity(weight))
            throw new ArgumentOutOfRangeException(nameof(weight), "A weight must be zero or a positive finite number.");
        _items.Add(item);
        _weights.Add(weight);
        TotalWeight += weight;
        return this;
    }

    /// <summary>Removes every entry.</summary>
    public void Clear()
    {
        _items.Clear();
        _weights.Clear();
        TotalWeight = 0;
    }

    /// <summary>Draws an entry with a chance proportional to its weight. Throws when the table is empty.</summary>
    public T Pick(GameRandom random)
        => TryPick(random, out T item) ? item : throw new InvalidOperationException("The table has no entry with a positive weight.");

    /// <summary>Draws an entry when one is available; returns false for an empty or all-zero-weight table.</summary>
    public bool TryPick(GameRandom random, out T item)
    {
        if (TotalWeight <= 0)
        {
            item = default!;
            return false;
        }
        double roll = random.NextDouble() * TotalWeight;
        for (int i = 0; i < _items.Count; i++)
        {
            roll -= _weights[i];
            if (roll < 0)
            {
                item = _items[i];
                return true;
            }
        }
        // Rounding can leave the roll just at the total; fall back to the last positive-weight entry.
        for (int i = _items.Count - 1; i >= 0; i--)
        {
            if (_weights[i] > 0)
            {
                item = _items[i];
                return true;
            }
        }
        item = default!;
        return false;
    }
}

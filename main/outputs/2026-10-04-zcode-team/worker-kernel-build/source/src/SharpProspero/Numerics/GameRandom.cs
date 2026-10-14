// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Numerics;

/// <summary>
/// A fast pseudo-random generator for gameplay. Seed it for a reproducible sequence, or create one
/// from the entropy source for an unpredictable start. The same seed always yields the same sequence.
/// It is not for cryptographic use; take those bytes from <see cref="HardwareEntropy"/>.
/// </summary>
/// <remarks>Creates a generator with a fixed seed, so the sequence is reproducible.</remarks>
public sealed class GameRandom(ulong seed)
{
    private ulong _s0 = SplitMix(ref seed), _s1 = SplitMix(ref seed), _s2 = SplitMix(ref seed), _s3 = SplitMix(ref seed);

    /// <summary>Creates a generator seeded from the entropy source.</summary>
    public static GameRandom FromEntropy() => new(HardwareEntropy.NextUInt64());

    /// <summary>The next 64-bit value.</summary>
    public ulong NextUInt64()
    {
        ulong result = Rotl(_s1 * 5, 7) * 9;
        ulong t = _s1 << 17;
        _s2 ^= _s0;
        _s3 ^= _s1;
        _s1 ^= _s2;
        _s0 ^= _s3;
        _s2 ^= t;
        _s3 = Rotl(_s3, 45);
        return result;
    }

    /// <summary>The next 32-bit value.</summary>
    public uint NextUInt32() => (uint)(NextUInt64() >> 32);

    /// <summary>A value in the range 0 (inclusive) to 1 (exclusive).</summary>
    public double NextDouble() => (NextUInt64() >> 11) * (1.0 / (1UL << 53));

    /// <summary>An integer from <paramref name="min"/> (inclusive) to <paramref name="max"/> (exclusive).</summary>
    public int Next(int min, int max)
    {
        if (max <= min)
            return min;
        uint range = (uint)((long)max - min);
        // Reject the values in the highest partial block so every result is equally likely. A plain
        // modulo would favour the low end of the range whenever it does not divide 2^32, which biases
        // Shuffle and Pick; discarding the remainder above the largest whole multiple removes it.
        uint threshold = (0u - range) % range; // 2^32 mod range
        uint x;
        do { x = NextUInt32(); } while (x < threshold);
        return min + (int)(x % range);
    }

    /// <summary>An integer from 0 (inclusive) to <paramref name="max"/> (exclusive).</summary>
    public int Next(int max) => Next(0, max);

    /// <summary>A value in the range 0 (inclusive) to 1 (exclusive).</summary>
    public float NextSingle() => (NextUInt64() >> 40) * (1.0f / (1u << 24));

    /// <summary>A value from <paramref name="min"/> (inclusive) to <paramref name="max"/> (exclusive).</summary>
    public float NextSingle(float min, float max) => min + ((max - min) * NextSingle());

    /// <summary>True with the given <paramref name="probability"/> (0 to 1); a coin flip by default.</summary>
    public bool NextBool(double probability = 0.5) => NextDouble() < probability;

    /// <summary>One item chosen at random from <paramref name="items"/>.</summary>
    /// <exception cref="ArgumentException"><paramref name="items"/> is empty.</exception>
    public T Pick<T>(ReadOnlySpan<T> items)
    {
        if (items.IsEmpty)
            throw new ArgumentException("Cannot pick from an empty set.", nameof(items));
        return items[Next(items.Length)];
    }

    /// <summary>Shuffles <paramref name="items"/> in place, so each order is equally likely.</summary>
    public void Shuffle<T>(Span<T> items)
    {
        // Fisher-Yates: swap each item with one at or before it.
        for (int i = items.Length - 1; i > 0; i--)
        {
            int j = Next(i + 1);
            (items[i], items[j]) = (items[j], items[i]);
        }
    }

    // SplitMix64 mixes a counter into a well-distributed 64-bit value.
    private static ulong SplitMix(ref ulong x)
    {
        x += 0x9E3779B97F4A7C15UL;
        ulong z = x;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
        return z ^ (z >> 31);
    }

    private static ulong Rotl(ulong x, int k) => (x << k) | (x >> (64 - k));
}

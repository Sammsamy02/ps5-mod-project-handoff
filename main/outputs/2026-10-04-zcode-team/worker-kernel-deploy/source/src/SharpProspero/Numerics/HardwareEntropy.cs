// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Random;
using System;
using System.Buffers.Binary;

namespace SharpProspero.Numerics;

/// <summary>
/// Random bytes from the system's entropy source. Use it for seeds and unpredictable values; for a
/// fast, reproducible gameplay sequence, seed a <see cref="GameRandom"/> from it instead.
/// </summary>
public static class HardwareEntropy
{
    /// <summary>Fills <paramref name="destination"/> with random bytes.</summary>
    public static unsafe void Fill(Span<byte> destination)
    {
        int offset = 0;
        while (offset < destination.Length)
        {
            int chunk = Math.Min(SceRandom.MaxSize, destination.Length - offset);
            fixed (byte* p = destination[offset..])
                SceResult.ThrowIfFailed(SceRandom.sceRandomGetRandomNumber(p, (nuint)chunk),
                    nameof(SceRandom.sceRandomGetRandomNumber));
            offset += chunk;
        }
    }

    /// <summary>A random 64-bit value.</summary>
    public static ulong NextUInt64()
    {
        Span<byte> bytes = stackalloc byte[8];
        Fill(bytes);
        return BinaryPrimitives.ReadUInt64LittleEndian(bytes);
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Process;
using System;

namespace SharpProspero.Payload.Orchestration;

/// <summary>
/// Cheat manager for reading and writing process memory values. Extends the
/// <see cref="PayloadProcessMemory"/> pattern scanner with value comparison
/// and multi-pass narrowing.
/// </summary>
public static unsafe class PayloadCheatManager
{
    /// <summary>
    /// Searches a process's memory for all locations containing the given 32-bit value.
    /// Returns the addresses in <paramref name="results"/>.
    /// </summary>
    /// <param name="pid">Target process.</param>
    /// <param name="start">Start of the search range.</param>
    /// <param name="length">Length of the search range.</param>
    /// <param name="value">The 32-bit value to find.</param>
    /// <param name="results">Buffer to receive matching addresses.</param>
    /// <param name="maxResults">Maximum number of results.</param>
    /// <returns>Number of matches found.</returns>
    public static int SearchU32(int pid, nint start, nuint length, uint value,
        Span<nint> results, int maxResults)
    {
        Span<byte> pattern = stackalloc byte[4];
        Span<byte> mask = stackalloc byte[4];
        BitConverter.TryWriteBytes(pattern, value);
        mask.Fill(0xFF);

        int count = 0;
        nint offset = 0;
        while (count < maxResults && (nuint)offset < length - 3)
        {
            nint match = PayloadProcessMemory.PatternScan(pid, start + offset,
                length - (nuint)offset, pattern, mask);
            if (match == 0) break;
            results[count++] = match;
            offset = match - start + 4;
        }
        return count;
    }

    /// <summary>
    /// Writes a 32-bit value to a specific address in a target process.
    /// </summary>
    public static bool WriteU32(int pid, nint addr, uint value)
    {
        return PayloadProcessMemory.WriteU32(pid, addr, value) == 0;
    }

    /// <summary>
    /// Writes a float value to a specific address in a target process.
    /// </summary>
    public static bool WriteFloat(int pid, nint addr, float value)
    {
        uint bits = BitConverter.SingleToUInt32Bits(value);
        return PayloadProcessMemory.WriteU32(pid, addr, bits) == 0;
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Security;

/// <summary>
/// The CRC-32 checksum (the reflected variant that zip archives and PNG files use), a fast 32-bit
/// integrity check. It catches accidental corruption; it is not a cryptographic hash, so use
/// <see cref="Sha256"/> to guard against deliberate tampering. Use <see cref="Compute"/> for a block of
/// bytes, <see cref="ComputeFileValue"/> for a file, or update one incrementally.
/// </summary>
public sealed class Crc32 : HashAlgorithm
{
    private static readonly uint[] Table = BuildTable();

    private uint _state = 0xFFFFFFFF;

    /// <inheritdoc/>
    public override int HashSize => 4;

    /// <summary>The checksum of everything added so far.</summary>
    public uint Value => _state ^ 0xFFFFFFFF;

    /// <inheritdoc/>
    public override void Update(ReadOnlySpan<byte> data)
    {
        uint state = _state;
        foreach (byte b in data)
            state = (state >> 8) ^ Table[(state ^ b) & 0xFF];
        _state = state;
    }

    /// <inheritdoc/>
    protected override void FinishCore(Span<byte> destination) => BinaryPrimitives.WriteUInt32BigEndian(destination, Value);

    /// <summary>Computes the CRC-32 of <paramref name="data"/>.</summary>
    public static uint Compute(ReadOnlySpan<byte> data)
    {
        var crc = new Crc32();
        crc.Update(data);
        return crc.Value;
    }

    /// <summary>Computes the CRC-32 of the file at <paramref name="path"/>.</summary>
    public static uint ComputeFileValue(string path)
    {
        var crc = new Crc32();
        crc.ComputeFile(path);
        return crc.Value;
    }

    private static uint[] BuildTable()
    {
        uint[] table = new uint[256];
        for (uint i = 0; i < 256; i++)
        {
            uint c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) != 0 ? 0xEDB88320 ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        return table;
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Numerics;

namespace SharpProspero.Security;

/// <summary>
/// The SHA-256 digest, a 32-byte hash used to verify that a file matches a published checksum or that
/// two files are identical. Use the static <see cref="Hash(ReadOnlySpan{byte})"/> for a block of bytes,
/// <see cref="HashFile"/> for a file, or construct one and call <see cref="HashAlgorithm.Update"/> to
/// hash a stream.
/// </summary>
/// <example>
/// <code>
/// string checksum = Sha256.HashFileHex("/app0/data.bin");
/// bool ok = checksum == published;
/// </code>
/// </example>
public sealed class Sha256 : BlockHashAlgorithm
{
    private static readonly uint[] K =
    [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];

    private uint _h0 = 0x6a09e667, _h1 = 0xbb67ae85, _h2 = 0x3c6ef372, _h3 = 0xa54ff53a;
    private uint _h4 = 0x510e527f, _h5 = 0x9b05688c, _h6 = 0x1f83d9ab, _h7 = 0x5be0cd19;

    /// <inheritdoc/>
    public override int HashSize => 32;

    /// <inheritdoc/>
    protected override bool LengthIsBigEndian => true;

    /// <inheritdoc/>
    protected override void ProcessBlock(ReadOnlySpan<byte> block)
    {
        Span<uint> w = stackalloc uint[64];
        for (int i = 0; i < 16; i++)
            w[i] = BinaryPrimitives.ReadUInt32BigEndian(block[(i * 4)..]);
        for (int i = 16; i < 64; i++)
        {
            uint s0 = BitOperations.RotateRight(w[i - 15], 7) ^ BitOperations.RotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint s1 = BitOperations.RotateRight(w[i - 2], 17) ^ BitOperations.RotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint a = _h0, b = _h1, c = _h2, d = _h3, e = _h4, f = _h5, g = _h6, h = _h7;
        for (int i = 0; i < 64; i++)
        {
            uint s1 = BitOperations.RotateRight(e, 6) ^ BitOperations.RotateRight(e, 11) ^ BitOperations.RotateRight(e, 25);
            uint ch = (e & f) ^ (~e & g);
            uint temp1 = h + s1 + ch + K[i] + w[i];
            uint s0 = BitOperations.RotateRight(a, 2) ^ BitOperations.RotateRight(a, 13) ^ BitOperations.RotateRight(a, 22);
            uint maj = (a & b) ^ (a & c) ^ (b & c);
            uint temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }

        _h0 += a; _h1 += b; _h2 += c; _h3 += d; _h4 += e; _h5 += f; _h6 += g; _h7 += h;
    }

    /// <inheritdoc/>
    protected override void WriteDigest(Span<byte> destination)
    {
        BinaryPrimitives.WriteUInt32BigEndian(destination, _h0);
        BinaryPrimitives.WriteUInt32BigEndian(destination[4..], _h1);
        BinaryPrimitives.WriteUInt32BigEndian(destination[8..], _h2);
        BinaryPrimitives.WriteUInt32BigEndian(destination[12..], _h3);
        BinaryPrimitives.WriteUInt32BigEndian(destination[16..], _h4);
        BinaryPrimitives.WriteUInt32BigEndian(destination[20..], _h5);
        BinaryPrimitives.WriteUInt32BigEndian(destination[24..], _h6);
        BinaryPrimitives.WriteUInt32BigEndian(destination[28..], _h7);
    }

    /// <summary>Computes the SHA-256 digest of <paramref name="data"/>.</summary>
    public static byte[] Hash(ReadOnlySpan<byte> data)
    {
        var sha = new Sha256();
        sha.Update(data);
        return sha.Finish();
    }

    /// <summary>Computes the SHA-256 digest of <paramref name="data"/> as a lowercase hexadecimal string.</summary>
    public static string HashHex(ReadOnlySpan<byte> data) => Convert.ToHexStringLower(Hash(data));

    /// <summary>Computes the SHA-256 digest of the file at <paramref name="path"/>.</summary>
    public static byte[] HashFile(string path) => new Sha256().ComputeFile(path);

    /// <summary>Computes the SHA-256 digest of the file at <paramref name="path"/> as a lowercase hexadecimal string.</summary>
    public static string HashFileHex(string path) => Convert.ToHexStringLower(HashFile(path));
}

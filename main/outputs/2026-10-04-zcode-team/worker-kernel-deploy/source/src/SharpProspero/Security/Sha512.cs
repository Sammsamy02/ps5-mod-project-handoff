// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Numerics;

namespace SharpProspero.Security;

/// <summary>
/// The SHA-512 digest, a 64-byte hash for verifying a file against a published checksum where the
/// stronger, wider digest is wanted. Use the static <see cref="Hash(ReadOnlySpan{byte})"/> for a block
/// of bytes, <see cref="HashFile"/> for a file, or construct one and call
/// <see cref="HashAlgorithm.Update"/> to hash a stream. It processes 128-byte blocks, so it does not
/// share the 64-byte machinery of the shorter digests.
/// </summary>
public sealed class Sha512 : HashAlgorithm
{
    private static readonly ulong[] K =
    [
        0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
        0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
        0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
        0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
        0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
        0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
        0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
        0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
        0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
        0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
        0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
        0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
        0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
        0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
        0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
        0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
        0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
        0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
        0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
        0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
    ];

    private ulong _h0 = 0x6a09e667f3bcc908, _h1 = 0xbb67ae8584caa73b, _h2 = 0x3c6ef372fe94f82b, _h3 = 0xa54ff53a5f1d36f1;
    private ulong _h4 = 0x510e527fade682d1, _h5 = 0x9b05688c2b3e6c1f, _h6 = 0x1f83d9abfb41bd6b, _h7 = 0x5be0cd19137e2179;

    private readonly byte[] _block = new byte[128];
    private int _blockLength;
    private ulong _totalBytes;

    /// <inheritdoc/>
    public override int HashSize => 64;

    /// <inheritdoc/>
    public override void Update(ReadOnlySpan<byte> data)
    {
        _totalBytes += (ulong)data.Length;

        // Top up a partial block first; only once it is full is it transformed.
        if (_blockLength > 0)
        {
            int take = Math.Min(128 - _blockLength, data.Length);
            data[..take].CopyTo(_block.AsSpan(_blockLength));
            _blockLength += take;
            data = data[take..];
            if (_blockLength < 128)
                return;
            ProcessBlock(_block);
            _blockLength = 0;
        }

        while (data.Length >= 128)
        {
            ProcessBlock(data[..128]);
            data = data[128..];
        }

        if (!data.IsEmpty)
        {
            data.CopyTo(_block);
            _blockLength = data.Length;
        }
    }

    /// <inheritdoc/>
    protected override void FinishCore(Span<byte> destination)
    {
        ulong bitLength = _totalBytes * 8;

        // Terminator, zero padding, then the message length as a 128-bit big-endian count. The high 64
        // bits are always zero for any message that fits in memory, so only the low word is written.
        Span<byte> tail = stackalloc byte[256];
        tail[0] = 0x80;
        int padLength = _blockLength < 112 ? 112 - _blockLength : 240 - _blockLength;
        BinaryPrimitives.WriteUInt64BigEndian(tail.Slice(padLength + 8, 8), bitLength);
        Update(tail[..(padLength + 16)]);

        BinaryPrimitives.WriteUInt64BigEndian(destination, _h0);
        BinaryPrimitives.WriteUInt64BigEndian(destination[8..], _h1);
        BinaryPrimitives.WriteUInt64BigEndian(destination[16..], _h2);
        BinaryPrimitives.WriteUInt64BigEndian(destination[24..], _h3);
        BinaryPrimitives.WriteUInt64BigEndian(destination[32..], _h4);
        BinaryPrimitives.WriteUInt64BigEndian(destination[40..], _h5);
        BinaryPrimitives.WriteUInt64BigEndian(destination[48..], _h6);
        BinaryPrimitives.WriteUInt64BigEndian(destination[56..], _h7);
    }

    private void ProcessBlock(ReadOnlySpan<byte> block)
    {
        Span<ulong> w = stackalloc ulong[80];
        for (int i = 0; i < 16; i++)
            w[i] = BinaryPrimitives.ReadUInt64BigEndian(block[(i * 8)..]);
        for (int i = 16; i < 80; i++)
        {
            ulong s0 = BitOperations.RotateRight(w[i - 15], 1) ^ BitOperations.RotateRight(w[i - 15], 8) ^ (w[i - 15] >> 7);
            ulong s1 = BitOperations.RotateRight(w[i - 2], 19) ^ BitOperations.RotateRight(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        ulong a = _h0, b = _h1, c = _h2, d = _h3, e = _h4, f = _h5, g = _h6, h = _h7;
        for (int i = 0; i < 80; i++)
        {
            ulong s1 = BitOperations.RotateRight(e, 14) ^ BitOperations.RotateRight(e, 18) ^ BitOperations.RotateRight(e, 41);
            ulong ch = (e & f) ^ (~e & g);
            ulong temp1 = h + s1 + ch + K[i] + w[i];
            ulong s0 = BitOperations.RotateRight(a, 28) ^ BitOperations.RotateRight(a, 34) ^ BitOperations.RotateRight(a, 39);
            ulong maj = (a & b) ^ (a & c) ^ (b & c);
            ulong temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }

        _h0 += a; _h1 += b; _h2 += c; _h3 += d; _h4 += e; _h5 += f; _h6 += g; _h7 += h;
    }

    /// <summary>Computes the SHA-512 digest of <paramref name="data"/>.</summary>
    public static byte[] Hash(ReadOnlySpan<byte> data)
    {
        var sha = new Sha512();
        sha.Update(data);
        return sha.Finish();
    }

    /// <summary>Computes the SHA-512 digest of <paramref name="data"/> as a lowercase hexadecimal string.</summary>
    public static string HashHex(ReadOnlySpan<byte> data) => Convert.ToHexStringLower(Hash(data));

    /// <summary>Computes the SHA-512 digest of the file at <paramref name="path"/>.</summary>
    public static byte[] HashFile(string path) => new Sha512().ComputeFile(path);

    /// <summary>Computes the SHA-512 digest of the file at <paramref name="path"/> as a lowercase hexadecimal string.</summary>
    public static string HashFileHex(string path) => Convert.ToHexStringLower(HashFile(path));
}

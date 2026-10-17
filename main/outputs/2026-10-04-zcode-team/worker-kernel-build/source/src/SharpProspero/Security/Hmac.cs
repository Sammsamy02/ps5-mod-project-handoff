// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Security;

/// <summary>
/// A keyed digest (HMAC): a hash that also depends on a secret key, so it proves a message was not
/// changed by anyone without the key. Use the static helpers — <see cref="Sha256"/>,
/// <see cref="Sha512"/>, <see cref="Sha1"/>, <see cref="Md5"/> — for a key and a block of bytes, or
/// construct one and call <see cref="Update"/> to key a stream, then <see cref="Finish()"/>.
/// </summary>
/// <remarks>
/// This is the standard construction over any of the digests here, so it needs no system module and
/// gives the same result on the device and in tests. Each instance keeps state; use a fresh one per tag.
/// </remarks>
public sealed class Hmac
{
    private const byte InnerPad = 0x36;
    private const byte OuterPad = 0x5c;

    private readonly Func<HashAlgorithm> _factory;
    private readonly byte[] _outerKeyBlock;
    private readonly HashAlgorithm _inner;

    /// <summary>
    /// Creates a keyed digest over the hash that <paramref name="hashFactory"/> makes, using
    /// <paramref name="key"/>. <paramref name="blockSize"/> is the hash's internal block size in bytes
    /// (64 for SHA-256, SHA-1 and MD5; 128 for SHA-512).
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="hashFactory"/> is null.</exception>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="blockSize"/> is not positive.</exception>
    public Hmac(ReadOnlySpan<byte> key, Func<HashAlgorithm> hashFactory, int blockSize)
    {
        ArgumentNullException.ThrowIfNull(hashFactory);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(blockSize);
        _factory = hashFactory;

        // The key is reduced to the block size: a longer key is hashed first, a shorter one is zero
        // padded. Each block is then combined with the inner and outer constants.
        byte[] keyBlock = new byte[blockSize];
        if (key.Length > blockSize)
        {
            HashAlgorithm keyHash = hashFactory();
            keyHash.Update(key);
            keyHash.Finish(keyBlock); // digest is never longer than the block size for these hashes
        }
        else
        {
            key.CopyTo(keyBlock);
        }

        _outerKeyBlock = new byte[blockSize];
        Span<byte> innerKeyBlock = blockSize <= 256 ? stackalloc byte[blockSize] : new byte[blockSize];
        for (int i = 0; i < blockSize; i++)
        {
            _outerKeyBlock[i] = (byte)(keyBlock[i] ^ OuterPad);
            innerKeyBlock[i] = (byte)(keyBlock[i] ^ InnerPad);
        }

        _inner = hashFactory();
        _inner.Update(innerKeyBlock);
    }

    /// <summary>The size of the tag in bytes, the same as the underlying hash.</summary>
    public int HashSize => _inner.HashSize;

    /// <summary>Adds <paramref name="data"/> to the running tag.</summary>
    public void Update(ReadOnlySpan<byte> data) => _inner.Update(data);

    /// <summary>Writes the final tag into <paramref name="destination"/>, which must hold <see cref="HashSize"/> bytes.</summary>
    /// <exception cref="ArgumentException"><paramref name="destination"/> is too small.</exception>
    public void Finish(Span<byte> destination)
    {
        if (destination.Length < HashSize)
            throw new ArgumentException($"The destination needs at least {HashSize} bytes.", nameof(destination));

        Span<byte> innerDigest = stackalloc byte[HashSize];
        _inner.Finish(innerDigest);

        HashAlgorithm outer = _factory();
        outer.Update(_outerKeyBlock);
        outer.Update(innerDigest);
        outer.Finish(destination);
    }

    /// <summary>Returns the final tag as a new array.</summary>
    public byte[] Finish()
    {
        byte[] tag = new byte[HashSize];
        Finish(tag);
        return tag;
    }

    /// <summary>Returns the final tag as a lowercase hexadecimal string.</summary>
    public string FinishHex() => Convert.ToHexStringLower(Finish());

    /// <summary>Computes HMAC-SHA-256 of <paramref name="data"/> under <paramref name="key"/>.</summary>
    public static byte[] Sha256(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Compute(key, data, static () => new Sha256(), 64);

    /// <summary>Computes HMAC-SHA-256 of <paramref name="data"/> under <paramref name="key"/> as lowercase hexadecimal.</summary>
    public static string Sha256Hex(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Convert.ToHexStringLower(Sha256(key, data));

    /// <summary>Computes HMAC-SHA-512 of <paramref name="data"/> under <paramref name="key"/>.</summary>
    public static byte[] Sha512(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Compute(key, data, static () => new Sha512(), 128);

    /// <summary>Computes HMAC-SHA-512 of <paramref name="data"/> under <paramref name="key"/> as lowercase hexadecimal.</summary>
    public static string Sha512Hex(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Convert.ToHexStringLower(Sha512(key, data));

    /// <summary>Computes HMAC-SHA-1 of <paramref name="data"/> under <paramref name="key"/>.</summary>
    public static byte[] Sha1(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Compute(key, data, static () => new Sha1(), 64);

    /// <summary>Computes HMAC-SHA-1 of <paramref name="data"/> under <paramref name="key"/> as lowercase hexadecimal.</summary>
    public static string Sha1Hex(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Convert.ToHexStringLower(Sha1(key, data));

    /// <summary>Computes HMAC-MD5 of <paramref name="data"/> under <paramref name="key"/>.</summary>
    public static byte[] Md5(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Compute(key, data, static () => new Md5(), 64);

    /// <summary>Computes HMAC-MD5 of <paramref name="data"/> under <paramref name="key"/> as lowercase hexadecimal.</summary>
    public static string Md5Hex(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data)
        => Convert.ToHexStringLower(Md5(key, data));

    private static byte[] Compute(ReadOnlySpan<byte> key, ReadOnlySpan<byte> data, Func<HashAlgorithm> factory, int blockSize)
    {
        var hmac = new Hmac(key, factory, blockSize);
        hmac.Update(data);
        return hmac.Finish();
    }
}

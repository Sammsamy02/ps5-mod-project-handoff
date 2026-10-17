// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Security.Cryptography;
using System.Text;

namespace SharpProspero.Prx;

/// <summary>
/// Computes the mangled identifier for a symbol name. A module's dynamic symbols are keyed by an
/// 11-character identifier derived from the name, so a linker and a loader can bind a plain name to
/// the module's export.
/// </summary>
public static class SceNid
{
    // Fixed 16-byte suffix appended to the name before hashing.
    private static ReadOnlySpan<byte> Suffix =>
    [
        0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
        0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30,
    ];

    /// <summary>Length of an identifier in characters.</summary>
    public const int Length = 11;

    /// <summary>
    /// Returns the eight-byte identifier value for <paramref name="name"/>: the first eight bytes of
    /// the SHA-1 of the name followed by the suffix.
    /// </summary>
    public static byte[] ComputeBytes(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        int nameLength = Encoding.ASCII.GetByteCount(name);
        byte[] input = new byte[nameLength + Suffix.Length];
        Encoding.ASCII.GetBytes(name, input);
        Suffix.CopyTo(input.AsSpan(nameLength));

        byte[] digest = SHA1.HashData(input);
        byte[] value = new byte[8];
        Array.Copy(digest, value, 8);
        return value;
    }

    /// <summary>
    /// Returns the 11-character identifier for <paramref name="name"/>. The eight-byte value is
    /// emitted most-significant-byte first and base64-encoded with '-' in place of '/'.
    /// </summary>
    public static string Compute(string name)
    {
        byte[] value = ComputeBytes(name);
        Array.Reverse(value);
        string encoded = Convert.ToBase64String(value);
        return encoded[..Length].Replace('/', '-');
    }
}

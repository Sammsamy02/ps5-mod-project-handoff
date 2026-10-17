// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK

using System;
using System.Security.Cryptography;
using System.Text;

namespace SharpProspero.Link;

/// <summary>
/// Encodes a plain symbol name into its 11-character identifier the dynamic linker resolves by.
/// The identifier is the first eight bytes of the salted SHA-1 digest, read as a little-endian
/// unsigned 64-bit value and re-emitted big-endian - equivalent to reversing the eight bytes -
/// then base-64-encoded with a custom alphabet, with four trailing zero bytes appended so the
/// third and fourth base-64 triplets read clean padding.
/// </summary>
/// <remarks>
/// The ground-truth reference is the on-device module the platform's dynamic linker actually matches
/// against. Every export of the SDK's <c>target/sce_module/libc.prx</c> is stored as a name of the
/// form <c>&lt;nid&gt;#&lt;lib&gt;#&lt;mod&gt;</c> in the module's own <c>.dynstr</c>: a run-time
/// resolver lookup succeeds only when the payload asks for exactly the identifier the module
/// publishes there. Grepping the payload-side libc.prx confirms this encoder byte for byte across
/// twenty-eight common runtime imports (<c>puts</c>=<c>YQ0navp+YIc</c>, <c>memset</c>=<c>8zTFvBIAIN8</c>,
/// <c>malloc</c>=<c>gQX+4GDQjpM</c>, <c>free</c>=<c>tIhsqj0qsFE</c>, <c>memcpy</c>=<c>Q3VBxCXhUHs</c>,
/// <c>strlen</c>=<c>j4ViWNHEgww</c>, <c>fclose</c>=<c>uodLYyUip20</c>, ...). The salt is the sixteen
/// bytes the SDK's own <c>crt1.o</c> stores at <c>.rodata.cst16 + 0x10</c>. A draft that dropped the
/// eight-byte swap produced identifiers that matched the SDK stub library's <c>.scenid</c> section
/// but no on-device module - the stub library stores raw bytes in the linker's own internal order,
/// and the linker itself byte-swaps them to build the runtime identifier the resolver looks for.
/// </remarks>
public static class NidEncoder
{
    private static ReadOnlySpan<byte> Salt =>
    [
        0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
        0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30,
    ];

    private const string Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

    /// <summary>Returns the 11-character encoded identifier for <paramref name="symbolName"/>.</summary>
    public static string Encode(string symbolName)
    {
        Span<byte> input = stackalloc byte[Encoding.UTF8.GetByteCount(symbolName) + Salt.Length];
        int nameLen = Encoding.UTF8.GetBytes(symbolName, input);
        Salt.CopyTo(input[nameLen..]);

        Span<byte> digest = stackalloc byte[20];
        SHA1.HashData(input, digest);

        // Read the first eight bytes as a little-endian unsigned 64-bit value and re-emit them
        // big-endian - reverse in place - then zero the last four of the twelve encoded bytes so the
        // last two base-64 triplets read clean trailing zeros. The reversal is what makes the encoder
        // match the identifiers the on-device modules actually publish (grepping the SDK's own
        // libc.prx confirms twenty-eight known imports byte for byte); an earlier draft that skipped
        // it matched the SDK stub library's on-disk `.scenid` bytes instead, which the resolver at
        // run time does not look for.
        (digest[0], digest[7]) = (digest[7], digest[0]);
        (digest[1], digest[6]) = (digest[6], digest[1]);
        (digest[2], digest[5]) = (digest[5], digest[2]);
        (digest[3], digest[4]) = (digest[4], digest[3]);
        digest[8..16].Clear();

        Span<char> result = stackalloc char[16];
        int pos = 0;
        for (int i = 0; i < 12; i += 3)
        {
            int abc = (digest[i] << 16) | (digest[i + 1] << 8) | digest[i + 2];
            result[pos++] = Alphabet[(abc >> 18) & 0x3F];
            result[pos++] = Alphabet[(abc >> 12) & 0x3F];
            result[pos++] = Alphabet[(abc >> 6) & 0x3F];
            result[pos++] = Alphabet[abc & 0x3F];
        }

        return result[..11].ToString();
    }
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Buffers;

/// <summary>
/// Turns bytes into text and back: hexadecimal, Base32, and Base64 (standard or URL-safe). Use it for an
/// HTTP header, a token or a <c>data:</c> URL, a config or save blob, or the encode and decode a developer
/// tool offers. Decoding ignores whitespace and, for the base codecs, padding.
/// </summary>
public static class BaseN
{
    private const string Base64Standard = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    private const string Base64UrlSafe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    private const string Base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    /// <summary>Encodes <paramref name="data"/> as hexadecimal, lower-case unless <paramref name="upperCase"/> is set.</summary>
    public static string ToHex(ReadOnlySpan<byte> data, bool upperCase = false)
    {
        ReadOnlySpan<char> digits = upperCase ? "0123456789ABCDEF" : "0123456789abcdef";
        var builder = new StringBuilder(Capacity((long)data.Length * 2));
        foreach (byte b in data)
        {
            builder.Append(digits[b >> 4]);
            builder.Append(digits[b & 0xF]);
        }

        return builder.ToString();
    }

    /// <summary>Decodes a hex string, ignoring whitespace.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="text"/> is null.</exception>
    /// <exception cref="FormatException">A character is not a hex digit, or the digit count is odd.</exception>
    public static byte[] FromHex(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        var nibbles = new List<int>(text.Length);
        foreach (char c in text)
        {
            if (char.IsWhiteSpace(c))
                continue;
            int value = HexValue(c);
            if (value < 0)
                throw new FormatException($"'{c}' is not a hexadecimal digit.");
            nibbles.Add(value);
        }

        if (nibbles.Count % 2 != 0)
            throw new FormatException("The hex string has an odd number of digits.");

        byte[] result = new byte[nibbles.Count / 2];
        for (int i = 0; i < result.Length; i++)
            result[i] = (byte)((nibbles[2 * i] << 4) | nibbles[2 * i + 1]);
        return result;
    }

    /// <summary>
    /// Encodes <paramref name="data"/> as Base64. With <paramref name="urlSafe"/> the alphabet uses
    /// <c>-</c> and <c>_</c> instead of <c>+</c> and <c>/</c>; with <paramref name="padding"/> false the
    /// trailing <c>=</c> characters are omitted.
    /// </summary>
    public static string ToBase64(ReadOnlySpan<byte> data, bool urlSafe = false, bool padding = true)
    {
        string alphabet = urlSafe ? Base64UrlSafe : Base64Standard;
        var builder = new StringBuilder(Capacity(((long)data.Length + 2) / 3 * 4));

        int i = 0;
        for (; i + 3 <= data.Length; i += 3)
        {
            int n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
            builder.Append(alphabet[(n >> 18) & 0x3F]);
            builder.Append(alphabet[(n >> 12) & 0x3F]);
            builder.Append(alphabet[(n >> 6) & 0x3F]);
            builder.Append(alphabet[n & 0x3F]);
        }

        int remaining = data.Length - i;
        if (remaining == 1)
        {
            int n = data[i] << 16;
            builder.Append(alphabet[(n >> 18) & 0x3F]);
            builder.Append(alphabet[(n >> 12) & 0x3F]);
            if (padding)
                builder.Append("==");
        }
        else if (remaining == 2)
        {
            int n = (data[i] << 16) | (data[i + 1] << 8);
            builder.Append(alphabet[(n >> 18) & 0x3F]);
            builder.Append(alphabet[(n >> 12) & 0x3F]);
            builder.Append(alphabet[(n >> 6) & 0x3F]);
            if (padding)
                builder.Append('=');
        }

        return builder.ToString();
    }

    /// <summary>Decodes Base64, accepting either alphabet, with or without padding, and ignoring whitespace.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="text"/> is null.</exception>
    /// <exception cref="FormatException">A character is not valid Base64.</exception>
    public static byte[] FromBase64(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        var output = new List<byte>(Capacity((long)text.Length * 3 / 4 + 1));
        int buffer = 0;
        int bits = 0;
        foreach (char c in text)
        {
            if (c == '=' || char.IsWhiteSpace(c))
                continue;
            int value = Base64Value(c);
            if (value < 0)
                throw new FormatException($"'{c}' is not a valid Base64 character.");

            buffer = (buffer << 6) | value;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                output.Add((byte)((buffer >> bits) & 0xFF));
                buffer &= (1 << bits) - 1;
            }
        }

        return [.. output];
    }

    /// <summary>Encodes <paramref name="data"/> as Base32 (RFC 4648), padded with <c>=</c> unless disabled.</summary>
    public static string ToBase32(ReadOnlySpan<byte> data, bool padding = true)
    {
        var builder = new StringBuilder(Capacity(((long)data.Length + 4) / 5 * 8));
        int buffer = 0;
        int bits = 0;
        foreach (byte b in data)
        {
            buffer = (buffer << 8) | b;
            bits += 8;
            while (bits >= 5)
            {
                bits -= 5;
                builder.Append(Base32Alphabet[(buffer >> bits) & 0x1F]);
            }

            buffer &= (1 << bits) - 1;
        }

        if (bits > 0)
            builder.Append(Base32Alphabet[(buffer << (5 - bits)) & 0x1F]);

        if (padding)
        {
            while (builder.Length % 8 != 0)
                builder.Append('=');
        }

        return builder.ToString();
    }

    /// <summary>Decodes Base32 (RFC 4648), accepting either case, with or without padding, ignoring whitespace.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="text"/> is null.</exception>
    /// <exception cref="FormatException">A character is not valid Base32.</exception>
    public static byte[] FromBase32(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        var output = new List<byte>(Capacity((long)text.Length * 5 / 8 + 1));
        int buffer = 0;
        int bits = 0;
        foreach (char c in text)
        {
            if (c == '=' || char.IsWhiteSpace(c))
                continue;
            int value = Base32Value(c);
            if (value < 0)
                throw new FormatException($"'{c}' is not a valid Base32 character.");

            buffer = (buffer << 5) | value;
            bits += 5;
            if (bits >= 8)
            {
                bits -= 8;
                output.Add((byte)((buffer >> bits) & 0xFF));
                buffer &= (1 << bits) - 1;
            }
        }

        return [.. output];
    }

    // A capacity hint clamped to a valid int, so a very large input does not overflow the estimate into a
    // negative value that the StringBuilder or List constructor would reject.
    private static int Capacity(long estimate) => (int)Math.Min(int.MaxValue, estimate);

    private static int HexValue(char c) => c switch
    {
        >= '0' and <= '9' => c - '0',
        >= 'a' and <= 'f' => c - 'a' + 10,
        >= 'A' and <= 'F' => c - 'A' + 10,
        _ => -1,
    };

    private static int Base64Value(char c) => c switch
    {
        >= 'A' and <= 'Z' => c - 'A',
        >= 'a' and <= 'z' => c - 'a' + 26,
        >= '0' and <= '9' => c - '0' + 52,
        '+' or '-' => 62,
        '/' or '_' => 63,
        _ => -1,
    };

    private static int Base32Value(char c) => c switch
    {
        >= 'A' and <= 'Z' => c - 'A',
        >= 'a' and <= 'z' => c - 'a',
        >= '2' and <= '7' => c - '2' + 26,
        _ => -1,
    };
}

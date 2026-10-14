// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// The URL text handling the HTTP client and server need: percent-encode a value so it is safe in a URL,
/// decode one back, build a query string from name/value pairs, and parse a query string or an
/// <c>application/x-www-form-urlencoded</c> request body into pairs. Text is handled as UTF-8.
/// </summary>
public static class WebEncoding
{
    private static readonly char[] HexUpper = "0123456789ABCDEF".ToCharArray();

    /// <summary>
    /// Percent-encodes <paramref name="value"/>, leaving the unreserved characters (letters, digits, and
    /// <c>- . _ ~</c>) as they are. With <paramref name="spaceAsPlus"/> a space becomes <c>+</c> (form
    /// style); otherwise it becomes <c>%20</c>.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="value"/> is null.</exception>
    public static string PercentEncode(string value, bool spaceAsPlus = false)
    {
        ArgumentNullException.ThrowIfNull(value);
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        var builder = new StringBuilder(bytes.Length);
        foreach (byte b in bytes)
        {
            if (IsUnreserved(b))
                builder.Append((char)b);
            else if (b == ' ' && spaceAsPlus)
                builder.Append('+');
            else
                builder.Append('%').Append(HexUpper[b >> 4]).Append(HexUpper[b & 0xF]);
        }

        return builder.ToString();
    }

    /// <summary>
    /// Decodes percent-escapes in <paramref name="value"/>. With <paramref name="plusAsSpace"/> a
    /// <c>+</c> becomes a space (form style); otherwise <c>+</c> is left as itself.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="value"/> is null.</exception>
    /// <exception cref="FormatException">A <c>%</c> is not followed by two hex digits.</exception>
    public static string PercentDecode(string value, bool plusAsSpace = false)
    {
        ArgumentNullException.ThrowIfNull(value);
        var bytes = new List<byte>(value.Length);
        int i = 0;
        while (i < value.Length)
        {
            char c = value[i];
            if (c == '%')
            {
                if (i + 2 >= value.Length)
                    throw new FormatException("A percent-escape is truncated.");
                int high = HexValue(value[i + 1]);
                int low = HexValue(value[i + 2]);
                if (high < 0 || low < 0)
                    throw new FormatException("A percent-escape has a non-hex digit.");
                bytes.Add((byte)((high << 4) | low));
                i += 3;
            }
            else if (c == '+' && plusAsSpace)
            {
                bytes.Add((byte)' ');
                i++;
            }
            else
            {
                // Take the whole run of literal characters and encode it at once, so a non-BMP character
                // (a surrogate pair) is not split into two lone surrogates that would each become "?".
                int start = i;
                while (i < value.Length && value[i] != '%' && !(value[i] == '+' && plusAsSpace))
                    i++;
                bytes.AddRange(Encoding.UTF8.GetBytes(value[start..i]));
            }
        }

        return Encoding.UTF8.GetString([.. bytes]);
    }

    /// <summary>
    /// Joins <paramref name="parameters"/> into a query string such as "a=1&amp;b=two%20words", encoding
    /// each name and value in form style (a space becomes <c>+</c>). No leading <c>?</c> is added.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="parameters"/> is null.</exception>
    public static string BuildQuery(IEnumerable<KeyValuePair<string, string>> parameters)
    {
        ArgumentNullException.ThrowIfNull(parameters);
        var builder = new StringBuilder();
        foreach (KeyValuePair<string, string> pair in parameters)
        {
            if (builder.Length > 0)
                builder.Append('&');
            builder.Append(PercentEncode(pair.Key, spaceAsPlus: true));
            builder.Append('=');
            builder.Append(PercentEncode(pair.Value ?? string.Empty, spaceAsPlus: true));
        }

        return builder.ToString();
    }

    /// <summary>
    /// Parses a query string or a form body into name/value pairs, decoding each. Any <c>?</c> and the
    /// text before it (a full URL) is skipped. A name with no <c>=</c> yields an empty value. Names may
    /// repeat, so the result is a list rather than a map.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="query"/> is null.</exception>
    /// <exception cref="FormatException">A percent-escape is malformed.</exception>
    public static List<KeyValuePair<string, string>> ParseQuery(string query, bool plusAsSpace = true)
    {
        ArgumentNullException.ThrowIfNull(query);
        var result = new List<KeyValuePair<string, string>>();

        int mark = query.IndexOf('?');
        string body = mark >= 0 ? query[(mark + 1)..] : query;
        foreach (string pair in body.Split('&', StringSplitOptions.RemoveEmptyEntries))
        {
            int equals = pair.IndexOf('=');
            string name = equals < 0 ? pair : pair[..equals];
            string value = equals < 0 ? string.Empty : pair[(equals + 1)..];
            result.Add(new KeyValuePair<string, string>(
                PercentDecode(name, plusAsSpace),
                PercentDecode(value, plusAsSpace)));
        }

        return result;
    }

    private static bool IsUnreserved(byte b) =>
        b is (>= (byte)'A' and <= (byte)'Z') or (>= (byte)'a' and <= (byte)'z') or (>= (byte)'0' and <= (byte)'9')
        or (byte)'-' or (byte)'.' or (byte)'_' or (byte)'~';

    private static int HexValue(char c) => c switch
    {
        >= '0' and <= '9' => c - '0',
        >= 'a' and <= 'f' => c - 'a' + 10,
        >= 'A' and <= 'F' => c - 'A' + 10,
        _ => -1,
    };
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Text;

/// <summary>
/// Formats and orders the human-readable strings an application shows but the runtime does not produce:
/// a file size, a playback duration, a filename-friendly sort, aligned columns, and a byte dump. All of
/// it is plain-string work, so it runs anywhere and allocates only the result.
/// </summary>
public static class TextFormat
{
    private static readonly string[] BinarySuffixes = ["B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"];
    private static readonly string[] DecimalSuffixes = ["B", "KB", "MB", "GB", "TB", "PB", "EB"];

    /// <summary>
    /// Formats a byte count as a size, for example <c>1536</c> as "1.5 KiB". With
    /// <paramref name="binary"/> true the step is 1024 and the units are KiB/MiB/…; with it false the
    /// step is 1000 and the units are KB/MB/….
    /// </summary>
    public static string ByteSize(long bytes, bool binary = true)
    {
        bool negative = bytes < 0;
        double value = negative ? -(double)bytes : bytes; // via double so long.MinValue does not overflow
        double step = binary ? 1024d : 1000d;
        string[] suffixes = binary ? BinarySuffixes : DecimalSuffixes;

        int unit = 0;
        while (value >= step && unit < suffixes.Length - 1)
        {
            value /= step;
            unit++;
        }

        // Rounding to one decimal can push the mantissa up to the base (1023.99 KiB -> "1024"); promote
        // one more unit so the result reads "1 MiB" rather than "1024 KiB".
        if (unit > 0 && unit < suffixes.Length - 1 && Math.Round(value, 1) >= step)
        {
            value /= step;
            unit++;
        }

        string number = unit == 0 ? ((long)value).ToString() : value.ToString("0.#");
        return (negative ? "-" : string.Empty) + number + " " + suffixes[unit];
    }

    /// <summary>
    /// Formats a number of seconds as a duration: "m:ss" under an hour, "h:mm:ss" at or above one. A
    /// negative value is treated as zero.
    /// </summary>
    public static string Duration(double totalSeconds)
    {
        if (!double.IsFinite(totalSeconds) || totalSeconds < 0)
            totalSeconds = 0;
        else if (totalSeconds > 359_999_999d)
            totalSeconds = 359_999_999d; // cap at 99999:59:59 so a huge value cannot saturate the cast

        long total = (long)totalSeconds;
        long hours = total / 3600;
        long minutes = total % 3600 / 60;
        long seconds = total % 60;
        return hours > 0 ? $"{hours}:{minutes:00}:{seconds:00}" : $"{minutes}:{seconds:00}";
    }

    /// <summary>An ordering that sorts embedded numbers by value, so "file2" comes before "file10".</summary>
    public static IComparer<string> NaturalComparer { get; } = new NaturalStringComparer();

    /// <summary>
    /// Compares two strings so that runs of digits sort by numeric value rather than character by
    /// character. Letters compare without regard to case. Returns a negative number, zero, or a positive
    /// number in the usual way.
    /// </summary>
    public static int CompareNatural(string left, string right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        int i = 0;
        int j = 0;
        while (i < left.Length && j < right.Length)
        {
            char a = left[i];
            char b = right[j];
            // Only ASCII digits take the numeric path; the trim and code-point compare below assume them.
            // Digits from other scripts fall through to the character comparison, which stays consistent.
            if (IsAsciiDigit(a) && IsAsciiDigit(b))
            {
                int startA = i;
                int startB = j;
                while (i < left.Length && IsAsciiDigit(left[i]))
                    i++;
                while (j < right.Length && IsAsciiDigit(right[j]))
                    j++;

                ReadOnlySpan<char> numberA = left.AsSpan(startA, i - startA).TrimStart('0');
                ReadOnlySpan<char> numberB = right.AsSpan(startB, j - startB).TrimStart('0');
                if (numberA.Length != numberB.Length)
                    return numberA.Length - numberB.Length; // more significant digits means a larger value

                int compare = numberA.SequenceCompareTo(numberB);
                if (compare != 0)
                    return compare;

                // Equal in value: the run with fewer leading zeros orders first, deterministically.
                if (i - startA != j - startB)
                    return (i - startA) - (j - startB);
            }
            else
            {
                char lowerA = char.ToLowerInvariant(a);
                char lowerB = char.ToLowerInvariant(b);
                if (lowerA != lowerB)
                    return lowerA < lowerB ? -1 : 1;
                i++;
                j++;
            }
        }

        return (left.Length - i) - (right.Length - j);
    }

    private static bool IsAsciiDigit(char c) => c is >= '0' and <= '9';

    /// <summary>
    /// Lays out rows of cells as left-aligned columns, each column widened to its longest cell plus
    /// <paramref name="spacing"/> spaces. Rows may be ragged; missing cells are treated as empty. Returns
    /// the rows joined by newlines with no trailing spaces on the last column.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="spacing"/> is negative.</exception>
    public static string Columns(IReadOnlyList<IReadOnlyList<string>> rows, int spacing = 2)
    {
        ArgumentNullException.ThrowIfNull(rows);
        ArgumentOutOfRangeException.ThrowIfNegative(spacing);

        int columnCount = 0;
        foreach (IReadOnlyList<string> row in rows)
            columnCount = Math.Max(columnCount, row.Count);

        int[] widths = new int[columnCount];
        foreach (IReadOnlyList<string> row in rows)
        {
            for (int c = 0; c < row.Count; c++)
                widths[c] = Math.Max(widths[c], row[c]?.Length ?? 0);
        }

        var builder = new StringBuilder();
        for (int r = 0; r < rows.Count; r++)
        {
            IReadOnlyList<string> row = rows[r];
            if (r > 0)
                builder.Append('\n');

            for (int c = 0; c < row.Count; c++)
            {
                string cell = row[c] ?? string.Empty;
                if (c == row.Count - 1)
                {
                    builder.Append(cell); // no padding on the last cell of a row
                }
                else
                {
                    builder.Append(cell);
                    builder.Append(' ', widths[c] - cell.Length + spacing);
                }
            }
        }

        return builder.ToString();
    }

    /// <summary>
    /// Renders <paramref name="data"/> as a hex dump: an eight-digit offset, the bytes in hex, then the
    /// printable characters, one line per <paramref name="bytesPerRow"/> bytes. Returns an empty string
    /// for empty input.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="bytesPerRow"/> is not positive.</exception>
    public static string HexDump(ReadOnlySpan<byte> data, int bytesPerRow = 16)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(bytesPerRow);

        var builder = new StringBuilder();
        for (int offset = 0; offset < data.Length; offset += bytesPerRow)
        {
            builder.Append(offset.ToString("x8")).Append("  ");
            int end = Math.Min(offset + bytesPerRow, data.Length);
            for (int i = offset; i < offset + bytesPerRow; i++)
            {
                if (i < end)
                    builder.Append(data[i].ToString("x2")).Append(' ');
                else
                    builder.Append("   ");
            }

            builder.Append(' ');
            for (int i = offset; i < end; i++)
            {
                byte b = data[i];
                builder.Append(b is >= 0x20 and < 0x7F ? (char)b : '.');
            }

            builder.Append('\n');
        }

        return builder.ToString();
    }

    private sealed class NaturalStringComparer : IComparer<string>
    {
        public int Compare(string? x, string? y) => CompareNatural(x ?? string.Empty, y ?? string.Empty);
    }
}

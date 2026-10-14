// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Globalization;

namespace SharpProspero.Prx;

/// <summary>
/// A system software version, as "MM.mm" (for example 11.20).
/// </summary>
/// <remarks>
/// The two numbers are held the way the system stores them: one byte each, with the digits written
/// out as they read. Version 11.20 is the byte pair <c>0x11 0x20</c>, not <c>0x0B 0x14</c>. Reading
/// those bytes as ordinary numbers turns 11.20 into 17.32, so every conversion here goes through the
/// digits.
/// </remarks>
public readonly record struct SystemVersion : IComparable<SystemVersion>
{
    private SystemVersion(ushort packed) => Packed = packed;

    /// <summary>The version a module or package records when it names no requirement.</summary>
    public static SystemVersion None => new(0);

    /// <summary>The major and minor bytes packed into one value, major first.</summary>
    public ushort Packed { get; }

    /// <summary>True when this is a real version rather than the absent one.</summary>
    public bool HasValue => Packed != 0;

    /// <summary>The major number, as written (11 for 11.20).</summary>
    public int Major => FromDigits((byte)(Packed >> 8));

    /// <summary>The minor number, as written (20 for 11.20).</summary>
    public int Minor => FromDigits((byte)(Packed & 0xFF));

    /// <summary>
    /// The version a module was built against, taken from the module's own parameter block. The block
    /// packs the version as major, minor and patch; the patch is dropped because a requirement names
    /// only the major and minor.
    /// </summary>
    public static SystemVersion FromModuleSdkVersion(uint sdkVersion)
        => sdkVersion == 0 ? None : new SystemVersion((ushort)(sdkVersion >> 16));

    /// <summary>
    /// Reads a version from either form a user or a document can hold it in: "11.20", "1120", or the
    /// 64-bit field a package carries ("0x1120000000000000").
    /// </summary>
    /// <returns>True when <paramref name="text"/> is a version this understands.</returns>
    public static bool TryParse(string? text, out SystemVersion version)
    {
        version = None;
        if (string.IsNullOrWhiteSpace(text))
            return false;

        string s = text.Trim();

        // The 64-bit field a package carries. The version sits in the top 16 bits; the rest is zero.
        if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
        {
            if (!ulong.TryParse(s.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong raw))
                return false;
            ushort top = (ushort)(raw >> 48);
            if (top == 0)
                return false;
            if (!IsDigits((byte)(top >> 8)) || !IsDigits((byte)(top & 0xFF)))
                return false;
            version = new SystemVersion(top);
            return true;
        }

        // "11.20", "2.00", and the same without the separator. A second separator, or a part that is
        // not one or two digits, is not a version.
        string digits;
        int dot = s.IndexOf('.');
        if (dot >= 0)
        {
            string major = s[..dot];
            string minor = s[(dot + 1)..];
            if (minor.Contains('.', StringComparison.Ordinal))
                return false;
            if (major.Length is < 1 or > 2 || minor.Length != 2)
                return false;
            digits = major.PadLeft(2, '0') + minor;
        }
        else
        {
            if (s.Length is < 3 or > 4)
                return false;
            digits = s.PadLeft(4, '0');
        }

        foreach (char c in digits)
        {
            if (c is < '0' or > '9')
                return false;
        }

        ushort packed = (ushort)((ToDigits(digits.AsSpan(0, 2)) << 8) | ToDigits(digits.AsSpan(2, 2)));
        if (packed == 0)
            return false;
        version = new SystemVersion(packed);
        return true;
    }

    /// <summary>Reads a version, and throws when the text is not one.</summary>
    /// <exception cref="FormatException"><paramref name="text"/> is not a version.</exception>
    public static SystemVersion Parse(string? text)
        => TryParse(text, out SystemVersion v)
            ? v
            : throw new FormatException($"'{text}' is not a system version. Use MM.mm (for example 11.20) or the 64-bit form (0x1120000000000000).");

    /// <summary>The value a package's metadata carries for this version.</summary>
    public string ToPackageValue()
        => HasValue ? $"0x{(ulong)Packed << 48:X16}" : "";

    /// <summary>The version as a person reads it, for example "11.20".</summary>
    public override string ToString()
        => HasValue ? $"{(Packed >> 8) & 0xFF:X2}.{Packed & 0xFF:X2}" : "";

    /// <inheritdoc />
    public int CompareTo(SystemVersion other)
    {
        // Both bytes hold their digits in order, so the packed pair orders exactly as the version does.
        return Packed.CompareTo(other.Packed);
    }

    public static bool operator <(SystemVersion left, SystemVersion right) => left.CompareTo(right) < 0;
    public static bool operator >(SystemVersion left, SystemVersion right) => left.CompareTo(right) > 0;
    public static bool operator <=(SystemVersion left, SystemVersion right) => left.CompareTo(right) <= 0;
    public static bool operator >=(SystemVersion left, SystemVersion right) => left.CompareTo(right) >= 0;

    private static bool IsDigits(byte b) => (b >> 4) <= 9 && (b & 0xF) <= 9;

    private static int ToDigits(ReadOnlySpan<char> pair) => ((pair[0] - '0') << 4) | (pair[1] - '0');

    private static int FromDigits(byte b) => ((b >> 4) * 10) + (b & 0xF);
}

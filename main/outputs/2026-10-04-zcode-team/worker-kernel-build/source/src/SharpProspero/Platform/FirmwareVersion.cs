// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// The system software version the module is running on, as "MM.mm" (for example 10.01). Read
/// <see cref="Current"/> at startup to learn the firmware, then compare it to decide whether a feature
/// is available on this system.
/// </summary>
/// <remarks>
/// The two numbers are held the way the system stores them: one byte each, with the digits written out
/// as they read. Version 11.20 is the byte pair <c>0x11 0x20</c>, not <c>0x0B 0x14</c>. Reading those
/// bytes as ordinary numbers turns 11.20 into 17.32, so every conversion here goes through the digits.
/// The packed pair still orders exactly as the version does, so 10.01 correctly sorts above 09.60.
/// </remarks>
public readonly record struct FirmwareVersion : IComparable<FirmwareVersion>
{
    private FirmwareVersion(ushort packed) => Packed = packed;

    /// <summary>The absent version, held when nothing names one.</summary>
    public static FirmwareVersion None => new(0);

    /// <summary>The major and minor bytes packed into one value, major first (0x1001 for 10.01).</summary>
    public ushort Packed { get; }

    /// <summary>True when this is a real version rather than the absent one.</summary>
    public bool HasValue => Packed != 0;

    /// <summary>The major number, as written (10 for 10.01).</summary>
    public int Major => FromDigits((byte)(Packed >> 8));

    /// <summary>The minor number, as written (1 for 10.01).</summary>
    public int Minor => FromDigits((byte)(Packed & 0xFF));

    /// <summary>Builds a version from the packed pair, major in the high byte (0x1001 for 10.01).</summary>
    public static FirmwareVersion FromPacked(ushort packed) => new(packed);

    /// <summary>
    /// Builds a version from the major and minor as written, for example <c>FromMajorMinor(10, 1)</c>
    /// for 10.01. Both parts are 0..99.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">A part is outside 0..99.</exception>
    public static FirmwareVersion FromMajorMinor(int major, int minor)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(major);
        ArgumentOutOfRangeException.ThrowIfNegative(minor);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(major, 99);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(minor, 99);
        return new FirmwareVersion((ushort)((ToDigits(major) << 8) | ToDigits(minor)));
    }

    /// <summary>
    /// Builds a version from the value the kernel reports (<see cref="SystemInfo.SystemSoftwareVersionValue"/>),
    /// which carries the major and minor in the high 16 bits.
    /// </summary>
    public static FirmwareVersion FromSystemValue(uint systemValue) => new((ushort)(systemValue >> 16));

    /// <summary>
    /// The version of the system this module is running on, read from the kernel.
    /// </summary>
    /// <exception cref="ProsperoException">The version could not be read.</exception>
    public static FirmwareVersion Current => FromSystemValue(SystemInfo.SystemSoftwareVersionValue);

    /// <summary>
    /// Reads <see cref="Current"/> without throwing. Returns false and <see cref="None"/> when the
    /// version could not be read, so a diagnostic path can report the firmware when it is known and
    /// carry on when it is not.
    /// </summary>
    public static bool TryGetCurrent(out FirmwareVersion version)
    {
        try
        {
            version = Current;
            return version.HasValue;
        }
        catch (ProsperoException)
        {
            version = None;
            return false;
        }
    }

    /// <summary>
    /// Reads a version from the forms a person writes one in: "10.01" or "1001". The packed value the
    /// kernel reports is read with <see cref="FromSystemValue"/> instead.
    /// </summary>
    /// <returns>True when <paramref name="text"/> is a version this understands.</returns>
    public static bool TryParse(string? text, out FirmwareVersion version)
    {
        version = None;
        if (string.IsNullOrWhiteSpace(text))
            return false;

        string s = text.Trim();
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

        ushort packed = (ushort)((PairToDigits(digits.AsSpan(0, 2)) << 8) | PairToDigits(digits.AsSpan(2, 2)));
        if (packed == 0)
            return false;
        version = new FirmwareVersion(packed);
        return true;
    }

    /// <summary>Reads a version, and throws when the text is not one.</summary>
    /// <exception cref="FormatException"><paramref name="text"/> is not a version.</exception>
    public static FirmwareVersion Parse(string? text)
        => TryParse(text, out FirmwareVersion v)
            ? v
            : throw new FormatException($"'{text}' is not a firmware version. Use MM.mm (for example 10.01) or 1001.");

    /// <summary>True when this version is <paramref name="other"/> or newer.</summary>
    public bool IsAtLeast(FirmwareVersion other) => this >= other;

    /// <summary>The version as a person reads it, for example "10.01".</summary>
    public override string ToString() => HasValue ? $"{Major:D2}.{Minor:D2}" : "";

    /// <inheritdoc />
    // Both bytes hold their digits in order, so the packed pair orders exactly as the version does.
    public int CompareTo(FirmwareVersion other) => Packed.CompareTo(other.Packed);

    public static bool operator <(FirmwareVersion left, FirmwareVersion right) => left.CompareTo(right) < 0;
    public static bool operator >(FirmwareVersion left, FirmwareVersion right) => left.CompareTo(right) > 0;
    public static bool operator <=(FirmwareVersion left, FirmwareVersion right) => left.CompareTo(right) <= 0;
    public static bool operator >=(FirmwareVersion left, FirmwareVersion right) => left.CompareTo(right) >= 0;

    private static int ToDigits(int value) => ((value / 10) << 4) | (value % 10);

    private static int PairToDigits(ReadOnlySpan<char> pair) => ((pair[0] - '0') << 4) | (pair[1] - '0');

    private static int FromDigits(byte b) => ((b >> 4) * 10) + (b & 0xF);
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Platform;

/// <summary>
/// A span of system software versions, from a lowest version up to a highest one. An open-ended range
/// (its <see cref="Maximum"/> is <see cref="FirmwareVersion.None"/>) has no upper bound, so it covers
/// the lowest version and everything after it — the usual case, because a version surface stays
/// available on later systems.
/// </summary>
/// <param name="Minimum">The lowest version in the range.</param>
/// <param name="Maximum">The highest version, or <see cref="FirmwareVersion.None"/> for no upper bound.</param>
public readonly record struct FirmwareRange(FirmwareVersion Minimum, FirmwareVersion Maximum)
{
    /// <summary>True when the range has no upper bound, so it covers the minimum and everything after.</summary>
    public bool IsOpenEnded => !Maximum.HasValue;

    /// <summary>
    /// Reports whether <paramref name="version"/> falls within the range. The absent version is never
    /// in range.
    /// </summary>
    public bool Contains(FirmwareVersion version)
        => version.HasValue && version >= Minimum && (IsOpenEnded || version <= Maximum);

    /// <summary>The range as a person reads it, for example "10.01 and later" or "02.00 to 11.20".</summary>
    public override string ToString()
        => IsOpenEnded ? $"{Minimum} and later" : $"{Minimum} to {Maximum}";
}

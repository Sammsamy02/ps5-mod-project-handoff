// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Globalization;

namespace SharpProspero.Platform;

/// <summary>
/// A temperature. The platform reports thermal-zone temperatures as a whole number of tenths of a
/// kelvin, which is what <see cref="DeciKelvin"/> keeps; every other property is derived from it, so no
/// precision is lost by holding a reading in this type.
/// </summary>
public readonly struct Temperature : IEquatable<Temperature>, IComparable<Temperature>
{
    /// <summary>Absolute zero in degrees Celsius.</summary>
    public const double AbsoluteZeroCelsius = -273.15;

    private readonly int _deciKelvin;

    private Temperature(int deciKelvin) => _deciKelvin = deciKelvin;

    /// <summary>Builds a reading from a raw value in tenths of a kelvin.</summary>
    public static Temperature FromDeciKelvin(int deciKelvin) => new(deciKelvin);

    /// <summary>Builds a reading from kelvin, rounded to the tenth the platform works in.</summary>
    public static Temperature FromKelvin(double kelvin) => new((int)Math.Round(kelvin * 10.0));

    /// <summary>Builds a reading from degrees Celsius, rounded to the tenth of a kelvin the platform works in.</summary>
    public static Temperature FromCelsius(double celsius) => FromKelvin(celsius - AbsoluteZeroCelsius);

    /// <summary>The reading as the platform holds it: tenths of a kelvin.</summary>
    public int DeciKelvin => _deciKelvin;

    /// <summary>The reading in kelvin.</summary>
    public double Kelvin => _deciKelvin / 10.0;

    /// <summary>The reading in degrees Celsius.</summary>
    public double Celsius => (_deciKelvin / 10.0) + AbsoluteZeroCelsius;

    /// <summary>The reading in degrees Fahrenheit.</summary>
    public double Fahrenheit => (Celsius * 9.0 / 5.0) + 32.0;

    /// <summary>True when this reading equals <paramref name="other"/> to the tenth of a kelvin.</summary>
    public bool Equals(Temperature other) => _deciKelvin == other._deciKelvin;

    /// <inheritdoc/>
    public override bool Equals(object? obj) => obj is Temperature other && Equals(other);

    /// <inheritdoc/>
    public override int GetHashCode() => _deciKelvin;

    /// <summary>Orders readings from coldest to hottest.</summary>
    public int CompareTo(Temperature other) => _deciKelvin.CompareTo(other._deciKelvin);

    /// <summary>True when the two readings are the same.</summary>
    public static bool operator ==(Temperature left, Temperature right) => left.Equals(right);

    /// <summary>True when the two readings differ.</summary>
    public static bool operator !=(Temperature left, Temperature right) => !left.Equals(right);

    /// <summary>True when <paramref name="left"/> is colder than <paramref name="right"/>.</summary>
    public static bool operator <(Temperature left, Temperature right) => left.CompareTo(right) < 0;

    /// <summary>True when <paramref name="left"/> is hotter than <paramref name="right"/>.</summary>
    public static bool operator >(Temperature left, Temperature right) => left.CompareTo(right) > 0;

    /// <summary>True when <paramref name="left"/> is no hotter than <paramref name="right"/>.</summary>
    public static bool operator <=(Temperature left, Temperature right) => left.CompareTo(right) <= 0;

    /// <summary>True when <paramref name="left"/> is no colder than <paramref name="right"/>.</summary>
    public static bool operator >=(Temperature left, Temperature right) => left.CompareTo(right) >= 0;

    /// <summary>The reading in degrees Celsius to one decimal place, for showing to a user.</summary>
    public override string ToString()
        => Celsius.ToString("0.0", CultureInfo.InvariantCulture) + " C";
}

/// <summary>
/// Which of a thermal zone's temperature thresholds the last poll found crossed. A zone whose platform
/// firmware does not define a threshold never raises the matching flag.
/// </summary>
[Flags]
public enum ThermalAlarms
{
    /// <summary>The zone is below every threshold it defines.</summary>
    None = 0,

    /// <summary>
    /// The zone reached its passive-cooling threshold, so the system is slowing the processor down to
    /// bring the temperature back.
    /// </summary>
    PassiveThresholdReached = 0x1,

    /// <summary>The zone reached its "too hot" threshold, at which the system suspends.</summary>
    HotThresholdReached = 0x4,

    /// <summary>The zone reached its critical threshold, at which the system shuts down.</summary>
    CriticalThresholdReached = 0x8,
}

/// <summary>
/// Everything one thermal zone reports, gathered in a single read so the parts belong to the same
/// moment.
/// </summary>
/// <param name="Zone">The zone number these readings came from.</param>
/// <param name="Temperature">The temperature recorded by the zone's last poll.</param>
/// <param name="Alarms">Which thresholds that poll found crossed.</param>
/// <param name="ActiveCoolingLevel">
/// The active-cooling step in effect, indexing <paramref name="ActiveTripPoints"/>, or a negative
/// number when no step is engaged.
/// </param>
/// <param name="PassiveCoolingEnabled">Whether the zone is allowed to slow the processor down.</param>
/// <param name="PassiveTripPoint">The temperature at which passive cooling starts, or null when the zone does not define one.</param>
/// <param name="HotTripPoint">The temperature at which the system suspends, or null when the zone does not define one.</param>
/// <param name="CriticalTripPoint">The temperature at which the system shuts down, or null when the zone does not define one.</param>
/// <param name="ActiveTripPoints">
/// The temperature at which each active-cooling step engages, coldest step last; an entry is null where
/// the zone does not define that step.
/// </param>
public readonly record struct ThermalZoneReading(
    int Zone,
    Temperature Temperature,
    ThermalAlarms Alarms,
    int ActiveCoolingLevel,
    bool PassiveCoolingEnabled,
    Temperature? PassiveTripPoint,
    Temperature? HotTripPoint,
    Temperature? CriticalTripPoint,
    IReadOnlyList<Temperature?> ActiveTripPoints);

/// <summary>
/// Reads what the running system will tell an application about its own temperature and cooling.
/// </summary>
/// <remarks>
/// <para>
/// The readings come from the platform's thermal zones, published as named system values under
/// <c>hw.acpi.thermal</c> and read through <see cref="Sysctl"/>. A zone reports the temperature its
/// driver recorded at the last poll - not a fresh sample - so reading faster than
/// <see cref="TryReadPollingIntervalSeconds"/> returns the same number again.
/// </para>
/// <para>
/// Whether any zone exists at all is decided by the platform firmware of the machine the application is
/// running on, and a retail machine may declare none. Every reading therefore has a <c>Try</c> form
/// that reports false rather than throwing, and <see cref="EnumerateThermalZones"/> returns an empty
/// list rather than failing. Call it first and read nothing if it comes back empty.
/// </para>
/// <para>
/// Fan duty, fan speed and electrical measurements are not among these readings. The system publishes
/// no named value for any of them and no library an application links exports one, so there is nothing
/// an application can ask. The nearest reachable substitute is
/// <see cref="TryReadActiveCoolingLevel"/>, which names the cooling step the zone has engaged.
/// </para>
/// </remarks>
public static class SystemTelemetry
{
    /// <summary>The name of the tree the thermal zones are published under.</summary>
    public const string ThermalRoot = "hw.acpi.thermal";

    /// <summary>The number of active-cooling steps a zone reports trip points for.</summary>
    public const int ActiveTripPointCount = 10;

    /// <summary>The value a zone reports for a trip point its platform firmware does not define.</summary>
    public const int UndefinedTripPoint = -1;

    /// <summary>The highest zone number <see cref="EnumerateThermalZones"/> looks for.</summary>
    public const int HighestThermalZone = 15;

    /// <summary>
    /// Builds the name a thermal zone publishes one of its values under, for reading something through
    /// <see cref="Sysctl"/> that has no method of its own here.
    /// </summary>
    /// <param name="zone">The zone number.</param>
    /// <param name="leaf">The name of the value within the zone, for example <c>temperature</c>.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ArgumentException"><paramref name="leaf"/> is empty.</exception>
    public static string ThermalZoneNode(int zone, string leaf)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(zone);
        ArgumentException.ThrowIfNullOrEmpty(leaf);
        return string.Concat(ThermalRoot, ".tz", zone.ToString(CultureInfo.InvariantCulture), ".", leaf);
    }

    /// <summary>
    /// The zones this machine publishes, in order. Zones are numbered from zero and the search stops at
    /// the first number that answers nothing, or at <see cref="HighestThermalZone"/>. An empty list
    /// means the machine declares no thermal zone and none of the other readings will answer.
    /// </summary>
    public static IReadOnlyList<int> EnumerateThermalZones()
    {
        var zones = new List<int>();
        for (int zone = 0; zone <= HighestThermalZone; zone++)
        {
            if (!Sysctl.Exists(ThermalZoneNode(zone, "temperature")))
                break;
            zones.Add(zone);
        }
        return zones;
    }

    /// <summary>
    /// The temperature zone <paramref name="zone"/> recorded at its last poll.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static Temperature ReadTemperature(int zone)
        => TryReadTemperature(zone, out Temperature value) ? value : throw Refused(zone, "temperature");

    /// <summary>
    /// Reads the temperature zone <paramref name="zone"/> recorded at its last poll. Returns false when
    /// the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadTemperature(int zone, out Temperature temperature)
    {
        bool ok = Sysctl.TryReadInt32(ThermalZoneNode(zone, "temperature"), out int value);
        temperature = Temperature.FromDeciKelvin(ok ? value : 0);
        return ok;
    }

    /// <summary>
    /// Which of zone <paramref name="zone"/>'s thresholds its last poll found crossed.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static ThermalAlarms ReadAlarms(int zone)
        => TryReadAlarms(zone, out ThermalAlarms value) ? value : throw Refused(zone, "thermal_flags");

    /// <summary>
    /// Reads which of zone <paramref name="zone"/>'s thresholds its last poll found crossed. Returns
    /// false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadAlarms(int zone, out ThermalAlarms alarms)
    {
        bool ok = Sysctl.TryReadInt32(ThermalZoneNode(zone, "thermal_flags"), out int value);
        alarms = ok ? DecodeAlarms(value) : ThermalAlarms.None;
        return ok;
    }

    /// <summary>
    /// The active-cooling step zone <paramref name="zone"/> has engaged. A value from zero to
    /// <see cref="ActiveTripPointCount"/> minus one indexes the zone's active trip points; a negative
    /// value means no step is engaged, either because the zone is cool enough or because the driver has
    /// not decided yet.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static int ReadActiveCoolingLevel(int zone)
        => TryReadActiveCoolingLevel(zone, out int value) ? value : throw Refused(zone, "active");

    /// <summary>
    /// Reads the active-cooling step zone <paramref name="zone"/> has engaged into
    /// <paramref name="level"/>. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadActiveCoolingLevel(int zone, out int level)
        => Sysctl.TryReadInt32(ThermalZoneNode(zone, "active"), out level);

    /// <summary>
    /// Whether zone <paramref name="zone"/> is allowed to cool by slowing the processor down.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static bool ReadPassiveCoolingEnabled(int zone)
        => TryReadPassiveCoolingEnabled(zone, out bool value) ? value : throw Refused(zone, "passive_cooling");

    /// <summary>
    /// Reads whether zone <paramref name="zone"/> is allowed to cool by slowing the processor down.
    /// Returns false when the machine will not answer, which is not the same as the zone answering that
    /// passive cooling is off.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadPassiveCoolingEnabled(int zone, out bool enabled)
    {
        bool ok = Sysctl.TryReadInt32(ThermalZoneNode(zone, "passive_cooling"), out int value);
        enabled = ok && value != 0;
        return ok;
    }

    /// <summary>
    /// The temperature at which zone <paramref name="zone"/> starts cooling by slowing the processor
    /// down, or null when the zone does not define one.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static Temperature? ReadPassiveTripPoint(int zone)
        => TryReadPassiveTripPoint(zone, out Temperature? value) ? value : throw Refused(zone, "_PSV");

    /// <summary>
    /// Reads the temperature at which zone <paramref name="zone"/> starts cooling by slowing the
    /// processor down. Returns false when the machine will not answer; sets
    /// <paramref name="tripPoint"/> to null when the zone answers that it defines no such threshold.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadPassiveTripPoint(int zone, out Temperature? tripPoint)
        => TryReadTripPoint(zone, "_PSV", out tripPoint);

    /// <summary>
    /// The temperature at which the system suspends because zone <paramref name="zone"/> is too hot, or
    /// null when the zone does not define one.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static Temperature? ReadHotTripPoint(int zone)
        => TryReadHotTripPoint(zone, out Temperature? value) ? value : throw Refused(zone, "_HOT");

    /// <summary>
    /// Reads the temperature at which the system suspends because zone <paramref name="zone"/> is too
    /// hot. Returns false when the machine will not answer; sets <paramref name="tripPoint"/> to null
    /// when the zone answers that it defines no such threshold.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadHotTripPoint(int zone, out Temperature? tripPoint)
        => TryReadTripPoint(zone, "_HOT", out tripPoint);

    /// <summary>
    /// The temperature at which the system shuts down because zone <paramref name="zone"/> is too hot,
    /// or null when the zone does not define one.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static Temperature? ReadCriticalTripPoint(int zone)
        => TryReadCriticalTripPoint(zone, out Temperature? value) ? value : throw Refused(zone, "_CRT");

    /// <summary>
    /// Reads the temperature at which the system shuts down because zone <paramref name="zone"/> is too
    /// hot. Returns false when the machine will not answer; sets <paramref name="tripPoint"/> to null
    /// when the zone answers that it defines no such threshold.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadCriticalTripPoint(int zone, out Temperature? tripPoint)
        => TryReadTripPoint(zone, "_CRT", out tripPoint);

    /// <summary>
    /// The temperature at which each of zone <paramref name="zone"/>'s active-cooling steps engages.
    /// The list is <see cref="ActiveTripPointCount"/> long and indexed the same way
    /// <see cref="ReadActiveCoolingLevel"/> counts; an entry is null where the zone defines no such
    /// step.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static IReadOnlyList<Temperature?> ReadActiveTripPoints(int zone)
        => TryReadActiveTripPoints(zone, out IReadOnlyList<Temperature?> value) ? value : throw Refused(zone, "_ACx");

    /// <summary>
    /// Reads the temperature at which each of zone <paramref name="zone"/>'s active-cooling steps
    /// engages. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadActiveTripPoints(int zone, out IReadOnlyList<Temperature?> tripPoints)
    {
        tripPoints = [];
        if (!Sysctl.TryReadRaw(ThermalZoneNode(zone, "_ACx"), out byte[] raw))
            return false;
        if (raw.Length % sizeof(int) != 0)
            return false;
        tripPoints = DecodeTripPoints(raw);
        return true;
    }

    /// <summary>
    /// The first of the two constants zone <paramref name="zone"/> applies when it works out how hard
    /// to slow the processor down. The zone reports it as a plain number with no unit.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static int ReadPassiveCoolingConstant1(int zone)
        => TryReadPassiveCoolingConstant1(zone, out int value) ? value : throw Refused(zone, "_TC1");

    /// <summary>
    /// Reads the first of the two constants zone <paramref name="zone"/> applies when it works out how
    /// hard to slow the processor down. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadPassiveCoolingConstant1(int zone, out int value)
        => Sysctl.TryReadInt32(ThermalZoneNode(zone, "_TC1"), out value);

    /// <summary>
    /// The second of the two constants zone <paramref name="zone"/> applies when it works out how hard
    /// to slow the processor down. The zone reports it as a plain number with no unit.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static int ReadPassiveCoolingConstant2(int zone)
        => TryReadPassiveCoolingConstant2(zone, out int value) ? value : throw Refused(zone, "_TC2");

    /// <summary>
    /// Reads the second of the two constants zone <paramref name="zone"/> applies when it works out how
    /// hard to slow the processor down. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadPassiveCoolingConstant2(int zone, out int value)
        => Sysctl.TryReadInt32(ThermalZoneNode(zone, "_TC2"), out value);

    /// <summary>
    /// How often, in tenths of a second, zone <paramref name="zone"/> samples itself while it is
    /// cooling by slowing the processor down.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static int ReadPassiveSamplingPeriodDeciseconds(int zone)
        => TryReadPassiveSamplingPeriodDeciseconds(zone, out int value) ? value : throw Refused(zone, "_TSP");

    /// <summary>
    /// Reads how often, in tenths of a second, zone <paramref name="zone"/> samples itself while it is
    /// cooling by slowing the processor down. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadPassiveSamplingPeriodDeciseconds(int zone, out int value)
        => Sysctl.TryReadInt32(ThermalZoneNode(zone, "_TSP"), out value);

    /// <summary>
    /// Every reading zone <paramref name="zone"/> offers, taken together.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    /// <exception cref="ProsperoException">The zone did not answer.</exception>
    public static ThermalZoneReading ReadZone(int zone)
        => TryReadZone(zone, out ThermalZoneReading value) ? value : throw Refused(zone, "temperature");

    /// <summary>
    /// Reads everything zone <paramref name="zone"/> offers. Returns false when the temperature itself
    /// will not answer; the trip points and the cooling state fall back to their unset values when only
    /// those are missing, because a zone that reports a temperature is worth returning even when it
    /// defines no thresholds.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="zone"/> is negative.</exception>
    public static bool TryReadZone(int zone, out ThermalZoneReading reading)
    {
        // The trip-point list is part of the returned value either way, so it starts empty rather than
        // null and a caller that ignores the result still gets something it can walk.
        reading = new ThermalZoneReading(
            zone, default, ThermalAlarms.None, UndefinedTripPoint, false, null, null, null, []);
        if (!TryReadTemperature(zone, out Temperature temperature))
            return false;

        TryReadAlarms(zone, out ThermalAlarms alarms);
        if (!TryReadActiveCoolingLevel(zone, out int level))
            level = UndefinedTripPoint;
        TryReadPassiveCoolingEnabled(zone, out bool passive);
        TryReadPassiveTripPoint(zone, out Temperature? passiveTrip);
        TryReadHotTripPoint(zone, out Temperature? hotTrip);
        TryReadCriticalTripPoint(zone, out Temperature? criticalTrip);
        TryReadActiveTripPoints(zone, out IReadOnlyList<Temperature?> activeTrips);

        reading = new ThermalZoneReading(
            zone, temperature, alarms, level, passive, passiveTrip, hotTrip, criticalTrip, activeTrips);
        return true;
    }

    /// <summary>
    /// How often, in seconds, the driver polls the thermal zones. Reading a zone more often than this
    /// returns the same recorded temperature again.
    /// </summary>
    /// <exception cref="ProsperoException">The machine did not answer.</exception>
    public static int ReadPollingIntervalSeconds()
        => TryReadPollingIntervalSeconds(out int value) ? value : throw Refused(ThermalRoot + ".polling_rate");

    /// <summary>
    /// Reads how often, in seconds, the driver polls the thermal zones. Returns false when the machine
    /// will not answer.
    /// </summary>
    public static bool TryReadPollingIntervalSeconds(out int seconds)
        => Sysctl.TryReadInt32(ThermalRoot + ".polling_rate", out seconds);

    /// <summary>
    /// The shortest time, in seconds, the driver keeps a cooling step engaged before it drops back.
    /// </summary>
    /// <exception cref="ProsperoException">The machine did not answer.</exception>
    public static int ReadMinimumCoolingRuntimeSeconds()
        => TryReadMinimumCoolingRuntimeSeconds(out int value) ? value : throw Refused(ThermalRoot + ".min_runtime");

    /// <summary>
    /// Reads the shortest time, in seconds, the driver keeps a cooling step engaged before it drops
    /// back. Returns false when the machine will not answer.
    /// </summary>
    public static bool TryReadMinimumCoolingRuntimeSeconds(out int seconds)
        => Sysctl.TryReadInt32(ThermalRoot + ".min_runtime", out seconds);

    /// <summary>
    /// Whether the driver accepts changes to the thresholds the platform firmware set. An application
    /// cannot change them either way; this reports how the running system is configured.
    /// </summary>
    /// <exception cref="ProsperoException">The machine did not answer.</exception>
    public static bool ReadSettingOverrideAllowed()
        => TryReadSettingOverrideAllowed(out bool value) ? value : throw Refused(ThermalRoot + ".user_override");

    /// <summary>
    /// Reads whether the driver accepts changes to the thresholds the platform firmware set. Returns
    /// false when the machine will not answer, which is not the same as the machine answering that
    /// changes are refused.
    /// </summary>
    public static bool TryReadSettingOverrideAllowed(out bool allowed)
    {
        bool ok = Sysctl.TryReadInt32(ThermalRoot + ".user_override", out int value);
        allowed = ok && value != 0;
        return ok;
    }

    /// <summary>
    /// Turns the flag word a zone reports into the thresholds it stands for. Bits the zone does not
    /// define are dropped.
    /// </summary>
    public static ThermalAlarms DecodeAlarms(int flags)
    {
        const int Known = (int)(ThermalAlarms.PassiveThresholdReached
            | ThermalAlarms.HotThresholdReached
            | ThermalAlarms.CriticalThresholdReached);
        return (ThermalAlarms)(flags & Known);
    }

    /// <summary>
    /// Turns the block of trip points a zone reports into readings, one per active-cooling step. Each
    /// entry is four bytes, least significant first, and an entry holding
    /// <see cref="UndefinedTripPoint"/> becomes null.
    /// </summary>
    /// <exception cref="ArgumentException">
    /// <paramref name="block"/> is not a whole number of four-byte entries.
    /// </exception>
    public static IReadOnlyList<Temperature?> DecodeTripPoints(ReadOnlySpan<byte> block)
    {
        if (block.Length % sizeof(int) != 0)
            throw new ArgumentException("A trip point block is a whole number of four-byte entries.", nameof(block));

        var points = new Temperature?[block.Length / sizeof(int)];
        for (int i = 0; i < points.Length; i++)
        {
            int value = BinaryPrimitives.ReadInt32LittleEndian(block[(i * sizeof(int))..]);
            points[i] = value == UndefinedTripPoint ? null : Temperature.FromDeciKelvin(value);
        }
        return points;
    }

    private static bool TryReadTripPoint(int zone, string leaf, out Temperature? tripPoint)
    {
        tripPoint = null;
        if (!Sysctl.TryReadInt32(ThermalZoneNode(zone, leaf), out int value))
            return false;
        if (value != UndefinedTripPoint)
            tripPoint = Temperature.FromDeciKelvin(value);
        return true;
    }

    private static ProsperoException Refused(int zone, string leaf)
        => Refused(ThermalZoneNode(zone, leaf));

    private static ProsperoException Refused(string name)
        => new(name, SceResult.KernelFacility | (Sysctl.LastErrorNumber & 0xFFFF));
}

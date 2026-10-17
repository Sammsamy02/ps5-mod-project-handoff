// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Rtc;
using System;

namespace SharpProspero.Timing;

/// <summary>
/// The wall-clock date and time from the real-time clock, as a <see cref="DateTime"/>. Use this for
/// the calendar date and time; use <see cref="GameClock"/> for a monotonic counter to pace and measure
/// frames. Unlike the game clock, these readings follow the system clock and can jump when it changes.
/// </summary>
public static class SystemClock
{
    /// <summary>The current UTC date and time.</summary>
    public static DateTime UtcNow => Read(utc: true);

    /// <summary>The current date and time in the system's local time zone.</summary>
    public static DateTime LocalNow => Read(utc: false);

    private static unsafe DateTime Read(bool utc)
    {
        SceRtcDateTime time;
        int rc = utc ? Rtc.sceRtcGetCurrentClock(&time, 0) : Rtc.sceRtcGetCurrentClockLocalTime(&time);
        SceResult.ThrowIfFailed(rc, utc ? nameof(Rtc.sceRtcGetCurrentClock) : nameof(Rtc.sceRtcGetCurrentClockLocalTime));
        return ToDateTime(time, utc ? DateTimeKind.Utc : DateTimeKind.Local);
    }

    /// <summary>
    /// Converts calendar fields into a <see cref="DateTime"/> of the given kind, carrying the
    /// microsecond remainder (each microsecond is ten <see cref="DateTime"/> ticks).
    /// </summary>
    public static DateTime ToDateTime(SceRtcDateTime time, DateTimeKind kind)
    {
        var value = new DateTime(time.Year, time.Month, time.Day, time.Hour, time.Minute, time.Second, kind);
        return value.AddTicks(time.Microsecond * 10L);
    }
}

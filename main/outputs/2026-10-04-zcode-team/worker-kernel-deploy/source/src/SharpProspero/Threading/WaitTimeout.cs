// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Threading;

/// <summary>
/// Turns a <see cref="TimeSpan"/> into the microsecond count the waiting calls take. The platform
/// counts in whole microseconds in an unsigned 32-bit field, which caps a wait at a little over
/// seventy-one minutes; a longer wait has to be built out of repeated shorter ones.
/// </summary>
public static class WaitTimeout
{
    /// <summary>The longest wait a single call can express, a shade over 71 minutes.</summary>
    public static readonly TimeSpan Maximum = TimeSpan.FromTicks(uint.MaxValue * (TimeSpan.TicksPerMillisecond / 1000));

    /// <summary>
    /// The microsecond count for <paramref name="timeout"/>. A remainder below a microsecond rounds up
    /// to one rather than down to zero, because zero means "do not wait at all" and a caller asking for
    /// a very short wait does not mean that.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">
    /// <paramref name="timeout"/> is negative, or longer than <see cref="Maximum"/>.
    /// </exception>
    public static uint ToMicroseconds(TimeSpan timeout)
    {
        if (timeout < TimeSpan.Zero)
            throw new ArgumentOutOfRangeException(nameof(timeout), timeout, "A wait cannot be negative.");
        if (timeout > Maximum)
            throw new ArgumentOutOfRangeException(nameof(timeout), timeout,
                $"A single wait cannot exceed {Maximum}. Repeat a shorter wait instead.");

        const long TicksPerMicrosecond = TimeSpan.TicksPerMillisecond / 1000;
        long ticks = timeout.Ticks;
        long microseconds = ticks / TicksPerMicrosecond;
        if (ticks % TicksPerMicrosecond != 0)
            microseconds++;
        return (uint)microseconds;
    }
}

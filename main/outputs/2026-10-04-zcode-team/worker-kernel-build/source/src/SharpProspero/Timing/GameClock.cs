// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Kernel;
using System;

namespace SharpProspero.Timing;

/// <summary>
/// A monotonic clock backed by the process-time counter. Create one to measure elapsed time from a
/// fixed origin, or use the static members for one-off readings and sleeping. Readings never move
/// backward and are not affected by wall-clock changes.
/// </summary>
public sealed class GameClock
{
    private long _originMicros;

    /// <summary>Starts a clock whose origin is the moment of construction.</summary>
    public GameClock() => _originMicros = KernelClock.sceKernelGetProcessTime();

    /// <summary>Microseconds elapsed since the clock's origin.</summary>
    public long ElapsedMicroseconds => KernelClock.sceKernelGetProcessTime() - _originMicros;

    /// <summary>Seconds elapsed since the clock's origin.</summary>
    public double ElapsedSeconds => ElapsedMicroseconds / 1_000_000.0;

    /// <summary>Moves the origin to now, so elapsed time counts from this call.</summary>
    public void Restart() => _originMicros = KernelClock.sceKernelGetProcessTime();

    /// <summary>Microseconds since the process started.</summary>
    public static long ProcessMicroseconds => KernelClock.sceKernelGetProcessTime();

    /// <summary>Suspends the caller for <paramref name="duration"/>, rounded to whole microseconds.</summary>
    public static void Sleep(TimeSpan duration)
    {
        long micros = (long)(duration.TotalMilliseconds * 1000.0);
        while (micros > 0)
        {
            uint chunk = micros > uint.MaxValue ? uint.MaxValue : (uint)micros;
            KernelClock.sceKernelUsleep(chunk);
            micros -= chunk;
        }
    }
}

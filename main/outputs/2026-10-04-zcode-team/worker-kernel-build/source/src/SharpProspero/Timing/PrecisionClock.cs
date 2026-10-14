// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;

namespace SharpProspero.Timing;

/// <summary>
/// The finest monotonic clock the machine offers, for pacing a frame, measuring a block of work, or
/// deciding how long an emulated instruction may take. <see cref="GameClock"/> counts whole
/// microseconds; this counts in the hardware's own units, which are far smaller, and it never moves
/// backward or jumps when the wall clock is set.
/// </summary>
/// <example>
/// Measure a piece of work:
/// <code>
/// ulong start = PrecisionClock.Ticks;
/// Simulate();
/// double milliseconds = PrecisionClock.ElapsedSince(start).TotalMilliseconds;
/// </code>
/// </example>
public static unsafe class PrecisionClock
{
    private static ulong _frequency;

    /// <summary>
    /// The counter's reading now. The origin is the moment the process started, so only differences
    /// between readings mean anything.
    /// </summary>
    public static ulong Ticks => KernelClock.sceKernelGetProcessTimeCounter();

    /// <summary>
    /// How many <see cref="Ticks"/> make one second. Read once from the machine and kept, because it
    /// does not change while a process runs.
    /// </summary>
    public static ulong Frequency
    {
        get
        {
            ulong cached = _frequency;
            if (cached == 0)
            {
                cached = KernelClock.sceKernelGetProcessTimeCounterFrequency();
                _frequency = cached;
            }
            return cached;
        }
    }

    /// <summary>The processor cycle counter, and how many of its units make one second.</summary>
    /// <remarks>
    /// This is the counter the processor itself keeps. It is finer than <see cref="Ticks"/> but it is
    /// per-processor, so a thread free to move between processors can read two values that do not
    /// compare. Pin the thread before timing anything with it.
    /// </remarks>
    public static ulong CycleCounter => KernelClock.sceKernelReadTsc();

    /// <summary>How many <see cref="CycleCounter"/> units make one second.</summary>
    public static ulong CycleCounterFrequency => KernelClock.sceKernelGetTscFrequency();

    /// <summary>The time between <paramref name="startTicks"/> and now.</summary>
    public static TimeSpan ElapsedSince(ulong startTicks) => ToTimeSpan(Ticks - startTicks, Frequency);

    /// <summary>The seconds between <paramref name="startTicks"/> and now.</summary>
    public static double SecondsSince(ulong startTicks) => (Ticks - startTicks) / (double)Frequency;

    /// <summary>
    /// Converts a count of counter units into a span, given how many of them make a second.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="frequency"/> is zero.</exception>
    public static TimeSpan ToTimeSpan(ulong elapsedTicks, ulong frequency)
    {
        ArgumentOutOfRangeException.ThrowIfZero(frequency);
        // Scale before dividing so a counter finer than a tick does not round to nothing, and split the
        // whole seconds off first so a long measurement cannot overflow the multiplication.
        ulong seconds = elapsedTicks / frequency;
        ulong remainder = elapsedTicks - (seconds * frequency);
        long ticks = (long)(seconds * (ulong)TimeSpan.TicksPerSecond)
                   + (long)(remainder * (ulong)TimeSpan.TicksPerSecond / frequency);
        return new TimeSpan(ticks);
    }

    /// <summary>
    /// Converts a count of counter units into whole microseconds, given how many of them make a second.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="frequency"/> is zero.</exception>
    public static long ToMicroseconds(ulong elapsedTicks, ulong frequency)
    {
        ArgumentOutOfRangeException.ThrowIfZero(frequency);
        ulong seconds = elapsedTicks / frequency;
        ulong remainder = elapsedTicks - (seconds * frequency);
        return (long)(seconds * 1_000_000UL) + (long)(remainder * 1_000_000UL / frequency);
    }

    /// <summary>
    /// How many counter units stand for <paramref name="duration"/>, given how many make a second.
    /// Negative durations count as none.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="frequency"/> is zero.</exception>
    public static ulong FromTimeSpan(TimeSpan duration, ulong frequency)
    {
        ArgumentOutOfRangeException.ThrowIfZero(frequency);
        if (duration <= TimeSpan.Zero)
            return 0;
        ulong wholeSeconds = (ulong)(duration.Ticks / TimeSpan.TicksPerSecond);
        ulong remainder = (ulong)(duration.Ticks % TimeSpan.TicksPerSecond);
        return (wholeSeconds * frequency) + (remainder * frequency / (ulong)TimeSpan.TicksPerSecond);
    }

    /// <summary>
    /// The smallest step the monotonic clock can report, which is the floor on how precise a
    /// <see cref="Sleep(TimeSpan)"/> can be.
    /// </summary>
    /// <exception cref="ProsperoException">The resolution could not be read.</exception>
    public static TimeSpan Resolution
    {
        get
        {
            KernelTimespec step;
            SceResult.ThrowIfFailed(
                KernelClock.sceKernelClockGetres(KernelClock.ClockMonotonic, &step),
                nameof(KernelClock.sceKernelClockGetres));
            return TimeSpan.FromTicks((step.Seconds * TimeSpan.TicksPerSecond) + (step.Nanoseconds / 100));
        }
    }

    /// <summary>
    /// Suspends the caller for <paramref name="duration"/> to nanosecond precision. The machine may
    /// sleep a little longer than asked; it never returns early of its own accord.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="duration"/> is negative.</exception>
    public static void Sleep(TimeSpan duration)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(duration, TimeSpan.Zero);
        SleepNanoseconds(duration.Ticks * 100L);
    }

    /// <summary>Suspends the caller for <paramref name="nanoseconds"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="nanoseconds"/> is negative.</exception>
    public static void SleepNanoseconds(long nanoseconds)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(nanoseconds);
        if (nanoseconds == 0)
            return;

        var requested = new KernelTimespec
        {
            Seconds = nanoseconds / 1_000_000_000L,
            Nanoseconds = nanoseconds % 1_000_000_000L,
        };
        // A wait can be cut short by something the process is told about, and the call then reports how
        // much of the interval was left. Asking again for the remainder is what makes the whole interval
        // elapse rather than part of it.
        //
        // The remainder is written only when the wait was cut short that way; every other refusal leaves
        // it as it was. It is cleared before each call and the loop turns only on that one reason, so a
        // refusal for any other reason ends the wait instead of asking again for whatever the stack
        // happened to hold - which is either an interval of no length or one of an arbitrary one.
        while (true)
        {
            KernelTimespec remaining = default;
            int rc = KernelClock.sceKernelNanosleep(&requested, &remaining);
            if (!ShouldSleepAgain(rc, remaining))
                return;
            requested = remaining;
        }
    }

    /// <summary>
    /// The code the sleep reports when it was cut short by something the process was told about rather
    /// than by the interval running out. Value 0x80020004.
    /// </summary>
    internal const int InterruptedError = unchecked((int)0x80020004);

    // Whether a sleep that came back early should be asked for again with what was left. Only the
    // interrupt writes a remainder; every other answer leaves it as the caller had it, so asking again
    // on one of those would sleep for whatever the stack held rather than for the rest of the interval.
    internal static bool ShouldSleepAgain(int resultCode, KernelTimespec remaining)
        => resultCode == InterruptedError && (remaining.Seconds > 0 || remaining.Nanoseconds > 0);

    /// <summary>
    /// Waits until the counter reaches <paramref name="targetTicks"/>, sleeping for the bulk of the wait
    /// and then giving up the processor in short turns for the last part. This lands closer to the mark
    /// than a plain sleep, at the cost of the tail spent spinning.
    /// </summary>
    /// <param name="targetTicks">The <see cref="Ticks"/> reading to wait for.</param>
    /// <param name="spinTailMicroseconds">
    /// How much of the wait to spin through rather than sleep. Zero sleeps the whole way.
    /// </param>
    public static void WaitUntil(ulong targetTicks, long spinTailMicroseconds = 250)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(spinTailMicroseconds);
        ulong frequency = Frequency;

        ulong now = Ticks;
        if (now >= targetTicks)
            return;

        ulong tail = FromTimeSpan(TimeSpan.FromTicks(spinTailMicroseconds * 10L), frequency);
        if (targetTicks - now > tail)
        {
            long sleepNanoseconds = ToMicroseconds(targetTicks - now - tail, frequency) * 1000L;
            SleepNanoseconds(sleepNanoseconds);
        }

        // The tail: hand the processor to anything else that is ready rather than burning the core on a
        // tight read of the counter.
        while (Ticks < targetTicks)
            KernelThread.scePthreadYield();
    }

    /// <summary>
    /// Which processor the calling thread is on at this instant. A thread that has not been confined
    /// can report a different number from one call to the next.
    /// </summary>
    public static int CurrentProcessor => KernelClock.sceKernelGetCurrentCpu();
}

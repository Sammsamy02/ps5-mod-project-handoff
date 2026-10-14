// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Timing;

/// <summary>
/// Turns a variable frame delta into a fixed number of equal simulation steps, so game physics or an
/// emulated core advance the same amount regardless of the display rate. Feed it the real time since the
/// last frame; it returns how many fixed steps to run and leaves an <see cref="Alpha"/> for interpolating
/// the render between the last two steps.
/// </summary>
/// <example>
/// <code>
/// var clock = new FixedTimestep(1.0 / 60);
/// // each frame:
/// int steps = clock.Advance(context.DeltaSeconds);
/// for (int i = 0; i &lt; steps; i++)
///     Simulate(clock.Step);
/// Render(clock.Alpha); // 0..1 between the previous and current step
/// </code>
/// </example>
public sealed class FixedTimestep
{
    // A hard cap on the steps one Advance can return, so a pathologically small Step cannot spin the
    // caller's loop for a huge count or overflow the returned int. It is far above any real configuration.
    private const int MaxStepsPerAdvance = 1_000_000;

    private double _accumulator;
    private double _maxFrameTime = 0.25;

    /// <summary>Creates a fixed timestep of <paramref name="step"/> seconds (for example 1.0/60).</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="step"/> is not positive.</exception>
    public FixedTimestep(double step)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(step);
        Step = step;
    }

    /// <summary>The length of one fixed step, in seconds.</summary>
    public double Step { get; }

    /// <summary>
    /// The most real time, in seconds, to absorb in one <see cref="Advance(double)"/>. A frame slower than this
    /// (a hitch or a breakpoint) is clamped so the simulation does not try to catch up with a burst of
    /// steps — the "spiral of death". Defaults to 0.25 s and must be positive and finite.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">Set to a value that is not positive and finite.</exception>
    public double MaxFrameTime
    {
        get => _maxFrameTime;
        set
        {
            ArgumentOutOfRangeException.ThrowIfNegativeOrZero(value);
            if (!double.IsFinite(value))
                throw new ArgumentOutOfRangeException(nameof(value), "The maximum frame time must be finite.");
            _maxFrameTime = value;
        }
    }

    /// <summary>
    /// How far, from 0 to 1, the leftover time has moved toward the next step after the last
    /// <see cref="Advance(double)"/>. Use it to interpolate the rendered state between the previous and current
    /// simulation step so motion stays smooth.
    /// </summary>
    public double Alpha => _accumulator / Step;

    /// <summary>
    /// Adds <paramref name="deltaSeconds"/> of real time (clamped to <see cref="MaxFrameTime"/>) and
    /// returns how many fixed steps are now due. A negative or non-finite delta contributes nothing.
    /// </summary>
    public int Advance(double deltaSeconds)
    {
        if (double.IsFinite(deltaSeconds) && deltaSeconds > 0)
            _accumulator += Math.Min(deltaSeconds, MaxFrameTime);

        if (_accumulator < Step)
            return 0;

        // Compute the due steps in one division rather than a loop, so a tiny Step cannot spin here, and
        // cap the count so it cannot overflow an int or hand the caller an unbounded loop.
        long due = (long)(_accumulator / Step);
        int steps = (int)Math.Min(due, MaxStepsPerAdvance);
        _accumulator -= steps * Step;
        if (due > MaxStepsPerAdvance)
            _accumulator = 0; // the safety cap was hit; drop the backlog instead of carrying it forward

        return steps;
    }

    /// <summary>
    /// Adds <paramref name="deltaSeconds"/> and runs <paramref name="onStep"/> once for each fixed step
    /// now due — the callback form of <see cref="Advance(double)"/>.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="onStep"/> is null.</exception>
    public void Advance(double deltaSeconds, Action onStep)
    {
        ArgumentNullException.ThrowIfNull(onStep);
        int steps = Advance(deltaSeconds);
        for (int i = 0; i < steps; i++)
        {
            try
            {
                onStep();
            }
            catch
            {
                _accumulator += (steps - i) * Step; // return the time for the steps not run, so none is lost
                throw;
            }
        }
    }

    /// <summary>Clears the leftover time, so the next <see cref="Advance(double)"/> starts from zero.</summary>
    public void Reset() => _accumulator = 0;
}

// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Animation;

/// <summary>What a tween does when it reaches the end.</summary>
public enum TweenMode
{
    /// <summary>Stops at the end value and reports itself complete.</summary>
    Once,

    /// <summary>Jumps back to the start and runs again, without end.</summary>
    Loop,

    /// <summary>Runs to the end, then back to the start, then forward again, without end.</summary>
    PingPong,
}

/// <summary>
/// Moves a number from one value to another over a set time, along an easing curve. Advance it each
/// frame with the time since the last, and read <see cref="Value"/> to place whatever it drives — a
/// panel sliding in, a fade, a bar filling. It holds no reference to what it moves, so one tween can
/// drive a position, a colour channel, or an alpha.
/// </summary>
/// <remarks>
/// Pass the frame's delta (from <c>FrameContext.DeltaSeconds</c>) to <see cref="Update"/> each frame.
/// A <see cref="TweenMode.Once"/> tween settles on the end value and reports <see cref="IsComplete"/>;
/// a looping or ping-pong tween runs until you stop reading it.
/// </remarks>
/// <example>
/// <code>
/// var slideIn = new Tween(from: -200, to: 0, durationSeconds: 0.3f, Ease.OutCubic);
/// // each frame:
/// int x = (int)slideIn.Update((float)frame.DeltaSeconds);
/// panel.DrawAt(x, y);
/// </code>
/// </example>
public sealed class Tween
{
    private readonly float _duration;
    private float _elapsed;

    /// <summary>Creates a tween from <paramref name="from"/> to <paramref name="to"/>.</summary>
    /// <param name="from">The value at the start.</param>
    /// <param name="to">The value at the end.</param>
    /// <param name="durationSeconds">How long one run takes; must be positive.</param>
    /// <param name="ease">The curve to move along. Default a straight line.</param>
    /// <param name="mode">What happens at the end. Default stop.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="durationSeconds"/> is not positive.</exception>
    public Tween(float from, float to, float durationSeconds, Ease ease = Ease.Linear, TweenMode mode = TweenMode.Once)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(durationSeconds);
        From = from;
        To = to;
        _duration = durationSeconds;
        Ease = ease;
        Mode = mode;
    }

    /// <summary>The value at the start of a run.</summary>
    public float From { get; }

    /// <summary>The value at the end of a run.</summary>
    public float To { get; }

    /// <summary>The curve the value moves along.</summary>
    public Ease Ease { get; }

    /// <summary>What the tween does at the end of a run.</summary>
    public TweenMode Mode { get; }

    /// <summary>How far through the current run, from 0 to 1, before the curve is applied.</summary>
    public float Progress => _duration <= 0f ? 1f : PositionPhase();

    /// <summary>The current value, shaped by the easing curve.</summary>
    public float Value => From + ((To - From) * Easing.Apply(Ease, Progress));

    /// <summary>
    /// True once a <see cref="TweenMode.Once"/> tween has reached the end. A looping or ping-pong tween
    /// never completes.
    /// </summary>
    public bool IsComplete => Mode == TweenMode.Once && _elapsed >= _duration;

    /// <summary>
    /// Advances by <paramref name="deltaSeconds"/> and returns the new <see cref="Value"/>. A negative
    /// or zero delta leaves the tween where it is.
    /// </summary>
    public float Update(float deltaSeconds)
    {
        if (deltaSeconds > 0f)
        {
            _elapsed += deltaSeconds;

            // Once clamps at the end. Loop and ping-pong are periodic, so the elapsed time is folded
            // back into one period; this keeps it bounded and holds the precision of the position over
            // an animation left running for a long time.
            if (Mode == TweenMode.Once)
            {
                if (_elapsed > _duration)
                    _elapsed = _duration;
            }
            else
            {
                float period = Mode == TweenMode.PingPong ? 2f * _duration : _duration;
                if (_elapsed >= period)
                    _elapsed %= period;
            }
        }
        return Value;
    }

    /// <summary>Moves the tween back to its start so it runs again from the beginning.</summary>
    public void Restart() => _elapsed = 0f;

    // The 0..1 position within the current run, resolved for the mode. Once clamps at the end; loop
    // wraps; ping-pong folds a two-run cycle into a triangle so it runs out and back.
    private float PositionPhase()
    {
        switch (Mode)
        {
            case TweenMode.Loop:
                return (_elapsed % _duration) / _duration;

            case TweenMode.PingPong:
                float cycle = (_elapsed / _duration) % 2f;
                return cycle <= 1f ? cycle : 2f - cycle;

            default:
                float clamped = _elapsed >= _duration ? _duration : _elapsed;
                return clamped / _duration;
        }
    }
}

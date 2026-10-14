// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using System;
using System.Collections.Generic;

namespace SharpProspero.Input;

/// <summary>What the light bar is doing over time.</summary>
public enum LightBarMode
{
    /// <summary>One color that does not change.</summary>
    Solid = 0,

    /// <summary>The color breathes between a low and a high brightness.</summary>
    Pulse = 1,

    /// <summary>The color switches between two colors once per period.</summary>
    Blink = 2,

    /// <summary>The color travels from one color to another over a duration.</summary>
    Ramp = 3,

    /// <summary>A queue of timed steps runs one after another.</summary>
    Sequence = 4,
}

/// <summary>
/// One entry in a light-bar sequence: a color and how long it lasts. A holding step shows its color for
/// the whole duration; a fading step travels to its color from whatever the light bar showed when the
/// step began.
/// </summary>
public readonly struct LightBarStep
{
    /// <summary>The color this step ends on.</summary>
    public Color Color { get; init; }

    /// <summary>How long the step lasts, in seconds. Must be greater than zero.</summary>
    public float DurationSeconds { get; init; }

    /// <summary>True to travel to <see cref="Color"/> across the step, false to show it at once.</summary>
    public bool Fade { get; init; }

    /// <summary>A step that shows <paramref name="color"/> for <paramref name="seconds"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="seconds"/> is not greater than zero.</exception>
    public static LightBarStep Hold(Color color, float seconds)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(seconds);
        return new LightBarStep { Color = color, DurationSeconds = seconds, Fade = false };
    }

    /// <summary>
    /// A step that travels from the previous color to <paramref name="color"/> across
    /// <paramref name="seconds"/>.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="seconds"/> is not greater than zero.</exception>
    public static LightBarStep FadeTo(Color color, float seconds)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(seconds);
        return new LightBarStep { Color = color, DurationSeconds = seconds, Fade = true };
    }
}

/// <summary>
/// The color a light bar should show, and how that color moves from frame to frame. Nothing here touches
/// a device: pick a state, call <see cref="Update"/> once per frame with the frame's elapsed time, and
/// read <see cref="Current"/>. <see cref="LightBar"/> drives one of these and writes the result to a
/// controller.
/// </summary>
public sealed class LightBarAnimator
{
    private const float Tau = MathF.PI * 2f;

    private LightBarMode _mode = LightBarMode.Solid;
    private Color _current = Color.Black;
    private Color _from = Color.Black;
    private Color _to = Color.Black;
    private float _phase;
    private float _elapsed;
    private float _period = 1f;
    private float _duration = 1f;
    private float _dutyCycle = 0.5f;
    private float _minBrightness;
    private float _maxBrightness = 1f;
    private bool _loop;
    private bool _finished = true;

    private readonly List<LightBarStep> _steps = [];
    private int _stepIndex;
    private Color _stepStartColor = Color.Black;

    private Color _lastTaken;
    private bool _hasTaken;

    /// <summary>What the animator is doing.</summary>
    public LightBarMode Mode => _mode;

    /// <summary>The color for this frame.</summary>
    public Color Current => _current;

    /// <summary>
    /// True when nothing is left to animate: a solid color, a finished ramp, or a finished sequence. A
    /// pulse, a blink and anything looping never finish.
    /// </summary>
    public bool IsFinished => _finished;

    /// <summary>Shows <paramref name="color"/> and stops animating.</summary>
    public void Solid(Color color)
    {
        _mode = LightBarMode.Solid;
        _current = color;
        _loop = false;
        _finished = true;
        _steps.Clear();
    }

    /// <summary>Turns the light bar off, which is a solid black.</summary>
    public void Off() => Solid(Color.Black);

    /// <summary>
    /// Breathes <paramref name="color"/> between <paramref name="minBrightness"/> and
    /// <paramref name="maxBrightness"/> (each 0 to 1) once every <paramref name="periodSeconds"/>. The
    /// cycle starts at the low end, reaches the high end at half the period and returns to the low end at
    /// the end of it.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">
    /// The period is not greater than zero, a brightness lies outside 0 to 1, or the low end is above the
    /// high end.
    /// </exception>
    public void Pulse(Color color, float periodSeconds, float minBrightness = 0f, float maxBrightness = 1f)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(periodSeconds);
        ArgumentOutOfRangeException.ThrowIfNegative(minBrightness);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(maxBrightness, 1f);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(minBrightness, maxBrightness);

        _mode = LightBarMode.Pulse;
        _from = color;
        _period = periodSeconds;
        _minBrightness = minBrightness;
        _maxBrightness = maxBrightness;
        _phase = 0f;
        _loop = true;
        _finished = false;
        _steps.Clear();
        _current = Brightened(color, minBrightness);
    }

    /// <summary>
    /// Switches between <paramref name="onColor"/> and <paramref name="offColor"/> once every
    /// <paramref name="periodSeconds"/>. <paramref name="dutyCycle"/> is the share of the period the on
    /// color holds, from 0 to 1. The cycle starts on the on color.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">
    /// The period is not greater than zero, or the duty cycle lies outside 0 to 1.
    /// </exception>
    public void Blink(Color onColor, Color offColor, float periodSeconds, float dutyCycle = 0.5f)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(periodSeconds);
        ArgumentOutOfRangeException.ThrowIfNegative(dutyCycle);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(dutyCycle, 1f);

        _mode = LightBarMode.Blink;
        _from = onColor;
        _to = offColor;
        _period = periodSeconds;
        _dutyCycle = dutyCycle;
        _phase = 0f;
        _loop = true;
        _finished = false;
        _steps.Clear();
        _current = dutyCycle > 0f ? onColor : offColor;
    }

    /// <summary>
    /// Travels from <paramref name="from"/> to <paramref name="to"/> across
    /// <paramref name="durationSeconds"/>. With <paramref name="loop"/> set the ramp restarts at
    /// <paramref name="from"/>; without it the ramp holds <paramref name="to"/> and reports finished.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">The duration is not greater than zero.</exception>
    public void Ramp(Color from, Color to, float durationSeconds, bool loop = false)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(durationSeconds);

        _mode = LightBarMode.Ramp;
        _from = from;
        _to = to;
        _duration = durationSeconds;
        _elapsed = 0f;
        _loop = loop;
        _finished = false;
        _steps.Clear();
        _current = from;
    }

    /// <summary>
    /// Runs <paramref name="steps"/> in order. A fading step starts from the color showing when it began,
    /// so the first one starts from <see cref="Current"/>. With <paramref name="loop"/> set the queue
    /// restarts once it runs out.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="steps"/> is null.</exception>
    /// <exception cref="ArgumentException">The queue is empty.</exception>
    /// <exception cref="ArgumentOutOfRangeException">A step lasts zero seconds or less.</exception>
    public void Sequence(IEnumerable<LightBarStep> steps, bool loop = false)
    {
        ArgumentNullException.ThrowIfNull(steps);

        _steps.Clear();
        _steps.AddRange(steps);
        if (_steps.Count == 0)
            throw new ArgumentException("A light-bar sequence needs at least one step.", nameof(steps));

        foreach (LightBarStep step in _steps)
        {
            // A step of no length would leave the walk in Update with nothing to subtract.
            if (step.DurationSeconds <= 0f)
                throw new ArgumentOutOfRangeException(nameof(steps), "Every light-bar step must last longer than zero seconds.");
        }

        _mode = LightBarMode.Sequence;
        _stepIndex = 0;
        _elapsed = 0f;
        _loop = loop;
        _finished = false;
        _stepStartColor = _current;
        _current = _steps[0].Fade ? _stepStartColor : _steps[0].Color;
    }

    /// <summary>Advances the state by <paramref name="deltaSeconds"/> and recomputes <see cref="Current"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="deltaSeconds"/> is negative.</exception>
    public void Update(float deltaSeconds)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(deltaSeconds);

        switch (_mode)
        {
            case LightBarMode.Pulse:
                _phase = Wrap(_phase + deltaSeconds / _period);
                float level = _minBrightness
                    + (_maxBrightness - _minBrightness) * (1f - MathF.Cos(_phase * Tau)) * 0.5f;
                _current = Brightened(_from, level);
                break;

            case LightBarMode.Blink:
                _phase = Wrap(_phase + deltaSeconds / _period);
                _current = _phase < _dutyCycle ? _from : _to;
                break;

            case LightBarMode.Ramp:
                AdvanceRamp(deltaSeconds);
                break;

            case LightBarMode.Sequence:
                AdvanceSequence(deltaSeconds);
                break;
        }
    }

    /// <summary>
    /// Reports <see cref="Current"/> only when it differs from the color reported last, so a caller
    /// writes to a controller on the frames that changed rather than on every frame. The first call
    /// always reports.
    /// </summary>
    public bool TryTakeChangedColor(out Color color)
    {
        if (_hasTaken && _lastTaken.Value == _current.Value)
        {
            color = _current;
            return false;
        }

        _lastTaken = _current;
        _hasTaken = true;
        color = _current;
        return true;
    }

    /// <summary>
    /// Forgets which color was last reported, so the next <see cref="TryTakeChangedColor"/> reports again.
    /// Use it after something else has written the light bar and the animator's record is stale.
    /// </summary>
    public void InvalidateTakenColor() => _hasTaken = false;

    private void AdvanceRamp(float deltaSeconds)
    {
        _elapsed += deltaSeconds;
        float t;
        if (_elapsed >= _duration)
        {
            if (_loop)
            {
                _elapsed %= _duration;
                t = _elapsed / _duration;
            }
            else
            {
                _elapsed = _duration;
                t = 1f;
                _finished = true;
            }
        }
        else
        {
            t = _elapsed / _duration;
        }

        _current = Color.Lerp(_from, _to, t);
    }

    private void AdvanceSequence(float deltaSeconds)
    {
        if (_finished)
            return;

        _elapsed += deltaSeconds;
        while (_elapsed >= _steps[_stepIndex].DurationSeconds)
        {
            LightBarStep done = _steps[_stepIndex];
            if (_stepIndex + 1 >= _steps.Count && !_loop)
            {
                _elapsed = done.DurationSeconds;
                _finished = true;
                _current = done.Color;
                return;
            }

            _elapsed -= done.DurationSeconds;
            _stepStartColor = done.Color;
            _stepIndex = (_stepIndex + 1) % _steps.Count;
        }

        LightBarStep step = _steps[_stepIndex];
        _current = step.Fade
            ? Color.Lerp(_stepStartColor, step.Color, _elapsed / step.DurationSeconds)
            : step.Color;
    }

    private static Color Brightened(Color color, float level) => color.Darken(1f - level);

    private static float Wrap(float phase) => phase - MathF.Floor(phase);
}

/// <summary>
/// The light bar of one controller. Pick a state, then call <see cref="Update"/> once per frame; the
/// color is written to the controller only on the frames it changed, because each write is a request to
/// the controller service and a frame loop must not spend one where nothing moved.
/// </summary>
/// <remarks>
/// Setting a color and restoring the system default are the only light-bar operations an application can
/// reach. Everything else the frame loop needs - a pulse, a blink, a ramp, a queue of steps - is produced
/// here and written as a plain color, which is why <see cref="Update"/> never blocks.
/// </remarks>
public sealed class LightBar
{
    private readonly GamePad _pad;
    private readonly LightBarAnimator _animator = new();
    private bool _followingSystem = true;

    /// <summary>Binds a light bar to <paramref name="pad"/>.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="pad"/> is null.</exception>
    public LightBar(GamePad pad)
    {
        ArgumentNullException.ThrowIfNull(pad);
        _pad = pad;
    }

    /// <summary>The color the light bar should be showing this frame.</summary>
    public Color Current => _animator.Current;

    /// <summary>What the light bar is doing over time.</summary>
    public LightBarMode Mode => _animator.Mode;

    /// <summary>True once a ramp or a sequence has run out, and for a solid color.</summary>
    public bool IsFinished => _animator.IsFinished;

    /// <summary>
    /// True while the controller keeps the color the system chose, which is the state a light bar starts
    /// in and the state <see cref="ResetToSystemDefault"/> returns it to.
    /// </summary>
    public bool IsFollowingSystemDefault => _followingSystem;

    /// <summary>Shows one color that does not change.</summary>
    public void SetColor(Color color)
    {
        _animator.Solid(color);
        Resume();
    }

    /// <summary>Turns the light bar off.</summary>
    /// <remarks>
    /// The controller service refuses a color whose red, green and blue are all below 13 when a module
    /// runs under the earlier-generation compatibility path. A module built with this SDK does not take
    /// that path, so black is accepted and the light bar goes dark.
    /// </remarks>
    public void Off()
    {
        _animator.Off();
        Resume();
    }

    /// <summary>
    /// Restores the color the system chose for the controller and stops animating. Nothing is written
    /// again until another state is set.
    /// </summary>
    /// <returns>False when the controller does not accept the request.</returns>
    public bool ResetToSystemDefault()
    {
        _followingSystem = true;
        _animator.InvalidateTakenColor();
        return _pad.ResetLightBar();
    }

    /// <summary>
    /// Breathes <paramref name="color"/> between <paramref name="minBrightness"/> and
    /// <paramref name="maxBrightness"/> (each 0 to 1) once every <paramref name="periodSeconds"/>.
    /// </summary>
    public void Pulse(Color color, float periodSeconds, float minBrightness = 0f, float maxBrightness = 1f)
    {
        _animator.Pulse(color, periodSeconds, minBrightness, maxBrightness);
        Resume();
    }

    /// <summary>
    /// Switches between <paramref name="onColor"/> and <paramref name="offColor"/> once every
    /// <paramref name="periodSeconds"/>, holding the on color for <paramref name="dutyCycle"/> of it.
    /// </summary>
    public void Blink(Color onColor, Color offColor, float periodSeconds, float dutyCycle = 0.5f)
    {
        _animator.Blink(onColor, offColor, periodSeconds, dutyCycle);
        Resume();
    }

    /// <summary>Blinks <paramref name="color"/> against a dark light bar.</summary>
    public void Blink(Color color, float periodSeconds) => Blink(color, Color.Black, periodSeconds);

    /// <summary>
    /// Travels from <paramref name="from"/> to <paramref name="to"/> across
    /// <paramref name="durationSeconds"/>, restarting when <paramref name="loop"/> is set.
    /// </summary>
    public void Ramp(Color from, Color to, float durationSeconds, bool loop = false)
    {
        _animator.Ramp(from, to, durationSeconds, loop);
        Resume();
    }

    /// <summary>Runs <paramref name="steps"/> in order, restarting when <paramref name="loop"/> is set.</summary>
    public void Sequence(IEnumerable<LightBarStep> steps, bool loop = false)
    {
        _animator.Sequence(steps, loop);
        Resume();
    }

    /// <summary>
    /// Advances the state by the frame's elapsed time and writes the color when it changed. Returns false
    /// when a write was attempted and the controller refused it.
    /// </summary>
    public bool Update(float deltaSeconds)
    {
        if (_followingSystem)
            return true;

        _animator.Update(deltaSeconds);
        return !_animator.TryTakeChangedColor(out Color color) || _pad.SetLightBar(color);
    }

    private void Resume()
    {
        _followingSystem = false;

        // The controller is showing the system color at this point, so the first frame of the new state
        // has to be written even if it matches whatever the animator reported last.
        _animator.InvalidateTakenColor();
    }
}

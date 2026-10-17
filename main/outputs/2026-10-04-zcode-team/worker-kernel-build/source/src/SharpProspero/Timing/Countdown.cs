// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Timing;

/// <summary>
/// A one-shot timer that counts down and fires once — a delayed action, a respawn after a wait, a
/// message that clears itself. Advance it each frame with the time since the last; it returns true on the
/// single frame it reaches zero, then stays elapsed until restarted.
/// </summary>
/// <remarks>Creates a countdown of <paramref name="durationSeconds"/>, already running.</remarks>
/// <exception cref="ArgumentOutOfRangeException"><paramref name="durationSeconds"/> is negative.</exception>
public sealed class Countdown(float durationSeconds)
{
    private float _remaining = durationSeconds >= 0f
        ? durationSeconds
        : throw new ArgumentOutOfRangeException(nameof(durationSeconds));

    /// <summary>The length the countdown restarts to, in seconds.</summary>
    public float Duration { get; private set; } = durationSeconds >= 0f
        ? durationSeconds
        : throw new ArgumentOutOfRangeException(nameof(durationSeconds));

    /// <summary>How long is left, in seconds.</summary>
    public float Remaining => _remaining;

    /// <summary>Whether the countdown is still running.</summary>
    public bool IsRunning => _remaining > 0f;

    /// <summary>Whether the countdown has reached zero.</summary>
    public bool IsElapsed => _remaining <= 0f;

    /// <summary>How far the countdown has run, from 0 (just started) to 1 (elapsed), for a bar.</summary>
    public float Progress => Duration <= 0f ? 1f : 1f - (_remaining / Duration);

    /// <summary>
    /// Advances by <paramref name="deltaSeconds"/> and returns true on the one frame the countdown reaches
    /// zero. Once elapsed it returns false until it is restarted.
    /// </summary>
    public bool Advance(float deltaSeconds)
    {
        if (_remaining <= 0f || !float.IsFinite(deltaSeconds) || deltaSeconds <= 0f)
            return false;
        _remaining -= deltaSeconds;
        if (_remaining <= 0f)
        {
            _remaining = 0f;
            return true;
        }
        return false;
    }

    /// <summary>Restarts the countdown, optionally to a new <paramref name="durationSeconds"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="durationSeconds"/> is negative.</exception>
    public void Restart(float? durationSeconds = null)
    {
        if (durationSeconds is float duration)
        {
            ArgumentOutOfRangeException.ThrowIfNegative(duration);
            Duration = duration;
        }
        _remaining = Duration;
    }
}

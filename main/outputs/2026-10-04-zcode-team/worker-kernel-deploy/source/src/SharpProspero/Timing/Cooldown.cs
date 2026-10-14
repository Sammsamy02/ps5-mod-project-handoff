// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Timing;

/// <summary>
/// A gate that is ready, then not ready for a set time after it is used — a weapon that cannot fire
/// again at once, an ability on a recharge, a button that ignores a second press. Advance it each frame
/// with the time since the last, and try to trigger it: it fires only when ready, then goes cold for its
/// duration.
/// </summary>
/// <remarks>Creates a gate that stays cold for <paramref name="durationSeconds"/> after each use, starting ready.</remarks>
/// <exception cref="ArgumentOutOfRangeException"><paramref name="durationSeconds"/> is negative.</exception>
public sealed class Cooldown(float durationSeconds)
{
    private float _remaining;
    private float _duration = durationSeconds >= 0f
        ? durationSeconds
        : throw new ArgumentOutOfRangeException(nameof(durationSeconds));

    /// <summary>How long the gate stays cold after each use, in seconds. Must not be negative.</summary>
    /// <exception cref="ArgumentOutOfRangeException">The value is negative.</exception>
    public float Duration
    {
        get => _duration;
        set
        {
            ArgumentOutOfRangeException.ThrowIfNegative(value);
            _duration = value;
        }
    }

    /// <summary>How long until the gate is ready again, in seconds.</summary>
    public float Remaining => _remaining;

    /// <summary>Whether the gate is ready to use.</summary>
    public bool IsReady => _remaining <= 0f;

    /// <summary>How much of the cooldown is left, from 1 (just used) to 0 (ready), for a recharge meter.</summary>
    public float Fraction => Duration <= 0f ? 0f : _remaining / Duration;

    /// <summary>Advances by <paramref name="deltaSeconds"/>, cooling the gate down.</summary>
    public void Advance(float deltaSeconds)
    {
        if (!float.IsFinite(deltaSeconds) || deltaSeconds <= 0f || _remaining <= 0f)
            return;
        _remaining -= deltaSeconds;
        if (_remaining < 0f)
            _remaining = 0f;
    }

    /// <summary>Uses the gate if it is ready, starting its cooldown.</summary>
    /// <returns>True when the gate was ready and has now started cooling down.</returns>
    public bool TryUse()
    {
        if (_remaining > 0f)
            return false;
        _remaining = Duration;
        return true;
    }

    /// <summary>Starts the cooldown whether or not the gate was ready.</summary>
    public void Start() => _remaining = Duration;

    /// <summary>Makes the gate ready at once.</summary>
    public void Reset() => _remaining = 0f;
}

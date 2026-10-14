// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Animation;

/// <summary>How a value's progress is shaped over time: a straight line, or a curve that starts or
/// ends gently.</summary>
public enum Ease
{
    /// <summary>A straight line: constant speed.</summary>
    Linear,

    /// <summary>Starts slow, speeds up (squared).</summary>
    InQuad,

    /// <summary>Starts fast, slows down (squared).</summary>
    OutQuad,

    /// <summary>Slow at both ends, fast in the middle (squared).</summary>
    InOutQuad,

    /// <summary>Starts slow, speeds up (cubed) — a stronger ease than the squared form.</summary>
    InCubic,

    /// <summary>Starts fast, slows down (cubed).</summary>
    OutCubic,

    /// <summary>Slow at both ends, fast in the middle (cubed).</summary>
    InOutCubic,

    /// <summary>Starts very gently (sine).</summary>
    InSine,

    /// <summary>Ends very gently (sine).</summary>
    OutSine,

    /// <summary>Gentle at both ends (sine).</summary>
    InOutSine,

    /// <summary>Overshoots past the end and settles back, for a springy finish.</summary>
    OutBack,

    /// <summary>Lands and bounces a few times before settling, like a dropped ball.</summary>
    OutBounce,
}

/// <summary>
/// Shapes a progress value between 0 and 1 by a chosen curve. A motion reads its position at time
/// <c>t</c> as <c>Easing.Apply(ease, t)</c>, so 0 is the start, 1 is the end, and the curve decides how
/// it moves between. <see cref="Tween"/> uses these to animate a value over a duration.
/// </summary>
public static class Easing
{
    private const float BackC1 = 1.70158f;
    private const float BackC3 = BackC1 + 1f;
    private const float BounceN1 = 7.5625f;
    private const float BounceD1 = 2.75f;

    /// <summary>Applies <paramref name="ease"/> to <paramref name="t"/>, which is clamped to 0..1.</summary>
    public static float Apply(Ease ease, float t)
    {
        t = Math.Clamp(t, 0f, 1f);
        return ease switch
        {
            Ease.Linear => t,
            Ease.InQuad => t * t,
            Ease.OutQuad => t * (2f - t),
            Ease.InOutQuad => t < 0.5f ? 2f * t * t : 1f - (Sq(-2f * t + 2f) / 2f),
            Ease.InCubic => t * t * t,
            Ease.OutCubic => 1f - Cube(1f - t),
            Ease.InOutCubic => t < 0.5f ? 4f * t * t * t : 1f - (Cube(-2f * t + 2f) / 2f),
            Ease.InSine => 1f - MathF.Cos(t * MathF.PI / 2f),
            Ease.OutSine => MathF.Sin(t * MathF.PI / 2f),
            Ease.InOutSine => -(MathF.Cos(MathF.PI * t) - 1f) / 2f,
            Ease.OutBack => 1f + (BackC3 * Cube(t - 1f)) + (BackC1 * Sq(t - 1f)),
            Ease.OutBounce => OutBounce(t),
            _ => t,
        };
    }

    /// <summary>The eased fraction from <paramref name="from"/> to <paramref name="to"/> at time <paramref name="t"/>.</summary>
    public static float Interpolate(float from, float to, float t, Ease ease = Ease.Linear)
        => from + ((to - from) * Apply(ease, t));

    private static float Sq(float x) => x * x;

    private static float Cube(float x) => x * x * x;

    private static float OutBounce(float t)
    {
        if (t < 1f / BounceD1)
            return BounceN1 * t * t;
        if (t < 2f / BounceD1)
        {
            t -= 1.5f / BounceD1;
            return (BounceN1 * t * t) + 0.75f;
        }
        if (t < 2.5f / BounceD1)
        {
            t -= 2.25f / BounceD1;
            return (BounceN1 * t * t) + 0.9375f;
        }
        t -= 2.625f / BounceD1;
        return (BounceN1 * t * t) + 0.984375f;
    }
}

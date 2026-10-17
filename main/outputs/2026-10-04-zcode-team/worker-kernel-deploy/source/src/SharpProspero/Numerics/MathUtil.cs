// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Numerics;

/// <summary>
/// The small floating-point helpers game and drawing code reaches for: blending between values, mapping
/// a value from one range to another, easing an edge, and stepping toward a target. They complement
/// <see cref="Vector2"/> and the easing curves, and keep the arithmetic readable.
/// </summary>
public static class MathUtil
{
    /// <summary>Half a turn, in radians.</summary>
    public const float Pi = MathF.PI;

    /// <summary>A whole turn, in radians.</summary>
    public const float TwoPi = MathF.Tau;

    /// <summary>Multiply radians by this to get degrees.</summary>
    public const float DegreesPerRadian = 180f / MathF.PI;

    /// <summary>Multiply degrees by this to get radians.</summary>
    public const float RadiansPerDegree = MathF.PI / 180f;

    /// <summary>Holds <paramref name="value"/> to the range 0 to 1.</summary>
    public static float Clamp01(float value) => value < 0f ? 0f : (value > 1f ? 1f : value);

    /// <summary>Holds <paramref name="value"/> to the range <paramref name="min"/> to <paramref name="max"/>.</summary>
    public static float Clamp(float value, float min, float max) => value < min ? min : (value > max ? max : value);

    /// <summary>Blends from <paramref name="a"/> to <paramref name="b"/> by <paramref name="t"/> (not clamped).</summary>
    public static float Lerp(float a, float b, float t) => a + ((b - a) * t);

    /// <summary>Blends from <paramref name="a"/> to <paramref name="b"/> by <paramref name="t"/>, held to 0..1.</summary>
    public static float LerpClamped(float a, float b, float t) => a + ((b - a) * Clamp01(t));

    /// <summary>Where <paramref name="value"/> falls between <paramref name="a"/> and <paramref name="b"/>, as 0..1.</summary>
    public static float InverseLerp(float a, float b, float value) => a == b ? 0f : Clamp01((value - a) / (b - a));

    /// <summary>Maps <paramref name="value"/> from the range in to the range out.</summary>
    public static float Remap(float value, float inMin, float inMax, float outMin, float outMax)
        => Lerp(outMin, outMax, InverseLerp(inMin, inMax, value));

    /// <summary>Blends from <paramref name="a"/> to <paramref name="b"/> along an eased S-curve by <paramref name="t"/>.</summary>
    public static float SmoothStep(float a, float b, float t)
    {
        t = Clamp01(t);
        return Lerp(a, b, t * t * (3f - (2f * t)));
    }

    /// <summary>Moves <paramref name="current"/> toward <paramref name="target"/> by at most <paramref name="maxDelta"/>.</summary>
    public static float MoveTowards(float current, float target, float maxDelta)
    {
        if (MathF.Abs(target - current) <= maxDelta)
            return target;
        return current + (MathF.Sign(target - current) * maxDelta);
    }

    /// <summary>Whether two values are within <paramref name="epsilon"/> of each other.</summary>
    public static bool Approximately(float a, float b, float epsilon = 1e-6f) => MathF.Abs(a - b) <= epsilon;

    /// <summary>Wraps <paramref name="t"/> into the range 0 (inclusive) to <paramref name="length"/> (exclusive).</summary>
    public static float Repeat(float t, float length)
        => length <= 0f ? 0f : t - (MathF.Floor(t / length) * length);

    /// <summary>Bounces <paramref name="t"/> back and forth between 0 and <paramref name="length"/>.</summary>
    public static float PingPong(float t, float length)
    {
        if (length <= 0f)
            return 0f;
        float wrapped = Repeat(t, length * 2f);
        return length - MathF.Abs(wrapped - length);
    }

    /// <summary>Converts degrees to radians.</summary>
    public static float DegreesToRadians(float degrees) => degrees * RadiansPerDegree;

    /// <summary>Converts radians to degrees.</summary>
    public static float RadiansToDegrees(float radians) => radians * DegreesPerRadian;

    /// <summary>Wraps an angle into the range -pi (inclusive) to pi (exclusive).</summary>
    public static float WrapAngle(float radians) => Repeat(radians + Pi, TwoPi) - Pi;

    /// <summary>
    /// Blends between two angles in radians along the shortest way round, so a turn from just under a
    /// whole turn to just over zero moves a hair forward rather than most of the way back.
    /// </summary>
    public static float LerpAngle(float a, float b, float t) => a + (WrapAngle(b - a) * t);

    /// <summary>
    /// Eases <paramref name="current"/> toward <paramref name="target"/> like a smooth camera or a value
    /// that settles without overshooting. <paramref name="velocity"/> carries the motion between calls
    /// and must be the same variable each frame; <paramref name="smoothTime"/> is roughly how long the
    /// move takes in seconds, and <paramref name="maxSpeed"/> caps how fast it may travel.
    /// </summary>
    public static float SmoothDamp(float current, float target, ref float velocity, float smoothTime,
        float deltaTime, float maxSpeed = float.PositiveInfinity)
    {
        smoothTime = MathF.Max(0.0001f, smoothTime);
        float omega = 2f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1f / (1f + x + (0.48f * x * x) + (0.235f * x * x * x));

        float change = current - target;
        float originalTarget = target;
        float maxChange = maxSpeed * smoothTime;
        change = Clamp(change, -maxChange, maxChange);
        target = current - change;

        float temp = (velocity + (omega * change)) * deltaTime;
        velocity = (velocity - (omega * temp)) * exp;
        float output = target + ((change + temp) * exp);

        // Stop an overshoot from carrying the value past the target and oscillating.
        if ((originalTarget - current > 0f) == (output > originalTarget))
        {
            output = originalTarget;
            // The value has been pinned to the target, so it is no longer moving. Deriving that from
            // the difference divides a zero that is bit-exactly zero by the step, which is right until
            // the step is zero - and then it writes a number that is not a number back through the
            // caller's own speed, where it stays and poisons every later call.
            velocity = 0f;
        }
        return output;
    }

    /// <summary>Eases an angle in radians toward a target the short way round; see <see cref="SmoothDamp"/>.</summary>
    public static float SmoothDampAngle(float current, float target, ref float velocity, float smoothTime,
        float deltaTime, float maxSpeed = float.PositiveInfinity)
        => SmoothDamp(current, current + WrapAngle(target - current), ref velocity, smoothTime, deltaTime, maxSpeed);
}

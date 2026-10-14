// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Numerics;
using System;
using System.Collections.Generic;

namespace SharpProspero.Animation;

/// <summary>
/// Smooth 2D paths for motion that a straight A-to-B tween cannot express: a camera or enemy that follows
/// a curve, a Ken-Burns pan across a photo, a projectile arc. It evaluates quadratic and cubic Bezier
/// curves from control points, and a Catmull-Rom spline that passes through a list of waypoints.
/// </summary>
public static class Spline
{
    /// <summary>The point on a quadratic Bezier curve at <paramref name="t"/> (clamped to 0..1).</summary>
    public static Vector2 Bezier(Vector2 p0, Vector2 p1, Vector2 p2, float t)
    {
        t = Clamp01(t);
        float u = 1f - t;
        return (u * u * p0) + (2f * u * t * p1) + (t * t * p2);
    }

    /// <summary>The point on a cubic Bezier curve at <paramref name="t"/> (clamped to 0..1).</summary>
    public static Vector2 Bezier(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t)
    {
        t = Clamp01(t);
        float u = 1f - t;
        float uu = u * u;
        float tt = t * t;
        return (uu * u * p0) + (3f * uu * t * p1) + (3f * u * tt * p2) + (tt * t * p3);
    }

    /// <summary>
    /// The point on a Catmull-Rom spline that runs through <paramref name="points"/>, at <paramref name="t"/>
    /// from 0 (the first point) to 1 (the last). The curve passes through every point, with the ends
    /// clamped so it does not overshoot past the first and last.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="points"/> is null.</exception>
    /// <exception cref="ArgumentException"><paramref name="points"/> is empty.</exception>
    public static Vector2 CatmullRom(IReadOnlyList<Vector2> points, float t)
    {
        ArgumentNullException.ThrowIfNull(points);
        if (points.Count == 0)
            throw new ArgumentException("A spline needs at least one point.", nameof(points));
        if (points.Count == 1)
            return points[0];

        t = Clamp01(t);
        int segments = points.Count - 1;
        float scaled = t * segments;
        int segment = Math.Min((int)scaled, segments - 1);
        float local = scaled - segment;

        Vector2 p0 = points[Math.Max(segment - 1, 0)];
        Vector2 p1 = points[segment];
        Vector2 p2 = points[segment + 1];
        Vector2 p3 = points[Math.Min(segment + 2, points.Count - 1)];
        return CatmullRomSegment(p0, p1, p2, p3, local);
    }

    /// <summary>
    /// The point on one Catmull-Rom segment from <paramref name="p1"/> to <paramref name="p2"/>, with
    /// <paramref name="p0"/> and <paramref name="p3"/> shaping the tangents, at <paramref name="t"/> in 0..1.
    /// </summary>
    public static Vector2 CatmullRomSegment(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t)
    {
        t = Clamp01(t);
        float t2 = t * t;
        float t3 = t2 * t;

        // The uniform Catmull-Rom basis (tension 0.5).
        return 0.5f * (
            (2f * p1)
            + ((p2 - p0) * t)
            + (((2f * p0) - (5f * p1) + (4f * p2) - p3) * t2)
            + ((-p0 + (3f * p1) - (3f * p2) + p3) * t3));
    }

    // Written so that NaN (both comparisons false on the t > 0f branch) maps to 0, keeping the result on
    // the curve rather than propagating a NaN point.
    private static float Clamp01(float t) => t > 0f ? (t < 1f ? t : 1f) : 0f;
}

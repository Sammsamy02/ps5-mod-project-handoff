// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Numerics;

/// <summary>
/// The overlap tests game code reaches for, over <see cref="Vector2"/> and <see cref="RectF"/>: two
/// circles, a circle and a rectangle, a point in a circle, and where two line segments cross (for a
/// line-of-sight ray or a laser against a wall). Rectangle-against-rectangle and point-in-rectangle live
/// on <see cref="RectF"/> itself.
/// </summary>
public static class Collision
{
    /// <summary>Whether a point is within <paramref name="radius"/> of <paramref name="center"/>.</summary>
    public static bool PointInCircle(Vector2 point, Vector2 center, float radius)
        => Vector2.DistanceSquared(point, center) <= radius * radius;

    /// <summary>Whether two circles overlap or touch.</summary>
    public static bool CirclesOverlap(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB)
    {
        float reach = radiusA + radiusB;
        return Vector2.DistanceSquared(centerA, centerB) <= reach * reach;
    }

    /// <summary>Whether a circle overlaps or touches <paramref name="rect"/>.</summary>
    public static bool CircleOverlapsRect(Vector2 center, float radius, RectF rect)
    {
        // The nearest point of the rectangle to the circle's centre is within the radius exactly when
        // they meet.
        Vector2 nearest = rect.Clamp(center);
        return Vector2.DistanceSquared(center, nearest) <= radius * radius;
    }

    /// <summary>Whether the segments <paramref name="a1"/>-<paramref name="a2"/> and <paramref name="b1"/>-<paramref name="b2"/> cross.</summary>
    public static bool SegmentsIntersect(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2)
        => SegmentIntersection(a1, a2, b1, b2, out _);

    /// <summary>
    /// Finds where the segments <paramref name="a1"/>-<paramref name="a2"/> and <paramref name="b1"/>-
    /// <paramref name="b2"/> cross. Returns true and sets <paramref name="point"/> to the crossing when
    /// they meet at a single point; returns false when they miss or run parallel (a collinear overlap is
    /// reported as no single crossing).
    /// </summary>
    public static bool SegmentIntersection(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2, out Vector2 point)
    {
        point = default;
        Vector2 r = a2 - a1;
        Vector2 s = b2 - b1;
        float denominator = Vector2.Cross(r, s);
        if (denominator == 0f)
            return false; // parallel or collinear: no single crossing point

        Vector2 delta = b1 - a1;
        float t = Vector2.Cross(delta, s) / denominator;
        float u = Vector2.Cross(delta, r) / denominator;
        if (t < 0f || t > 1f || u < 0f || u > 1f)
            return false;

        point = a1 + (r * t);
        return true;
    }

    /// <summary>Whether the segment <paramref name="a"/>-<paramref name="b"/> touches or enters <paramref name="rect"/>.</summary>
    public static bool SegmentIntersectsRect(Vector2 a, Vector2 b, RectF rect)
    {
        // Either end inside the rectangle, or the segment crosses one of its four edges.
        if (rect.Contains(a) || rect.Contains(b))
            return true;
        var topLeft = new Vector2(rect.X, rect.Y);
        var topRight = new Vector2(rect.X + rect.Width, rect.Y);
        var bottomLeft = new Vector2(rect.X, rect.Y + rect.Height);
        var bottomRight = new Vector2(rect.X + rect.Width, rect.Y + rect.Height);
        return SegmentsIntersect(a, b, topLeft, topRight)
            || SegmentsIntersect(a, b, topRight, bottomRight)
            || SegmentsIntersect(a, b, bottomRight, bottomLeft)
            || SegmentsIntersect(a, b, bottomLeft, topLeft);
    }
}

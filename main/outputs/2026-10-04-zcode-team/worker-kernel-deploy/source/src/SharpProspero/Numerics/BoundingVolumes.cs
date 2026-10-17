using System;
using System.Numerics;

namespace SharpProspero.Numerics;

/// <summary>
/// An axis-aligned bounding box: the smallest box, lined up with the world axes, that contains a set of
/// points. Used for broad-phase culling and picking, where a cheap box test rejects most of the scene
/// before an exact test runs.
/// </summary>
public readonly struct BoundingBox(Vector3 min, Vector3 max)
{
    /// <summary>The corner with the smallest coordinates.</summary>
    public readonly Vector3 Min = min;

    /// <summary>The corner with the largest coordinates.</summary>
    public readonly Vector3 Max = max;

    /// <summary>The point at the middle of the box.</summary>
    public Vector3 Center => (Min + Max) * 0.5f;

    /// <summary>The full width, height, and depth.</summary>
    public Vector3 Size => Max - Min;

    /// <summary>Half the size, measured from the center.</summary>
    public Vector3 Extents => Size * 0.5f;

    /// <summary>The tightest box around a set of points.</summary>
    public static BoundingBox FromPoints(ReadOnlySpan<Vector3> points)
    {
        if (points.Length == 0) return new BoundingBox(Vector3.Zero, Vector3.Zero);
        Vector3 min = points[0], max = points[0];
        for (int i = 1; i < points.Length; i++)
        {
            min = Vector3.Min(min, points[i]);
            max = Vector3.Max(max, points[i]);
        }
        return new BoundingBox(min, max);
    }

    /// <summary>True when the point is inside or on the box.</summary>
    public bool Contains(Vector3 p) =>
        p.X >= Min.X && p.X <= Max.X && p.Y >= Min.Y && p.Y <= Max.Y && p.Z >= Min.Z && p.Z <= Max.Z;

    /// <summary>True when the two boxes overlap.</summary>
    public bool Intersects(BoundingBox other) =>
        Min.X <= other.Max.X && Max.X >= other.Min.X &&
        Min.Y <= other.Max.Y && Max.Y >= other.Min.Y &&
        Min.Z <= other.Max.Z && Max.Z >= other.Min.Z;

    /// <summary>The box grown to also contain <paramref name="p"/>.</summary>
    public BoundingBox Encapsulate(Vector3 p) => new(Vector3.Min(Min, p), Vector3.Max(Max, p));

    /// <summary>The axis-aligned box around this box after a transform (grows to stay axis-aligned).</summary>
    public BoundingBox Transform(Matrix4x4 m)
    {
        Span<Vector3> corners = stackalloc Vector3[8];
        GetCorners(corners);
        for (int i = 0; i < 8; i++) corners[i] = Vector3.Transform(corners[i], m);
        return FromPoints(corners);
    }

    /// <summary>Writes the eight corner points into <paramref name="dest"/> (length 8).</summary>
    public void GetCorners(Span<Vector3> dest)
    {
        dest[0] = new Vector3(Min.X, Min.Y, Min.Z);
        dest[1] = new Vector3(Max.X, Min.Y, Min.Z);
        dest[2] = new Vector3(Min.X, Max.Y, Min.Z);
        dest[3] = new Vector3(Max.X, Max.Y, Min.Z);
        dest[4] = new Vector3(Min.X, Min.Y, Max.Z);
        dest[5] = new Vector3(Max.X, Min.Y, Max.Z);
        dest[6] = new Vector3(Min.X, Max.Y, Max.Z);
        dest[7] = new Vector3(Max.X, Max.Y, Max.Z);
    }
}

/// <summary>
/// A bounding sphere: a center and a radius. The cheapest volume to test, and rotation-invariant, so it
/// is the usual choice for per-object frustum culling.
/// </summary>
public readonly struct BoundingSphere(Vector3 center, float radius)
{
    /// <summary>The center point.</summary>
    public readonly Vector3 Center = center;

    /// <summary>The radius.</summary>
    public readonly float Radius = radius;

    /// <summary>A sphere that contains every point, centered on their midpoint.</summary>
    public static BoundingSphere FromPoints(ReadOnlySpan<Vector3> points)
    {
        if (points.Length == 0) return new BoundingSphere(Vector3.Zero, 0f);
        BoundingBox box = BoundingBox.FromPoints(points);
        Vector3 center = box.Center;
        float r2 = 0f;
        foreach (Vector3 p in points) r2 = MathF.Max(r2, (p - center).LengthSquared());
        return new BoundingSphere(center, MathF.Sqrt(r2));
    }

    /// <summary>The sphere around a box.</summary>
    public static BoundingSphere FromBox(BoundingBox box) => new(box.Center, box.Extents.Length());

    /// <summary>True when the point is inside or on the sphere.</summary>
    public bool Contains(Vector3 p) => (p - Center).LengthSquared() <= Radius * Radius;

    /// <summary>True when the two spheres overlap.</summary>
    public bool Intersects(BoundingSphere other)
    {
        float r = Radius + other.Radius;
        return (Center - other.Center).LengthSquared() <= r * r;
    }
}

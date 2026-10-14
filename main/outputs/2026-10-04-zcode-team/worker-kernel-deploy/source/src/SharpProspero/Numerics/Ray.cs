using System;
using System.Numerics;

namespace SharpProspero.Numerics;

/// <summary>
/// A half-line: an origin and a unit direction. Used for picking (turn a screen pixel into a ray with
/// <see cref="Camera3D.ScreenToRay"/>) and line-of-sight tests. Each intersection method returns the
/// distance along the ray to the hit, or a negative value when there is none.
/// </summary>
public readonly struct Ray(Vector3 origin, Vector3 direction)
{
    /// <summary>Where the ray starts.</summary>
    public readonly Vector3 Origin = origin;

    /// <summary>The direction the ray points, expected to be unit length.</summary>
    public readonly Vector3 Direction = direction;

    /// <summary>The point at distance <paramref name="t"/> along the ray.</summary>
    public Vector3 At(float t) => Origin + Direction * t;

    /// <summary>Distance to the plane, or a negative value when the ray is parallel or points away.</summary>
    public float IntersectPlane(Plane plane)
    {
        float denom = Vector3.Dot(plane.Normal, Direction);
        if (MathF.Abs(denom) < 1e-6f) return -1f;
        float t = -(Vector3.Dot(plane.Normal, Origin) + plane.D) / denom;
        return t;
    }

    /// <summary>Distance to the nearest sphere surface hit, or a negative value when the ray misses.</summary>
    public float IntersectSphere(BoundingSphere sphere)
    {
        Vector3 m = Origin - sphere.Center;
        float b = Vector3.Dot(m, Direction);
        float c = m.LengthSquared() - sphere.Radius * sphere.Radius;
        if (c > 0f && b > 0f) return -1f;
        float disc = b * b - c;
        if (disc < 0f) return -1f;
        float t = -b - MathF.Sqrt(disc);
        return t < 0f ? 0f : t;
    }

    /// <summary>Distance to the nearest box face hit, or a negative value when the ray misses.</summary>
    public float IntersectBox(BoundingBox box)
    {
        float tMin = 0f, tMax = float.MaxValue;
        for (int axis = 0; axis < 3; axis++)
        {
            float o = Index(Origin, axis), d = Index(Direction, axis);
            float lo = Index(box.Min, axis), hi = Index(box.Max, axis);
            if (MathF.Abs(d) < 1e-8f)
            {
                if (o < lo || o > hi) return -1f;
            }
            else
            {
                float inv = 1f / d;
                float t1 = (lo - o) * inv, t2 = (hi - o) * inv;
                if (t1 > t2) (t1, t2) = (t2, t1);
                tMin = MathF.Max(tMin, t1);
                tMax = MathF.Min(tMax, t2);
                if (tMin > tMax) return -1f;
            }
        }
        return tMin;
    }

    /// <summary>Distance to the triangle, or a negative value when the ray misses. Front and back faces both hit.</summary>
    public float IntersectTriangle(Vector3 a, Vector3 b, Vector3 c)
    {
        Vector3 e1 = b - a, e2 = c - a;
        Vector3 p = Vector3.Cross(Direction, e2);
        float det = Vector3.Dot(e1, p);
        if (MathF.Abs(det) < 1e-8f) return -1f;
        float inv = 1f / det;
        Vector3 t = Origin - a;
        float u = Vector3.Dot(t, p) * inv;
        if (u < 0f || u > 1f) return -1f;
        Vector3 q = Vector3.Cross(t, e1);
        float v = Vector3.Dot(Direction, q) * inv;
        if (v < 0f || u + v > 1f) return -1f;
        float dist = Vector3.Dot(e2, q) * inv;
        return dist < 0f ? -1f : dist;
    }

    private static float Index(Vector3 v, int axis) => axis == 0 ? v.X : axis == 1 ? v.Y : v.Z;
}

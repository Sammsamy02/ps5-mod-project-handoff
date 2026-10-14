using System.Numerics;

namespace SharpProspero.Numerics;

/// <summary>
/// The six planes of a camera's view volume, pulled from a view-projection matrix. Test an object's
/// bounding sphere or box against it to skip drawing what the camera cannot see, which is the single
/// biggest win for a scene with many objects. Each plane's normal points inward, so a point is inside
/// the frustum when it is on the positive side of all six.
/// </summary>
public readonly struct Frustum
{
    private readonly Plane _left, _right, _bottom, _top, _near, _far;

    /// <summary>Builds the frustum from a view-projection matrix (clip-space depth 0 near, 1 far).</summary>
    public Frustum(Matrix4x4 viewProjection)
    {
        Matrix4x4 m = viewProjection;
        // Columns of the row-vector matrix: a clip component is the dot of the point with a column.
        Vector4 c1 = new(m.M11, m.M21, m.M31, m.M41);
        Vector4 c2 = new(m.M12, m.M22, m.M32, m.M42);
        Vector4 c3 = new(m.M13, m.M23, m.M33, m.M43);
        Vector4 c4 = new(m.M14, m.M24, m.M34, m.M44);
        _left = Normalize(c4 + c1);   // x >= -w
        _right = Normalize(c4 - c1);  // x <=  w
        _bottom = Normalize(c4 + c2); // y >= -w
        _top = Normalize(c4 - c2);    // y <=  w
        _near = Normalize(c3);        // z >=  0
        _far = Normalize(c4 - c3);    // z <=  w
    }

    private static Plane Normalize(Vector4 p)
    {
        float len = new Vector3(p.X, p.Y, p.Z).Length();
        if (len < 1e-8f) len = 1f;
        return new Plane(p.X / len, p.Y / len, p.Z / len, p.W / len);
    }

    /// <summary>True when the point is inside the view volume.</summary>
    public bool Contains(Vector3 point) =>
        Distance(_left, point) >= 0 && Distance(_right, point) >= 0 &&
        Distance(_bottom, point) >= 0 && Distance(_top, point) >= 0 &&
        Distance(_near, point) >= 0 && Distance(_far, point) >= 0;

    /// <summary>True when any part of the sphere is inside the view volume.</summary>
    public bool Intersects(BoundingSphere sphere)
    {
        float r = -sphere.Radius;
        return Distance(_left, sphere.Center) >= r && Distance(_right, sphere.Center) >= r &&
               Distance(_bottom, sphere.Center) >= r && Distance(_top, sphere.Center) >= r &&
               Distance(_near, sphere.Center) >= r && Distance(_far, sphere.Center) >= r;
    }

    /// <summary>True when any part of the box is inside the view volume.</summary>
    public bool Intersects(BoundingBox box)
    {
        return !Outside(_left, box) && !Outside(_right, box) && !Outside(_bottom, box) &&
               !Outside(_top, box) && !Outside(_near, box) && !Outside(_far, box);

        // A box is culled only if it lies entirely on the negative side of one plane. The positive-vertex
        // test picks the box corner furthest along the plane normal; if even that is outside, all are.
        static bool Outside(Plane plane, BoundingBox box)
        {
            Vector3 positive = new(
                plane.Normal.X >= 0 ? box.Max.X : box.Min.X,
                plane.Normal.Y >= 0 ? box.Max.Y : box.Min.Y,
                plane.Normal.Z >= 0 ? box.Max.Z : box.Min.Z);
            return Distance(plane, positive) < 0;
        }
    }

    private static float Distance(Plane plane, Vector3 p) => Vector3.Dot(plane.Normal, p) + plane.D;
}

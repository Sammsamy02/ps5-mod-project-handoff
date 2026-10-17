using System.Numerics;

namespace SharpProspero.Numerics;

/// <summary>
/// A position, rotation, and scale in 3D space, and the world matrix they compose to. Rotation is a
/// quaternion, so it interpolates cleanly and never gimbal-locks. The matrix is scale, then rotation,
/// then translation, which is the order almost every renderer expects.
/// </summary>
/// <remarks>Creates a transform from a position, rotation, and scale.</remarks>
public struct Transform(Vector3 position, Quaternion rotation, Vector3 scale)
{
    /// <summary>The position in world space.</summary>
    public Vector3 Position = position;

    /// <summary>The rotation as a quaternion.</summary>
    public Quaternion Rotation = rotation;

    /// <summary>The scale along each local axis.</summary>
    public Vector3 Scale = scale;

    /// <summary>A transform at the origin, unrotated, at unit scale.</summary>
    public static Transform Identity => new(Vector3.Zero, Quaternion.Identity, Vector3.One);

    /// <summary>Creates a transform at a position, unrotated, at unit scale.</summary>
    public Transform(Vector3 position) : this(position, Quaternion.Identity, Vector3.One) { }

    /// <summary>The world matrix: scale, then rotate, then translate.</summary>
    public readonly Matrix4x4 Matrix =>
        Matrix4x4.CreateScale(Scale) *
        Matrix4x4.CreateFromQuaternion(Rotation) *
        Matrix4x4.CreateTranslation(Position);

    /// <summary>The forward direction (-Z in the local frame), rotated into world space.</summary>
    public readonly Vector3 Forward => Vector3.Transform(-Vector3.UnitZ, Rotation);

    /// <summary>The right direction (+X in the local frame), rotated into world space.</summary>
    public readonly Vector3 Right => Vector3.Transform(Vector3.UnitX, Rotation);

    /// <summary>The up direction (+Y in the local frame), rotated into world space.</summary>
    public readonly Vector3 Up => Vector3.Transform(Vector3.UnitY, Rotation);

    /// <summary>Turns the transform to look from its position toward <paramref name="target"/>.</summary>
    public void LookAt(Vector3 target, Vector3 up)
    {
        Vector3 forward = Vector3.Normalize(target - Position);
        Rotation = LookRotation(forward, up);
    }

    /// <summary>Rotates the transform by <paramref name="degrees"/> around a world axis.</summary>
    public void Rotate(Vector3 axis, float degrees)
    {
        Rotation = Quaternion.Concatenate(Rotation, Quaternion.CreateFromAxisAngle(Vector3.Normalize(axis), MathUtil.DegreesToRadians(degrees)));
    }

    /// <summary>Transforms a local-space point into world space.</summary>
    public readonly Vector3 TransformPoint(Vector3 local) => Vector3.Transform(local, Matrix);

    /// <summary>Rotates a local-space direction into world space (ignores position and scale sign).</summary>
    public readonly Vector3 TransformDirection(Vector3 local) => Vector3.Transform(local, Rotation);

    /// <summary>Builds a rotation that faces <paramref name="forward"/> with the given up hint.</summary>
    public static Quaternion LookRotation(Vector3 forward, Vector3 up)
    {
        forward = Vector3.Normalize(forward);
        Vector3 right = Vector3.Cross(forward, up);
        // Fall back to a stable axis when forward and up are parallel.
        if (right.LengthSquared() < 1e-8f) right = Vector3.Cross(forward, Vector3.UnitX);
        if (right.LengthSquared() < 1e-8f) right = Vector3.Cross(forward, Vector3.UnitZ);
        right = Vector3.Normalize(right);
        Vector3 realUp = Vector3.Cross(right, forward);
        // Basis rows (right, up, -forward) form a proper (right-handed) rotation. Forward is -Z.
        var m = new Matrix4x4(
            right.X, right.Y, right.Z, 0,
            realUp.X, realUp.Y, realUp.Z, 0,
            -forward.X, -forward.Y, -forward.Z, 0,
            0, 0, 0, 1);
        return Quaternion.Normalize(Quaternion.CreateFromRotationMatrix(m));
    }
}

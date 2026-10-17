using System;
using System.Numerics;

namespace SharpProspero.Numerics;

/// <summary>
/// A 3D camera: an eye position and orientation that produce a view matrix, and a lens that produces a
/// projection matrix. The two combine into the view-projection matrix a shader multiplies each vertex
/// by. The projection is right-handed with a clip-space depth range of 0 at the near plane to 1 at the
/// far plane, which is what the display pipeline expects.
/// </summary>
public sealed class Camera3D
{
    /// <summary>The eye position in world space.</summary>
    public Vector3 Position { get; set; } = new(0, 0, 5);

    /// <summary>The point the camera looks at.</summary>
    public Vector3 Target { get; set; } = Vector3.Zero;

    /// <summary>The up hint used to orient the view.</summary>
    public Vector3 Up { get; set; } = Vector3.UnitY;

    /// <summary>The vertical field of view in degrees (perspective only).</summary>
    public float FieldOfView { get; set; } = 60f;

    /// <summary>The width divided by the height of the target.</summary>
    public float AspectRatio { get; set; } = 16f / 9f;

    /// <summary>The near clip distance; must be greater than zero.</summary>
    public float NearPlane { get; set; } = 0.1f;

    /// <summary>The far clip distance.</summary>
    public float FarPlane { get; set; } = 1000f;

    /// <summary>When set, the camera projects orthographically with this vertical size instead of a perspective.</summary>
    public float? OrthographicSize { get; set; }

    /// <summary>Points the camera at a target from an eye position.</summary>
    public void LookAt(Vector3 eye, Vector3 target)
    {
        Position = eye;
        Target = target;
    }

    /// <summary>The view matrix that moves the world into camera space.</summary>
    public Matrix4x4 View => Matrix4x4.CreateLookAt(Position, Target, Up);

    /// <summary>The projection matrix (perspective, or orthographic when <see cref="OrthographicSize"/> is set).</summary>
    public Matrix4x4 Projection => OrthographicSize is float size
        ? Orthographic(size * AspectRatio, size, NearPlane, FarPlane)
        : Perspective(MathUtil.DegreesToRadians(FieldOfView), AspectRatio, NearPlane, FarPlane);

    /// <summary>The combined view-projection matrix a vertex shader multiplies a world position by.</summary>
    public Matrix4x4 ViewProjection => View * Projection;

    /// <summary>The direction the camera faces.</summary>
    public Vector3 Forward => Vector3.Normalize(Target - Position);

    /// <summary>
    /// A right-handed perspective projection with a clip-space depth of 0 at the near plane and 1 at the
    /// far plane.
    /// </summary>
    public static Matrix4x4 Perspective(float fovYRadians, float aspect, float near, float far)
    {
        float f = 1f / MathF.Tan(fovYRadians * 0.5f);
        return new Matrix4x4(
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, far / (near - far), -1,
            0, 0, near * far / (near - far), 0);
    }

    /// <summary>
    /// A right-handed orthographic projection with a clip-space depth of 0 at the near plane and 1 at the
    /// far plane.
    /// </summary>
    public static Matrix4x4 Orthographic(float width, float height, float near, float far)
    {
        return new Matrix4x4(
            2f / width, 0, 0, 0,
            0, 2f / height, 0, 0,
            0, 0, 1f / (near - far), 0,
            0, 0, near / (near - far), 1);
    }

    /// <summary>
    /// Projects a world-space point to screen pixels for a target of the given size. The returned Z is the
    /// clip-space depth (0 at the near plane, 1 at the far); a point behind the camera returns W &lt;= 0 in
    /// <paramref name="visible"/>.
    /// </summary>
    public Vector3 WorldToScreen(Vector3 world, int screenWidth, int screenHeight, out bool visible)
    {
        Vector4 clip = Vector4.Transform(new Vector4(world, 1f), ViewProjection);
        visible = clip.W > 1e-6f;
        float inv = visible ? 1f / clip.W : 1f;
        float ndcX = clip.X * inv, ndcY = clip.Y * inv;
        return new Vector3(
            (ndcX * 0.5f + 0.5f) * screenWidth,
            (1f - (ndcY * 0.5f + 0.5f)) * screenHeight,
            clip.Z * inv);
    }

    /// <summary>
    /// Builds a world-space ray through a screen pixel, for picking. The ray starts on the near plane and
    /// points into the scene.
    /// </summary>
    public Ray ScreenToRay(float screenX, float screenY, int screenWidth, int screenHeight)
    {
        float ndcX = screenX / screenWidth * 2f - 1f;
        float ndcY = 1f - screenY / screenHeight * 2f;
        Matrix4x4.Invert(ViewProjection, out Matrix4x4 inv);
        Vector4 nearP = Vector4.Transform(new Vector4(ndcX, ndcY, 0f, 1f), inv);
        Vector4 farP = Vector4.Transform(new Vector4(ndcX, ndcY, 1f, 1f), inv);
        Vector3 origin = new Vector3(nearP.X, nearP.Y, nearP.Z) / nearP.W;
        Vector3 end = new Vector3(farP.X, farP.Y, farP.Z) / farP.W;
        return new Ray(origin, Vector3.Normalize(end - origin));
    }
}

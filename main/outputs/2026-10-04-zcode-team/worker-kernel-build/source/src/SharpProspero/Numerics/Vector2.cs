// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Numerics;

/// <summary>
/// A two-dimensional vector of single-precision numbers, for a position, a velocity or a direction in
/// game and drawing code. It is a small value type with the usual operators and helpers, so movement and
/// steering read as arithmetic rather than a pair of loose floats.
/// </summary>
/// <remarks>Creates a vector with the given components.</remarks>
public readonly struct Vector2(float x, float y) : IEquatable<Vector2>
{
    /// <summary>The horizontal component.</summary>
    public float X { get; } = x;

    /// <summary>The vertical component.</summary>
    public float Y { get; } = y;

    /// <summary>The zero vector.</summary>
    public static Vector2 Zero => new(0f, 0f);

    /// <summary>The vector whose components are both one.</summary>
    public static Vector2 One => new(1f, 1f);

    /// <summary>The unit vector along the x-axis.</summary>
    public static Vector2 UnitX => new(1f, 0f);

    /// <summary>The unit vector along the y-axis.</summary>
    public static Vector2 UnitY => new(0f, 1f);

    /// <summary>The squared length, cheaper than <see cref="Length"/> when only comparing distances.</summary>
    public float LengthSquared => (X * X) + (Y * Y);

    /// <summary>The length (magnitude) of the vector.</summary>
    public float Length => MathF.Sqrt(LengthSquared);

    /// <summary>A vector in the same direction with length one, or zero when this vector is (near) zero.</summary>
    public Vector2 Normalized()
    {
        float length = Length;
        return length <= 1e-6f ? Zero : new Vector2(X / length, Y / length);
    }

    /// <summary>This vector with a different x component.</summary>
    public Vector2 WithX(float value) => new(value, Y);

    /// <summary>This vector with a different y component.</summary>
    public Vector2 WithY(float value) => new(X, value);

    /// <summary>Turns the vector by <paramref name="radians"/> (clockwise on a screen whose y grows downward).</summary>
    public Vector2 Rotate(float radians)
    {
        float cos = MathF.Cos(radians), sin = MathF.Sin(radians);
        return new Vector2((X * cos) - (Y * sin), (X * sin) + (Y * cos));
    }

    /// <summary>Adds two vectors component-wise.</summary>
    public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.X + b.X, a.Y + b.Y);

    /// <summary>Subtracts <paramref name="b"/> from <paramref name="a"/> component-wise.</summary>
    public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.X - b.X, a.Y - b.Y);

    /// <summary>Negates the vector.</summary>
    public static Vector2 operator -(Vector2 v) => new(-v.X, -v.Y);

    /// <summary>Scales the vector by <paramref name="scalar"/>.</summary>
    public static Vector2 operator *(Vector2 v, float scalar) => new(v.X * scalar, v.Y * scalar);

    /// <summary>Scales the vector by <paramref name="scalar"/>.</summary>
    public static Vector2 operator *(float scalar, Vector2 v) => v * scalar;

    /// <summary>Divides the vector by <paramref name="scalar"/>.</summary>
    public static Vector2 operator /(Vector2 v, float scalar) => new(v.X / scalar, v.Y / scalar);

    /// <summary>The dot product of two vectors.</summary>
    public static float Dot(Vector2 a, Vector2 b) => (a.X * b.X) + (a.Y * b.Y);

    /// <summary>The distance between two points.</summary>
    public static float Distance(Vector2 a, Vector2 b) => (a - b).Length;

    /// <summary>The squared distance between two points, cheaper than <see cref="Distance"/> for comparisons.</summary>
    public static float DistanceSquared(Vector2 a, Vector2 b) => (a - b).LengthSquared;

    /// <summary>A point <paramref name="t"/> of the way from <paramref name="a"/> to <paramref name="b"/> (not clamped).</summary>
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t)
        => new(a.X + ((b.X - a.X) * t), a.Y + ((b.Y - a.Y) * t));

    /// <summary>
    /// The two-dimensional cross product (perpendicular dot product), <c>a.X * b.Y - a.Y * b.X</c>. Its
    /// sign tells which way <paramref name="b"/> turns from <paramref name="a"/> and its magnitude is the
    /// area of the parallelogram they span, useful for winding and turn-direction tests.
    /// </summary>
    public static float Cross(Vector2 a, Vector2 b) => (a.X * b.Y) - (a.Y * b.X);

    /// <summary>The vector turned a quarter turn (its perpendicular), <c>(-Y, X)</c>.</summary>
    public Vector2 Perpendicular() => new(-Y, X);

    /// <summary>Moves <paramref name="current"/> toward <paramref name="target"/> by at most <paramref name="maxDelta"/>, stopping exactly on it.</summary>
    public static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDelta)
    {
        Vector2 delta = target - current;
        float distance = delta.Length;
        if (distance <= maxDelta || distance <= 1e-6f)
            return target;
        return current + (delta / distance * maxDelta);
    }

    /// <summary>This vector shortened to <paramref name="maxLength"/> when it is longer, and unchanged otherwise.</summary>
    public Vector2 ClampLength(float maxLength)
    {
        float lengthSquared = LengthSquared;
        if (lengthSquared <= maxLength * maxLength || lengthSquared <= 1e-12f)
            return this;
        float scale = maxLength / MathF.Sqrt(lengthSquared);
        return new Vector2(X * scale, Y * scale);
    }

    /// <summary>
    /// Eases <paramref name="current"/> toward <paramref name="target"/> like a smooth camera follow that
    /// settles without overshooting. <paramref name="velocity"/> carries the motion between calls and must
    /// be the same variable each frame; <paramref name="smoothTime"/> is roughly how long the move takes
    /// in seconds, and <paramref name="maxSpeed"/> caps how fast it may travel.
    /// </summary>
    public static Vector2 SmoothDamp(Vector2 current, Vector2 target, ref Vector2 velocity, float smoothTime,
        float deltaTime, float maxSpeed = float.PositiveInfinity)
    {
        float vx = velocity.X, vy = velocity.Y;
        float x = MathUtil.SmoothDamp(current.X, target.X, ref vx, smoothTime, deltaTime, maxSpeed);
        float y = MathUtil.SmoothDamp(current.Y, target.Y, ref vy, smoothTime, deltaTime, maxSpeed);
        velocity = new Vector2(vx, vy);
        return new Vector2(x, y);
    }

    /// <summary>A vector pointing at <paramref name="radians"/> (measured from the x-axis) with the given length.</summary>
    public static Vector2 FromAngle(float radians, float length = 1f)
        => new(MathF.Cos(radians) * length, MathF.Sin(radians) * length);

    /// <summary>The direction of the vector as an angle from the x-axis, in radians (-pi to pi).</summary>
    public float ToAngle() => MathF.Atan2(Y, X);

    /// <inheritdoc/>
    public bool Equals(Vector2 other) => X == other.X && Y == other.Y;

    /// <inheritdoc/>
    public override bool Equals(object? obj) => obj is Vector2 other && Equals(other);

    /// <inheritdoc/>
    public override int GetHashCode() => HashCode.Combine(X, Y);

    /// <summary>Whether two vectors have equal components.</summary>
    public static bool operator ==(Vector2 a, Vector2 b) => a.Equals(b);

    /// <summary>Whether two vectors differ in either component.</summary>
    public static bool operator !=(Vector2 a, Vector2 b) => !a.Equals(b);

    /// <inheritdoc/>
    public override string ToString() => $"({X}, {Y})";
}
